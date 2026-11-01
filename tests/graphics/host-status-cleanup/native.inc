/* Include after ordinary coordinator/host_status.inc and packet begin/end.
 * All shared fixture state uses remote_lock. No fixture pointer is sent to PE.
 * The independent host/producer pins are lifetime observers, not resource or
 * status_controls guards. They cannot repair a lost ordinary cell owner. */
#include "fixture.h"
static struct
{
    struct remote_host *host;
    struct remote_producer *producer;
    HWND root;
    UINT context_id, writer_handle, event_handle;
    UINT64 epoch, lease, ticket, generation, sequence, cell_token;
    UINT64 birth, root_birth, hold_start, hold_frequency, serial;
    BOOL bound, finished, armed, held, hold_done, hold_timely, control_started, control_returned;
    NTSTATUS wait_status;
    DWORD control_error;
    UINT32 close_seen, close_attempts, close_status, close_partial, close_successes;
    UINT32 last_close_status, last_close_partial;
    BOOL decision_seen;
    UINT32 d_controls, d_resource_done, d_busy, d_owns, d_attempted, d_closed, d_removed;
    UINT32 d_cell, d_matches, d_registered, d_owners, d_registry_count, d_queued;
} host_cleanup_native;

static UINT32 cleanup_count64(UINT64 value)
{ return value > ~(UINT32)0 ? ~(UINT32)0 : (UINT32)value; }
static void cleanup_increment(UINT32 *value)
{ if (*value != ~(UINT32)0) ++*value; }

/* Called ONLY after an actual macdrv_status_cell_close invocation, outside
 * remote_lock. Its FIFO owner keeps host alive; cell is only compared, not
 * dereferenced or rescued by this observer. */
static void host_cleanup_fixture_after_close(struct remote_host *host, NTSTATUS closed,
                                              struct macdrv_status_cell *remaining)
{
    pthread_mutex_lock(&remote_lock);
    if (host == host_cleanup_native.host)
    {
        cleanup_increment(&host_cleanup_native.close_attempts);
        host_cleanup_native.last_close_status = closed;
        host_cleanup_native.last_close_partial = remaining != NULL;
        if (!closed && !remaining) cleanup_increment(&host_cleanup_native.close_successes);
        if (!host_cleanup_native.close_seen && closed == STATUS_HANDLE_NOT_CLOSABLE && remaining &&
            (UINT64)(UINT_PTR)remaining == host_cleanup_native.cell_token)
        {
            host_cleanup_native.close_seen = 1;
            host_cleanup_native.close_status = closed;
            host_cleanup_native.close_partial = 1;
        }
    }
    pthread_mutex_unlock(&remote_lock);
}

/* AFTER the ordinary reattach/unregister block, BEFORE status_busy is cleared,
 * under the SAME existing remote_lock. Select actual skipped-detach work while
 * a real admitted CANCEL is held. No NT call, signal, wait, or policy branch. */
static void host_cleanup_fixture_decision_locked(struct remote_host *host, BOOL owns_cell,
                                                 BOOL removed, BOOL attempted, NTSTATUS closed)
{
    if (host != host_cleanup_native.host || !host_cleanup_native.held ||
        !host_cleanup_native.close_seen || host_cleanup_native.decision_seen ||
        !host->status_controls || !host->status_resource_done || owns_cell || attempted) return;
    ++host_cleanup_native.serial;
    host_cleanup_native.d_controls = host->status_controls;
    host_cleanup_native.d_resource_done = host->status_resource_done;
    host_cleanup_native.d_busy = host->status_busy;
    host_cleanup_native.d_owns = owns_cell;
    host_cleanup_native.d_attempted = attempted;
    host_cleanup_native.d_closed = closed; /* explicitly a local, not NT success */
    host_cleanup_native.d_removed = removed;
    host_cleanup_native.d_cell = host->status_cell != NULL;
    host_cleanup_native.d_matches = host->status_cell &&
        (UINT64)(UINT_PTR)host->status_cell == host_cleanup_native.cell_token;
    host_cleanup_native.d_registered = host->status_registered;
    host_cleanup_native.d_owners = host->producer->host_status_owners;
    host_cleanup_native.d_registry_count = cleanup_count64(host_status_registered_count);
    host_cleanup_native.d_queued = host->status_queued;
    host_cleanup_native.decision_seen = TRUE;
}

/* In cancel_prepared_host, AFTER begin_host_status_control's lock release and
 * `if (!host) return error;`, BEFORE dispose/decrement. This is a Wine thread.
 * Event is owned by the PE caller until this invocation's kernel thread ends. */
static void host_cleanup_fixture_after_control_begin(struct remote_host *host,
                                                      const struct macdrv_remote_packet *packet)
{
    HANDLE event = NULL;
    LARGE_INTEGER start = {0}, frequency = {0}, end = {0}, timeout;
    NTSTATUS clock_status, wait_status;
    BOOL timely;
    pthread_mutex_lock(&remote_lock);
    if (host == host_cleanup_native.host && host_cleanup_native.armed &&
        packet->sequence == host_cleanup_native.sequence && host->status_controls)
    {
        host_cleanup_native.armed = FALSE;
        event = ULongToHandle(host_cleanup_native.event_handle);
    }
    pthread_mutex_unlock(&remote_lock);
    if (!event) return;
    clock_status = NtQueryPerformanceCounter(&start, &frequency);
    pthread_mutex_lock(&remote_lock);
    host_cleanup_native.hold_start = start.QuadPart;
    host_cleanup_native.hold_frequency = frequency.QuadPart;
    host_cleanup_native.held = TRUE;
    pthread_mutex_unlock(&remote_lock);
    timeout.QuadPart = -40000000; /* actual relative4s NT event wait, outside lock */
    wait_status = NtWaitForSingleObject(event, FALSE, &timeout);
    timely = !NtQueryPerformanceCounter(&end, NULL) && !clock_status && start.QuadPart >= 0 &&
        frequency.QuadPart > 0 && frequency.QuadPart <= 0x7fffffffffffffffLL / 4 &&
        end.QuadPart >= start.QuadPart && end.QuadPart - start.QuadPart < 4 * frequency.QuadPart;
    pthread_mutex_lock(&remote_lock);
    host_cleanup_native.wait_status = wait_status;
    host_cleanup_native.hold_timely = timely;
    host_cleanup_native.held = FALSE;
    host_cleanup_native.hold_done = TRUE;
    pthread_mutex_unlock(&remote_lock);
}

static void host_cleanup_snapshot(struct host_cleanup_fixture *f)
{
    struct remote_host *h;
    struct remote_producer *p, *cursor;
    struct producer_setup *setup;
    pthread_mutex_lock(&remote_lock);
    h = host_cleanup_native.host; p = host_cleanup_native.producer;
    f->flags = (host_cleanup_native.bound ? HOST_CLEANUP_BOUND : 0) |
               (host_cleanup_native.finished ? HOST_CLEANUP_FINISHED : 0);
    f->writer_handle = host_cleanup_native.writer_handle;
    f->root_epoch = host_cleanup_native.epoch; f->lease = host_cleanup_native.lease;
    f->ticket = host_cleanup_native.ticket; f->generation = host_cleanup_native.generation;
    f->control_sequence = host_cleanup_native.sequence; f->producer_birth = host_cleanup_native.birth;
    f->root_birth = host_cleanup_native.root_birth;
    f->event_handle = host_cleanup_native.event_handle;
    f->active = p ? p->active : ~(UINT32)0; f->watched = p ? p->watched : ~(UINT32)0;
    f->finalized = p ? p->finalized : ~(UINT32)0; f->request_inflight = p ? p->request_inflight : ~(UINT32)0;
    f->host_resources = p ? p->host_resources_pending : ~(UINT32)0;
    f->host_owners = p ? p->host_status_owners : ~(UINT32)0;
    f->registered_count = cleanup_count64(host_status_registered_count);
    f->host_registered = h ? h->status_registered : ~(UINT32)0;
    f->cell_present = h ? h->status_cell != NULL : ~(UINT32)0;
    f->cell_matches = h ? (h->status_cell && (UINT64)(UINT_PTR)h->status_cell == host_cleanup_native.cell_token) : ~(UINT32)0;
    f->cell_ready = h ? h->status_ready : ~(UINT32)0; f->status_busy = h ? h->status_busy : ~(UINT32)0;
    f->status_controls = h ? h->status_controls : ~(UINT32)0;
    f->status_resource_done = h ? h->status_resource_done : ~(UINT32)0;
    f->status_terminal = h ? h->status_terminal : ~(UINT32)0; f->status_queued = h ? h->status_queued : ~(UINT32)0;
    f->model_jobs = h ? h->model_jobs : ~(UINT32)0; f->model_possible = h ? h->model_possible : ~(UINT32)0;
    f->retirement_pending = h ? h->retirement_pending : ~(UINT32)0; f->host_state = h ? h->state : ~(UINT32)0;
    f->close_seen = host_cleanup_native.close_seen; f->close_attempts = host_cleanup_native.close_attempts;
    f->close_status = f->close_seen ? host_cleanup_native.close_status : ~(UINT32)0;
    f->close_partial = f->close_seen ? host_cleanup_native.close_partial : ~(UINT32)0;
    f->close_successes = host_cleanup_native.close_successes;
    f->last_close_status = f->close_attempts ? host_cleanup_native.last_close_status : ~(UINT32)0;
    f->last_close_partial = f->close_attempts ? host_cleanup_native.last_close_partial : ~(UINT32)0;
    f->hold_armed = host_cleanup_native.armed; f->hold_active = host_cleanup_native.held;
    f->hold_done = host_cleanup_native.hold_done;
    f->hold_wait_status = f->hold_done ? (UINT32)host_cleanup_native.wait_status : ~(UINT32)0;
    f->hold_timely = f->hold_done ? host_cleanup_native.hold_timely : ~(UINT32)0;
    f->hold_start = host_cleanup_native.hold_start; f->hold_frequency = host_cleanup_native.hold_frequency;
    f->control_returned = host_cleanup_native.control_returned;
    f->control_error = f->control_returned ? host_cleanup_native.control_error : ~(UINT32)0;
    f->decision_seen = host_cleanup_native.decision_seen; f->decision_serial = host_cleanup_native.serial;
    f->decision_controls = host_cleanup_native.d_controls; f->decision_resource_done = host_cleanup_native.d_resource_done;
    f->decision_busy = host_cleanup_native.d_busy; f->decision_owns_cell = host_cleanup_native.d_owns;
    f->decision_attempted = host_cleanup_native.d_attempted; f->decision_local_closed = host_cleanup_native.d_closed;
    f->decision_removed = host_cleanup_native.d_removed; f->decision_cell_present = host_cleanup_native.d_cell;
    f->decision_cell_matches = host_cleanup_native.d_matches; f->decision_registered = host_cleanup_native.d_registered;
    f->decision_host_owners = host_cleanup_native.d_owners; f->decision_registry_count = host_cleanup_native.d_registry_count;
    f->decision_queued = host_cleanup_native.d_queued;
    f->observer_owned = h != NULL;
    f->native_cleanup = f->native_setups = f->native_watches = 0;
    for (cursor = producer_cleanup; cursor; cursor = cursor->cleanup_next) cleanup_increment(&f->native_cleanup);
    for (setup = setup_cleanup; setup; setup = setup->cleanup_next) cleanup_increment(&f->native_cleanup);
    for (setup = producer_setups; setup; setup = setup->next) cleanup_increment(&f->native_setups);
    for (cursor = producer_watches; cursor; cursor = cursor->watch_next) cleanup_increment(&f->native_watches);
    pthread_mutex_unlock(&remote_lock);
}
static DWORD host_cleanup_bind(const struct host_cleanup_fixture *f)
{
    struct remote_host *h;
    struct remote_producer *p;
    UINT_PTR handle;
    DWORD error = ERROR_INVALID_HANDLE;
    pthread_mutex_lock(&remote_lock);
    if (host_cleanup_native.bound) { pthread_mutex_unlock(&remote_lock); return ERROR_ALREADY_EXISTS; }
    h = host_status_lookup_locked(f->ticket); p = h ? h->producer : NULL;
    if (h && p && h->status_registered && p->active && p->protocol == MACDRV_REMOTE_PREPARED_PACKET_VERSION &&
        p->id == f->lease && p->pid == GetCurrentProcessId() && p->generation == f->generation &&
        h->root->epoch == f->root_epoch && wine_server_user_handle(h->root->hwnd) == f->root &&
        h->context_id == f->context_id && h->state == HOST_RESERVED && h->status_ready && h->status_cell &&
        !h->status_preparing && !h->status_controls && !h->model_jobs && !h->model_possible)
    {
        if (h->status_busy) error = ERROR_IO_PENDING;
        else if (!(handle = (UINT_PTR)macdrv_status_cell_handle(h->status_cell)) || handle != (UINT32)handle)
            error = ERROR_INVALID_HANDLE;
        else
        {
            InterlockedIncrement(&h->refs); InterlockedIncrement(&p->refs);
            host_cleanup_native.host = h; host_cleanup_native.producer = p;
            host_cleanup_native.root = h->root->hwnd; host_cleanup_native.context_id = h->context_id;
            host_cleanup_native.writer_handle = handle;
            host_cleanup_native.cell_token = (UINT64)(UINT_PTR)h->status_cell;
            host_cleanup_native.epoch = f->root_epoch; host_cleanup_native.lease = p->id;
            host_cleanup_native.ticket = h->ticket; host_cleanup_native.generation = p->generation;
            host_cleanup_native.birth = p->birth; host_cleanup_native.root_birth = p->status_identity.root_birth;
            host_cleanup_native.bound = TRUE; error = ERROR_SUCCESS;
        }
    }
    pthread_mutex_unlock(&remote_lock);
    return error;
}
static DWORD host_cleanup_arm(const struct host_cleanup_fixture *f)
{
    DWORD error = ERROR_INVALID_STATE;
    pthread_mutex_lock(&remote_lock);
    if (host_cleanup_native.bound && !host_cleanup_native.armed && !host_cleanup_native.control_started &&
        host_cleanup_native.close_seen && host_cleanup_native.host->status_registered &&
        !host_cleanup_native.host->status_controls && f->event_handle &&
        f->control_sequence > host_cleanup_native.producer->last_sequence)
    {
        if (host_cleanup_native.host->status_busy) error = ERROR_IO_PENDING; /* nonacting transient */
        else if (host_cleanup_native.host->status_cell &&
            (UINT64)(UINT_PTR)host_cleanup_native.host->status_cell == host_cleanup_native.cell_token)
        {
            host_cleanup_native.event_handle = f->event_handle;
            host_cleanup_native.sequence = f->control_sequence;
            host_cleanup_native.armed = TRUE; error = ERROR_SUCCESS;
        }
    }
    pthread_mutex_unlock(&remote_lock);
    return error;
}
static DWORD host_cleanup_disarm(void)
{
    pthread_mutex_lock(&remote_lock);
    if (host_cleanup_native.held || (host_cleanup_native.control_started && !host_cleanup_native.control_returned))
    { pthread_mutex_unlock(&remote_lock); return ERROR_BUSY; }
    host_cleanup_native.armed = FALSE;
    host_cleanup_native.event_handle = 0; /* auxiliary borrow only; no ordinary resource changes */
    pthread_mutex_unlock(&remote_lock);
    return ERROR_SUCCESS;
}
static DWORD host_cleanup_run_cancel(void)
{
    struct macdrv_remote_packet packet = {0};
    struct remote_producer *p = NULL;
    DWORD error;
    pthread_mutex_lock(&remote_lock);
    if (!host_cleanup_native.armed || host_cleanup_native.control_started)
    { pthread_mutex_unlock(&remote_lock); return ERROR_INVALID_STATE; }
    host_cleanup_native.control_started = TRUE;
    packet.size = sizeof(packet); packet.version = MACDRV_REMOTE_PREPARED_PACKET_VERSION;
    packet.operation = MACDRV_REMOTE_CANCEL_HOST; packet.root = wine_server_user_handle(host_cleanup_native.root);
    packet.root_epoch = host_cleanup_native.epoch; packet.lease_id = host_cleanup_native.lease;
    packet.host_ticket = host_cleanup_native.ticket; packet.producer_generation = host_cleanup_native.generation;
    packet.sequence = host_cleanup_native.sequence;
    pthread_mutex_unlock(&remote_lock);
    packet.producer_pid = GetCurrentProcessId(); packet.producer_tid = GetCurrentThreadId();
    error = packet_producer_begin(&packet, &p); /* actual ordinary request admission/ref */
    if (!error)
    {
        error = cancel_prepared_host(p, &packet); /* actual ordinary status control, with hold hook */
        error = packet_producer_end(p, packet.operation, error); /* consumes same request pin */
    }
    pthread_mutex_lock(&remote_lock);
    host_cleanup_native.control_error = error; host_cleanup_native.control_returned = TRUE;
    pthread_mutex_unlock(&remote_lock);
    return error;
}
static DWORD host_cleanup_finish(void)
{
    struct remote_host *h;
    struct remote_producer *p;
    pthread_mutex_lock(&remote_lock);
    h = host_cleanup_native.host; p = host_cleanup_native.producer;
    if (host_cleanup_native.held || host_cleanup_native.armed ||
        (host_cleanup_native.control_started && !host_cleanup_native.control_returned) ||
        (h && (h->status_cell || h->status_registered || h->status_queued || h->status_busy || h->status_controls)) ||
        (p && (p->active || p->watched || !p->finalized || p->host_status_owners || p->host_resources_pending ||
               p->request_inflight || p->cleanup_inflight || p->process || p->cleanup_process || p->status_cell)))
    { pthread_mutex_unlock(&remote_lock); return ERROR_BUSY; }
    /* Lost-owner negative must never use registry/counter zero as cleanup. */
    if (host_cleanup_native.bound && !host_cleanup_native.close_successes)
    { pthread_mutex_unlock(&remote_lock); return ERROR_INVALID_DATA; }
    host_cleanup_native.host = NULL; host_cleanup_native.producer = NULL;
    host_cleanup_native.finished = TRUE;
    pthread_mutex_unlock(&remote_lock);
    if (h) host_release(h);
    if (p) producer_release(p);
    return ERROR_SUCCESS;
}
NTSTATUS macdrv_host_cleanup_fixture(void *args)
{
    struct host_cleanup_fixture *f = args;
    DWORD error = ERROR_SUCCESS;
    if (!f || f->size != sizeof(*f) || f->version != HOST_CLEANUP_VERSION || f->reserved ||
        f->operation < HOST_CLEANUP_BIND || f->operation > HOST_CLEANUP_DISARM) return STATUS_INVALID_PARAMETER;
    if (f->operation == HOST_CLEANUP_BIND) error = host_cleanup_bind(f);
    else if (f->operation == HOST_CLEANUP_ARM) error = host_cleanup_arm(f);
    else if (f->operation == HOST_CLEANUP_RUN_CANCEL) error = host_cleanup_run_cancel();
    else if (f->operation == HOST_CLEANUP_FINISH) error = host_cleanup_finish();
    else if (f->operation == HOST_CLEANUP_DISARM) error = host_cleanup_disarm();
    host_cleanup_snapshot(f);
    f->status = error;
    return STATUS_SUCCESS;
}
