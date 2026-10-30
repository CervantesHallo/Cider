#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include "wine/macdrv_remote.h"
#include <stdio.h>
#include <string.h>

/* Metadata/NT-page qualification only. Context1 is deliberately never imported;
 * this is not a CAContext, renderer, cross-process or gameplay test. */
static unsigned checks, failures;
static int output_failed;
static HWND root;
static UINT64 lease, epoch, birth;
static DWORD session_id;
static LONGLONG frequency;

static BOOL check(BOOL value, const char *name)
{
    ++checks;
    if (!value) ++failures;
    if (printf("{\"type\":\"host-status-metadata\",\"event\":\"check\",\"check\":%u,\"name\":\"%s\",\"ok\":%s}\n",
               checks, name, value ? "true" : "false") < 0 || fflush(stdout) == EOF) output_failed = 1;
    return value;
}
static BOOL tick(LONGLONG *value)
{
    LARGE_INTEGER now;
    if (!QueryPerformanceCounter(&now) || now.QuadPart < 0) return FALSE;
    *value = now.QuadPart;
    return TRUE;
}
static BOOL send_packet(struct macdrv_remote_packet *packet, UINT version, UINT operation, UINT64 sequence,
                        UINT64 ticket, UINT context_id)
{
    DWORD_PTR result = 0;
    LONGLONG start, end;
    LRESULT delivered;
    memset(packet, 0, sizeof(*packet));
    packet->size = sizeof(*packet); packet->version = version; packet->operation = operation;
    packet->root = (UINT32)(uintptr_t)root;
    packet->producer_pid = GetCurrentProcessId(); packet->producer_tid = GetCurrentThreadId();
    packet->producer_generation = operation == MACDRV_REMOTE_QUERY_ROOT ? 0 : UINT64_C(1) << 33;
    packet->root_epoch = operation == MACDRV_REMOTE_QUERY_ROOT ? 0 : epoch;
    packet->lease_id = operation <= MACDRV_REMOTE_ACQUIRE_PRODUCER ? 0 : lease;
    packet->host_ticket = ticket; packet->context_id = context_id; packet->sequence = sequence;
    packet->status = ERROR_IO_PENDING;
    if (!tick(&start)) return FALSE;
    delivered = SendMessageTimeoutW(root, WM_MACDRV_REMOTE_HOST_PACKET, 0, (LPARAM)packet,
                                   SMTO_ABORTIFHUNG, 3000, &result);
    if (!tick(&end) || end < start || end - start >= 4 * frequency) return FALSE;
    if (printf("{\"type\":\"host-status-metadata\",\"event\":\"call\",\"version\":%u,\"operation\":%u,"
               "\"sequence\":%llu,\"delivered\":%s,\"result\":%llu,\"status\":%u,\"lease\":%llu,\"ticket\":%llu}\n",
               version, operation, (unsigned long long)sequence, delivered ? "true" : "false",
               (unsigned long long)result, packet->status, (unsigned long long)packet->lease_id,
               (unsigned long long)packet->host_ticket) < 0 || fflush(stdout) == EOF) output_failed = 1;
    if (!tick(&end) || end < start || end - start >= 4 * frequency) return FALSE;
    return delivered && result == 1 && macdrv_remote_packet_header_valid(packet) &&
        packet->flags == MACDRV_REMOTE_PACKET_RESPONSE && packet->version == version && packet->operation == operation &&
        packet->producer_pid == GetCurrentProcessId() && packet->producer_tid == GetCurrentThreadId() &&
        packet->root_pid == GetCurrentProcessId() && packet->root_create_time == birth;
}
static void emit_page(const char *label, enum macdrv_status_read_result result, const struct macdrv_status_snapshot *s)
{
    if (printf("{\"type\":\"host-status-metadata\",\"event\":\"page\",\"label\":\"%s\",\"result\":%u,"
               "\"kind\":%u,\"session\":%u,\"root_pid\":%u,\"producer_pid\":%u,\"root_birth\":%llu,\"producer_birth\":%llu,"
               "\"root_epoch\":%llu,\"generation\":%llu,\"lease\":%llu,\"ticket\":%llu,\"context_id\":%u,"
               "\"safe\":%u,\"values_valid\":%u,\"state\":%u,\"applied\":%llu,\"quiescent\":%llu,"
               "\"requests\":%u,\"models\":%u,\"cleanup\":%u}\n",
               label,(unsigned)result,s->identity.kind,s->identity.session_id,s->identity.root_pid,s->identity.producer_pid,
               (unsigned long long)s->identity.root_birth,(unsigned long long)s->identity.producer_birth,
               (unsigned long long)s->identity.root_epoch,(unsigned long long)s->identity.generation,
               (unsigned long long)s->identity.lease,(unsigned long long)s->identity.ticket,s->identity.context_id,
               s->safe,s->values_valid,s->values.state,(unsigned long long)s->values.applied_sequence,
               (unsigned long long)s->values.quiescent_sequence,s->values.pending_requests,s->values.pending_models,
               s->values.cleanup_pending) < 0 || fflush(stdout)==EOF) output_failed=1;
}
static BOOL read_safe(struct macdrv_status_cell *cell, const char *label)
{
    struct macdrv_status_snapshot snapshot;
    enum macdrv_status_read_result result, previous = MACDRV_STATUS_READ_INVALID;
    LONGLONG start, now;
    if (!tick(&start)) return FALSE;
    for (;;)
    {
        result = macdrv_status_cell_read(cell, &snapshot);
        if (result != previous) { emit_page(label,result,&snapshot); previous=result; }
        if (!tick(&now) || now < start || now - start >= 4 * frequency) return FALSE;
        if (result == MACDRV_STATUS_READ_SAFE_ONLY)
            return snapshot.safe == 1 && !snapshot.values_valid;
        if (result != MACDRV_STATUS_READ_SNAPSHOT && result != MACDRV_STATUS_READ_RETRY &&
            result != MACDRV_STATUS_READ_EXHAUSTED) return FALSE;
        Sleep(5); /* pacing only; no USER/Native recovery request */
    }
}

int main(int argc, char **argv)
{
    static const WCHAR name[] = L"CiderHostStatusMetadataV1";
    WNDCLASSW wc;
    LARGE_INTEGER f;
    FILETIME created, exited, kernel, user;
    struct macdrv_remote_packet packet;
    struct macdrv_status_identity id, wrong_id;
    struct macdrv_status_cell *producer = NULL, *host = NULL, *wrong = NULL;
    struct macdrv_status_snapshot snapshot;
    MEMORY_BASIC_INFORMATION info;
    const void *view;
    SIZE_T bytes;
    UINT64 ticket = 0;
    ATOM atom = 0;
    NTSTATUS status;
    BOOL cleanup = TRUE, ok;
    unsigned expected = 28;
    (void)argv;
    memset(&wc, 0, sizeof(wc)); memset(&id, 0, sizeof(id));
    if (!check(argc == 1, "no-options")) goto done; /* 1 */
    if (!check(QueryPerformanceFrequency(&f) && f.QuadPart > 0 && f.QuadPart < INT64_MAX / 4, "clock")) goto done; /* 2 */
    frequency = f.QuadPart;
    wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = name;
    atom = RegisterClassW(&wc);
    if (!check(atom != 0, "register-class")) goto done; /* 3 */
    root = CreateWindowW(name, L"Cider metadata only", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                         20, 20, 320, 240, NULL, NULL, wc.hInstance, NULL);
    if (!check(root != NULL, "real-root")) goto done; /* 4 */
    ok = GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) &&
         ProcessIdToSessionId(GetCurrentProcessId(), &session_id);
    if (ok) birth = (UINT64)created.dwLowDateTime | (UINT64)created.dwHighDateTime << 32;
    if (!check(ok && birth, "actual-birth-session")) goto done; /* 5 */
    if (!check(send_packet(&packet, 2, MACDRV_REMOTE_QUERY_ROOT, 0, 0, 0) && !packet.status && packet.root_epoch,
               "actual-user96-v2-root-query")) goto done; /* 6 */
    epoch = packet.root_epoch;
    if (!check(send_packet(&packet, 2, MACDRV_REMOTE_ACQUIRE_PRODUCER, 0, 0, 0) && !packet.status && packet.lease_id,
               "actual-v2-acquire")) goto done; /* 7 */
    lease = packet.lease_id;
    if (!check(send_packet(&packet, 2, MACDRV_REMOTE_ACQUIRE_PRODUCER, 0, 0, 0) && !packet.status && packet.lease_id == lease,
               "same-protocol-idempotence")) goto done; /* 8 */
    if (!check(send_packet(&packet, 1, MACDRV_REMOTE_ACQUIRE_PRODUCER, 0, 0, 0) && packet.status == ERROR_REVISION_MISMATCH && !packet.lease_id,
               "cross-version-acquire-rejected")) goto done; /* 9 */
    if (!check(send_packet(&packet, 2, MACDRV_REMOTE_ACQUIRE_PRODUCER, 0, 0, 0) && !packet.status && packet.lease_id == lease,
               "existing-v2-unchanged")) goto done; /* 10 */
    id.kind = MACDRV_STATUS_PRODUCER; id.root_pid = id.producer_pid = GetCurrentProcessId();
    id.root_birth = id.producer_birth = birth; id.root_epoch = epoch; id.generation = UINT64_C(1) << 33;
    id.lease = lease; id.session_id = session_id;
    status = macdrv_status_cell_open(&id, &producer);
    if (!check(status == 0 && producer, "real-producer-ro-page")) goto done; /* 11 */
    if (!check(send_packet(&packet, 2, MACDRV_REMOTE_PREPARE_HOST, (UINT64_C(1) << 33) + 11, 0, 1) && !packet.status && packet.host_ticket,
               "metadata-reserve-never-import")) goto done; /* 12 */
    ticket = packet.host_ticket;
    id.kind = MACDRV_STATUS_HOST; id.ticket = ticket; id.context_id = 1;
    status = macdrv_status_cell_open(&id, &host);
    if (!check(status == 0 && host, "actual-host-ro-page")) goto done; /* 13 */
    if (!check(macdrv_status_cell_read(host, &snapshot) == MACDRV_STATUS_READ_SNAPSHOT && snapshot.values_valid && !snapshot.safe &&
               snapshot.values.state == MACDRV_STATUS_ACTIVE && !snapshot.values.applied_sequence && !snapshot.values.quiescent_sequence,
               "bootstrap-not-installed-prefix")) goto done; /* 14 */
    view = macdrv_status_cell_view(host, &bytes);
    if (!check(view && bytes >= sizeof(struct macdrv_status_wire) && VirtualQuery(view, &info, sizeof(info)) == sizeof(info) &&
               (info.Protect & 0xff) == PAGE_READONLY, "actual-page-read-only")) goto done; /* 15 */
    wrong_id = id; ++wrong_id.generation;
    status = macdrv_status_cell_open(&wrong_id, &wrong);
    if (!check(status == 0 && wrong && macdrv_status_cell_read(wrong, &snapshot) == MACDRV_STATUS_READ_MISMATCH,
               "same-name-wrong-immutable-identity-rejected")) goto done; /* 16 */
    if (!check(macdrv_status_cell_close(&wrong) == 0 && !wrong, "close-wrong-reader")) goto done; /* 17 */
    if (!check(send_packet(&packet, 2, MACDRV_REMOTE_INSTALL_HOST, (UINT64_C(1) << 33) + 12, 0, 1) && packet.status == ERROR_NOT_SUPPORTED,
               "v2-one-phase-import-rejected")) goto done; /* 18 */
    if (!check(send_packet(&packet, 2, MACDRV_REMOTE_CANCEL_HOST, (UINT64_C(1) << 33) + 13, ticket, 0) &&
               (packet.status == 0 || packet.status == ERROR_IO_PENDING), "known-ticket-cancel")) goto done; /* 19 */
    if (!check(read_safe(host,"host-terminal"), "retained-host-view-actual-safe")) goto done; /* 20 */
    if (!check(send_packet(&packet, 2, MACDRV_REMOTE_INSTALL_PREPARED_HOST, (UINT64_C(1) << 33) + 14, ticket, 0) &&
               packet.status == ERROR_INVALID_HANDLE, "late-install-cannot-resurrect")) goto done; /* 21 */
    if (!check(macdrv_status_cell_read(host, &snapshot) == MACDRV_STATUS_READ_SAFE_ONLY && !snapshot.values_valid,
               "safe-is-permission-only")) goto done; /* 22 */
    if (!check(macdrv_status_cell_close(&host) == 0 && !host, "close-host-reader")) goto done; /* 23 */
    if (!check(send_packet(&packet, 2, MACDRV_REMOTE_RELEASE_PRODUCER, (UINT64_C(1) << 33) + 16, 0, 0) &&
               (packet.status == 0 || packet.status == ERROR_IO_PENDING), "real-release-producer")) goto done; /* 24 */
    if (!check(read_safe(producer,"producer-terminal"), "whole-producer-waits-host-nt-owner-drain")) goto done; /* 25 */
    if (!check(macdrv_status_cell_close(&producer) == 0 && !producer, "close-producer-reader")) goto done; /* 26 */
    if (!check(DestroyWindow(root), "destroy-root")) goto done; /* 27 */
    root = NULL;
    if (!check(UnregisterClassW(name, wc.hInstance), "unregister-class")) goto done; /* 28 */
    atom = 0;
done:
    /* No INSTALL was ever sent with a usable reserved ticket before cancellation.
     * Failure cleanup closes only owned RO readers and the owned root. */
    if (wrong && macdrv_status_cell_close(&wrong)) cleanup = FALSE;
    if (host && macdrv_status_cell_close(&host)) cleanup = FALSE;
    if (producer && macdrv_status_cell_close(&producer)) cleanup = FALSE;
    if (root && !DestroyWindow(root)) cleanup = FALSE;
    if (atom && !UnregisterClassW(name, wc.hInstance)) cleanup = FALSE;
    if (printf("{\"type\":\"host-status-metadata\",\"event\":\"summary\",\"bits\":%u,\"checks\":%u,\"expected_checks\":%u,"
               "\"failures\":%u,\"lease\":%llu,\"ticket\":%llu,\"cleanup\":%s,\"io_error\":%s}\n",
               (unsigned)(8 * sizeof(void *)), checks, expected, failures, (unsigned long long)lease,
               (unsigned long long)ticket, cleanup ? "true" : "false", output_failed ? "true" : "false") < 0 || fflush(stdout) == EOF)
        output_failed = 1;
    return failures || checks != expected || !cleanup || output_failed ? 1 : 0;
}
