/* Actual known-ticket control using the ordinary v2 sender. Serialized private
 * PE controls run on Wine threads; no NT calls occur in the Cocoa observer. */
#include "fixture.h"
extern void macdrv_live_host_probe(void *,UINT,UINT64,struct live_host_fixture *);
extern bool macdrv_pixel_fixture_drain(void *);
static struct
{
    BOOL used,setup_done,prepare_sent,install_sent,cancel_sent,release_sent,context_released;
    BOOL context_release_attempted, resource_safe, producer_safe;
    HWND root,child;
    WineWindow *window;
    id_WineMetalSwapChain context;
    struct macdrv_status_cell *host_reader,*producer_reader;
    struct macdrv_status_identity identity;
    UINT context_id;
    UINT64 epoch,lease,ticket,sequence;
    DWORD reply_status;
    NTSTATUS close_status;
} live_host;
static void live_host_snapshot(struct live_host_fixture *f)
{
    struct macdrv_status_snapshot host={0},producer={0};
    f->flags=(live_host.setup_done?LIVE_SETUP_DONE:0)|(live_host.prepare_sent?LIVE_PREPARE_SENT:0)|
        (live_host.install_sent?LIVE_INSTALL_SENT:0)|(live_host.cancel_sent?LIVE_CANCEL_SENT:0)|
        (live_host.release_sent?LIVE_RELEASE_SENT:0)|(live_host.context_released?LIVE_CONTEXT_RELEASED:0);
    f->context_id=live_host.context_id;f->epoch=live_host.epoch;f->lease=live_host.lease;
    f->ticket=live_host.ticket;f->sequence=live_host.sequence;f->birth=live_host.identity.producer_birth;
    f->context_owned=live_host.context!=NULL;f->window_owned=live_host.window!=NULL;
    f->host_reader_owned=live_host.host_reader!=NULL;f->producer_reader_owned=live_host.producer_reader!=NULL;
    f->host_read=live_host.host_reader?macdrv_status_cell_read(live_host.host_reader,&host):MACDRV_STATUS_READ_INVALID;
    f->producer_read=live_host.producer_reader?macdrv_status_cell_read(live_host.producer_reader,&producer):MACDRV_STATUS_READ_INVALID;
    f->host_values_valid=host.values_valid;f->producer_values_valid=producer.values_valid;
    f->root_pid=live_host.identity.root_pid;f->producer_pid=live_host.identity.producer_pid;
    f->root_birth=live_host.identity.root_birth;f->role=LIVE_ROLE_PRODUCER;
    f->close_status=live_host.close_status;f->reply_status=live_host.reply_status;
    f->cocoa_valid=0;f->attached=f->ledger=f->indexed=~(UINT32)0;
    if(live_host.window&&live_host.context_id&&live_host.ticket)
        macdrv_live_host_probe(live_host.window,live_host.context_id,live_host.ticket,f);
}
static DWORD live_host_send(UINT operation,UINT64 ticket,UINT context_id,struct macdrv_remote_packet *packet)
{
    memset(packet,0,sizeof(*packet));
    if(live_host.sequence==~(UINT64)0)return ERROR_ARITHMETIC_OVERFLOW;
    packet->operation=operation;
    packet->producer_generation=operation==MACDRV_REMOTE_QUERY_ROOT?0:(UINT64)1<<33;
    packet->root_epoch=operation==MACDRV_REMOTE_QUERY_ROOT?0:live_host.epoch;
    packet->lease_id=operation<=MACDRV_REMOTE_ACQUIRE_PRODUCER?0:live_host.lease;
    packet->host_ticket=ticket;packet->context_id=context_id;
    if(operation>MACDRV_REMOTE_ACQUIRE_PRODUCER)packet->sequence=++live_host.sequence;
    if(operation==MACDRV_REMOTE_PREPARE_HOST)packet->child=wine_server_user_handle(live_host.child);
    if(!macdrv_remote_prepared_packet_send(live_host.root,packet))
    {live_host.reply_status=ERROR_IO_PENDING;return ERROR_IO_PENDING;} /* unknown, never replay an import */
    live_host.reply_status=packet->status;
    return packet->status;
}
static DWORD live_host_setup(const struct live_host_fixture *f)
{
    struct macdrv_remote_packet packet;
    KERNEL_USER_TIMES times;
    RECT bounds;
    DWORD error;
    NTSTATUS status;
    if(live_host.used)return ERROR_ALREADY_EXISTS;
    live_host.used=TRUE;live_host.root=wine_server_ptr_handle(f->root);live_host.child=wine_server_ptr_handle(f->child);
    live_host.sequence=((UINT64)1<<33)+20;
    if(!live_host.root||!live_host.child||NtUserGetAncestor(live_host.child,GA_ROOT)!=live_host.root||
       !NtUserGetClientRect(live_host.child,&bounds,NtUserGetWinMonitorDpi(live_host.child,MDT_RAW_DPI)))return ERROR_INVALID_WINDOW_HANDLE;
    error=live_host_send(MACDRV_REMOTE_QUERY_ROOT,0,0,&packet);if(error)return error;
    live_host.epoch=packet.root_epoch;
    if(!packet.root_pid||packet.root_pid==GetCurrentProcessId()||!packet.root_create_time)return ERROR_INVALID_DATA;
    live_host.identity.root_pid=packet.root_pid;live_host.identity.root_birth=packet.root_create_time;
    live_host.identity.root_epoch=packet.root_epoch;live_host.identity.producer_pid=GetCurrentProcessId();
    status=NtQueryInformationProcess(NtCurrentProcess(),ProcessTimes,&times,sizeof(times),NULL);
    if(!status)live_host.identity.producer_birth=times.CreateTime.QuadPart;
    if(!status)status=NtQueryInformationProcess(NtCurrentProcess(),ProcessSessionInformation,&live_host.identity.session_id,sizeof(live_host.identity.session_id),NULL);
    if(status)return RtlNtStatusToDosError(status);
    error=live_host_send(MACDRV_REMOTE_ACQUIRE_PRODUCER,0,0,&packet);if(error)return error;
    live_host.lease=packet.lease_id;live_host.identity.lease=packet.lease_id;
    live_host.identity.generation=(UINT64)1<<33;live_host.identity.kind=MACDRV_STATUS_PRODUCER;
    status=macdrv_status_cell_open(&live_host.identity,&live_host.producer_reader);
    if(status)return RtlNtStatusToDosError(status);
    live_host.context=macdrv_create_exported_metal_context(cgrect_from_rect(bounds),NULL);
    if(!live_host.context||!(live_host.context_id=macdrv_exported_metal_context_id(live_host.context)))return ERROR_NOT_ENOUGH_MEMORY;
    live_host.setup_done=TRUE;
    return ERROR_SUCCESS;
}
static DWORD live_host_cleanup(struct live_host_fixture *f)
{
    struct macdrv_remote_packet packet;
    struct macdrv_status_snapshot host={0},producer={0};
    enum macdrv_status_read_result h,p;
    DWORD error;
    NTSTATUS status;
    if(live_host.lease&&!live_host.release_sent)
    {
        /* Explicit irreversible release is cleanup, not an automatic INSTALL replay. */
        live_host.release_sent=TRUE;
        error=live_host_send(MACDRV_REMOTE_RELEASE_PRODUCER,0,0,&packet);
        if(error&&error!=ERROR_IO_PENDING)return error;
    }
    h=live_host.host_reader?macdrv_status_cell_read(live_host.host_reader,&host):MACDRV_STATUS_READ_INVALID;
    p=live_host.producer_reader?macdrv_status_cell_read(live_host.producer_reader,&producer):MACDRV_STATUS_READ_INVALID;
    if(p==MACDRV_STATUS_READ_SAFE_ONLY&&!producer.values_valid)live_host.producer_safe=TRUE;
    live_host_snapshot(f);
    if(live_host.install_sent&&!live_host.resource_safe)
    {
        /* The producer owns the context; the actual foreign root supplies
         * the page permission. Physical model observation is independently
         * obtained by the PE coordinator from that root via WM_COPYDATA. */
        if(h!=MACDRV_STATUS_READ_SAFE_ONLY&&p!=MACDRV_STATUS_READ_SAFE_ONLY)return ERROR_IO_PENDING;
        live_host.resource_safe=TRUE; /* before reader close makes later reads invalid */
    }
    if(live_host.context)
    {
        if(!macdrv_pixel_fixture_drain(live_host.context))return ERROR_IO_PENDING;
        if(live_host.context_release_attempted)return ERROR_INVALID_STATE;
        live_host.context_release_attempted=TRUE;
        macdrv_destroy_swapchain(live_host.context);
        live_host.context=NULL;live_host.context_released=TRUE;
    }
    if(live_host.lease&&!live_host.producer_safe)return ERROR_IO_PENDING;
    status=macdrv_status_cell_close(&live_host.host_reader);live_host.close_status=status;
    if(status)return RtlNtStatusToDosError(status);
    status=macdrv_status_cell_close(&live_host.producer_reader);live_host.close_status=status;
    if(status)return RtlNtStatusToDosError(status);
    if(live_host.window){macdrv_legacy_release_object(live_host.window);live_host.window=NULL;}
    return ERROR_SUCCESS;
}
static struct
{
    BOOL used,bound,last_absent,finished;
    HWND root;
    WineWindow *window;
    UINT64 epoch,birth,ticket,lease,producer_birth,last_probe_sequence;
    UINT context_id,producer_pid;
} cross_host;
static DWORD cross_host_control(struct live_host_fixture *f)
{
    struct live_host_fixture input=*f;
    struct macdrv_win_data *data;
    struct remote_host *host;
    KERNEL_USER_TIMES times;
    NTSTATUS status;
    HWND root=wine_server_ptr_handle(input.root),child=wine_server_ptr_handle(input.child);
    f->role=LIVE_ROLE_HOST;f->root_pid=GetCurrentProcessId();
    f->context_owned=f->host_reader_owned=f->producer_reader_owned=0;
    f->host_values_valid=f->producer_values_valid=0;
    f->host_read=f->producer_read=MACDRV_STATUS_READ_INVALID;
    f->flags=f->reply_status=f->close_status=0;f->cocoa_valid=0;
    f->attached=f->ledger=f->indexed=UINT32_MAX;f->probe_bound=0;f->probe_sequence=0;
    if(input.operation==LIVE_HOST_SETUP)
    {
        if(cross_host.used)return ERROR_ALREADY_EXISTS;
        cross_host.used=TRUE;cross_host.root=root;
        if(NtUserGetAncestor(child,GA_ROOT)!=root)return ERROR_INVALID_WINDOW_HANDLE;
        if(!(data=get_win_data(root)))return ERROR_INVALID_WINDOW_HANDLE;
        cross_host.window=macdrv_legacy_retain_object(data->cocoa_window);release_win_data(data);
        if(!cross_host.window||!(cross_host.epoch=macdrv_remote_root_epoch(root)))return ERROR_INVALID_HANDLE;
        status=NtQueryInformationProcess(NtCurrentProcess(),ProcessTimes,&times,sizeof(times),NULL);
        if(status)return RtlNtStatusToDosError(status);
        cross_host.birth=times.CreateTime.QuadPart;
    }
    else if(!cross_host.used||root!=cross_host.root)return ERROR_INVALID_HANDLE;
    else if(cross_host.finished)
    {if(input.operation!=LIVE_HOST_FINISH)return ERROR_INVALID_HANDLE;}
    else if(input.operation==LIVE_HOST_PROBE)
    {
        if(!input.ticket||!input.context_id||input.epoch!=cross_host.epoch||input.root_pid!=GetCurrentProcessId()||
           input.root_birth!=cross_host.birth||!input.producer_pid||input.producer_pid==GetCurrentProcessId()||
           !input.birth||!input.lease||!input.probe_sequence||input.probe_sequence<=cross_host.last_probe_sequence)
            return ERROR_INVALID_DATA;
        if(!cross_host.bound)
        {
            pthread_mutex_lock(&remote_lock);
            host=host_status_lookup_locked(input.ticket);
            if(!host||host->root->hwnd!=root||host->root->epoch!=cross_host.epoch||host->context_id!=input.context_id||
               !host->producer||host->producer->pid!=input.producer_pid||host->producer->birth!=input.birth||
               host->producer->id!=input.lease||host->state!=HOST_INSTALLED)
            {pthread_mutex_unlock(&remote_lock);return ERROR_INVALID_HANDLE;}
            cross_host.ticket=host->ticket;cross_host.context_id=host->context_id;
            cross_host.lease=host->producer->id;cross_host.producer_pid=host->producer->pid;
            cross_host.producer_birth=host->producer->birth;
            pthread_mutex_unlock(&remote_lock);
            macdrv_live_host_probe(cross_host.window,input.context_id,input.ticket,f);
            if(!f->cocoa_valid||f->attached!=1||f->ledger!=1||f->indexed!=1)return ERROR_INVALID_DATA;
            cross_host.bound=TRUE;
        }
        else if(input.ticket!=cross_host.ticket||input.context_id!=cross_host.context_id||input.producer_pid!=cross_host.producer_pid||
                input.lease!=cross_host.lease||input.birth!=cross_host.producer_birth)return ERROR_INVALID_DATA;
        macdrv_live_host_probe(cross_host.window,input.context_id,input.ticket,f);
        if(!f->cocoa_valid)return ERROR_INVALID_DATA;
        cross_host.last_absent=!f->attached&&!f->ledger&&!f->indexed;
        cross_host.last_probe_sequence=input.probe_sequence;
    }
    else if(input.operation==LIVE_HOST_FINISH)
    {
        if(cross_host.bound)
        {
            macdrv_live_host_probe(cross_host.window,cross_host.context_id,cross_host.ticket,f);
            if(!f->cocoa_valid||f->attached||f->ledger||f->indexed)return ERROR_BUSY;
            cross_host.last_absent=TRUE;
        }
        if(cross_host.window){macdrv_legacy_release_object(cross_host.window);cross_host.window=NULL;}
        cross_host.finished=TRUE;
    }
    f->role=LIVE_ROLE_HOST;f->root_pid=GetCurrentProcessId();f->root_birth=cross_host.birth;
    f->epoch=cross_host.epoch;f->probe_bound=cross_host.bound;
    f->lease=cross_host.lease;f->ticket=cross_host.ticket;f->context_id=cross_host.context_id;
    f->producer_pid=cross_host.producer_pid;f->birth=cross_host.producer_birth;
    f->window_owned=cross_host.window!=NULL;f->probe_sequence=cross_host.last_probe_sequence;
    return ERROR_SUCCESS;
}
NTSTATUS macdrv_live_host_fixture(void *args)
{
    struct live_host_fixture *f=args;
    struct macdrv_remote_packet packet;
    struct macdrv_status_identity identity;
    DWORD error=ERROR_SUCCESS;
    NTSTATUS status;
    if(!f||f->size!=sizeof(*f)||f->version!=2||f->operation<LIVE_SETUP||f->operation>LIVE_HOST_FINISH||!f->root||!f->child)
        return STATUS_INVALID_PARAMETER;
    if(f->operation>=LIVE_HOST_SETUP)
    {
        f->status=cross_host_control(f);
        return STATUS_SUCCESS;
    }
    if(f->operation==LIVE_SETUP)error=live_host_setup(f);
    else if(!live_host.used||wine_server_ptr_handle(f->root)!=live_host.root||wine_server_ptr_handle(f->child)!=live_host.child)
        error=ERROR_INVALID_HANDLE;
    else if(f->operation==LIVE_PREPARE)
    {
        if(!live_host.setup_done||live_host.prepare_sent)error=ERROR_INVALID_STATE;
        else
        {
            live_host.prepare_sent=TRUE;
            error=live_host_send(MACDRV_REMOTE_PREPARE_HOST,0,live_host.context_id,&packet);
            if(!error)
            {
                live_host.ticket=packet.host_ticket;
                identity=live_host.identity;identity.kind=MACDRV_STATUS_HOST;
                identity.ticket=live_host.ticket;identity.context_id=live_host.context_id;
                status=macdrv_status_cell_open(&identity,&live_host.host_reader);
                if(status)error=RtlNtStatusToDosError(status);
            }
        }
    }
    else if(f->operation==LIVE_INSTALL)
    {
        struct macdrv_status_snapshot snapshot;
        if(!live_host.host_reader||live_host.install_sent||
           macdrv_status_cell_read(live_host.host_reader,&snapshot)!=MACDRV_STATUS_READ_SNAPSHOT)error=ERROR_INVALID_STATE;
        else
        {
            live_host.install_sent=TRUE; /* preserve unknown side effects before send */
            error=live_host_send(MACDRV_REMOTE_INSTALL_PREPARED_HOST,live_host.ticket,0,&packet);
        }
    }
    else if(f->operation==LIVE_CANCEL)
    {
        if(!live_host.ticket||live_host.cancel_sent)error=ERROR_INVALID_STATE;
        else{live_host.cancel_sent=TRUE;error=live_host_send(MACDRV_REMOTE_CANCEL_HOST,live_host.ticket,0,&packet);}
    }
    else if(f->operation==LIVE_RELEASE_PRODUCER)
    {
        if(!live_host.lease||live_host.release_sent)error=ERROR_INVALID_STATE;
        else{live_host.release_sent=TRUE;error=live_host_send(MACDRV_REMOTE_RELEASE_PRODUCER,0,0,&packet);}
    }
    else if(f->operation==LIVE_CLEANUP)error=live_host_cleanup(f);
    live_host_snapshot(f);
    f->status=error;
    return STATUS_SUCCESS;
}
