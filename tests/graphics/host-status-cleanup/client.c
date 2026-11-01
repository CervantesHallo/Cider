#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include "wine/macdrv_remote.h"
#include "fixture.h"
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
    if (printf("{\"type\":\"host-cleanup-race\",\"event\":\"check\",\"check\":%u,\"name\":\"%s\",\"ok\":%s}\n",
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
    if (printf("{\"type\":\"host-cleanup-race\",\"event\":\"call\",\"version\":%u,\"operation\":%u,"
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
    if (printf("{\"type\":\"host-cleanup-race\",\"event\":\"page\",\"label\":\"%s\",\"result\":%u,"
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

typedef NTSTATUS (WINAPI *fixture_fn)(void *);
static fixture_fn fixture;
static struct host_cleanup_fixture frame;
static UINT64 ticket;
static HANDLE writer,event,worker;
static HMODULE module;
static BOOL bound_observer;
static BOOL protection_set,protection_cleared,clear_attempted;
static struct host_cleanup_fixture worker_frame;
static NTSTATUS worker_status;

static void init_frame(struct host_cleanup_fixture *f,unsigned op)
{
    memset(f,0,sizeof(*f));f->size=sizeof(*f);f->version=HOST_CLEANUP_VERSION;f->operation=op;
    f->root=(UINT32)(uintptr_t)root;f->context_id=HOST_CLEANUP_METADATA_ID;
    f->root_epoch=epoch;f->lease=lease;f->ticket=ticket;f->generation=HOST_CLEANUP_HIGH;
    f->event_handle=(UINT32)(uintptr_t)event;f->control_sequence=HOST_CLEANUP_HIGH+13;f->status=ERROR_INVALID_DATA;
}
static void emit_frame(const struct host_cleanup_fixture *f,NTSTATUS status,const char *label)
{
    unsigned i;UINT32 words[sizeof(*f)/sizeof(UINT32)];
    memcpy(words,f,sizeof(*f));
    if(printf("{\"type\":\"host-cleanup-race\",\"event\":\"frame\",\"label\":\"%s\",\"ntstatus\":%u,\"operation\":%u,\"status\":%u,"
              "\"lease\":%llu,\"ticket\":%llu,\"controls\":%u,\"cell\":%u,\"matches\":%u,\"owners\":%u,\"registered\":%u,\"close_seen\":%u,\"decision_seen\":%u,\"words\":[",
              label,(unsigned)(UINT32)status,f->operation,f->status,(unsigned long long)f->lease,(unsigned long long)f->ticket,
              f->status_controls,f->cell_present,f->cell_matches,f->host_owners,f->host_registered,f->close_seen,f->decision_seen)<0)output_failed=1;
    for(i=0;i<sizeof(words)/sizeof(words[0]);++i)
        if(printf("%s%u",i?",":"",words[i])<0)output_failed=1;
    if(printf("]}\n")<0||fflush(stdout)==EOF)output_failed=1;
}
static BOOL call_fixture(unsigned op,LONGLONG start)
{
    NTSTATUS status;LONGLONG end;
    init_frame(&frame,op);status=fixture(&frame);
    if(!status&&frame.size==sizeof(frame)&&frame.version==HOST_CLEANUP_VERSION)
    {
        if(frame.flags&HOST_CLEANUP_FINISHED)bound_observer=FALSE;
        else if(frame.flags&HOST_CLEANUP_BOUND)bound_observer=TRUE;
    }
    emit_frame(&frame,status,"main");
    return tick(&end)&&end>=start&&end-start<4*frequency&&!status&&frame.size==sizeof(frame)&&
        frame.version==HOST_CLEANUP_VERSION&&frame.operation==op&&frame.root==(UINT32)(uintptr_t)root&&
        frame.lease==lease&&frame.ticket==ticket&&frame.generation==HOST_CLEANUP_HIGH;
}
static BOOL once_fixture(unsigned op)
{
    LONGLONG start;return tick(&start)&&call_fixture(op,start)&&!frame.status;
}
static BOOL setup_fixture(unsigned op)
{
    LONGLONG start,end;if(!tick(&start))return FALSE;
    for(;;)
    {
        if(!call_fixture(op,start))return FALSE;
        if(!frame.status)return TRUE;
        if(frame.status!=ERROR_IO_PENDING)return FALSE;
        Sleep(5);if(!tick(&end)||end<start||end-start>=4*frequency)return FALSE;
    }
}
enum poll_kind { FAILED_CLOSE=1, HELD_DECISION, HOST_DRAINED, ALL_DRAINED };
static BOOL poll_fixture(enum poll_kind kind)
{
    LONGLONG start,end;BOOL ready;
    if(!tick(&start))return FALSE;
    for(;;)
    {
        if(!call_fixture(HOST_CLEANUP_SNAPSHOT,start)||frame.status)return FALSE;
        if(kind==FAILED_CLOSE)
            ready=frame.close_seen&&frame.cell_present&&frame.cell_matches&&frame.host_registered&&!frame.status_busy;
        else if(kind==HELD_DECISION)
            ready=frame.hold_active&&frame.decision_seen&&!frame.control_returned;
        else if(kind==HOST_DRAINED)
            ready=!frame.host_registered&&!frame.cell_present&&!frame.host_owners&&!frame.registered_count&&frame.close_successes;
        else
            ready=frame.finalized==1&&!frame.active&&!frame.watched&&!frame.request_inflight&&!frame.host_owners&&
                  !frame.host_resources&&!frame.registered_count&&!frame.native_cleanup&&!frame.native_setups&&!frame.native_watches;
        if(ready)return TRUE;
        Sleep(5);if(!tick(&end)||end<start||end-start>=4*frequency)return FALSE;
    }
}
static BOOL clear_protection(void)
{
    DWORD flags=0;BOOL ok;
    if(!protection_set||protection_cleared)return TRUE;
    if(clear_attempted)return FALSE;
    if(!GetHandleInformation(writer,&flags)||!(flags&HANDLE_FLAG_PROTECT_FROM_CLOSE))return FALSE;
    clear_attempted=TRUE;
    ok=SetHandleInformation(writer,HANDLE_FLAG_PROTECT_FROM_CLOSE,0);
    if(ok)protection_cleared=TRUE; /* before output/clock rejection; H may close immediately */
    if(printf("{\"type\":\"host-cleanup-race\",\"event\":\"clear\",\"before_flags\":%u,\"applied\":%s}\n",(unsigned)flags,ok?"true":"false")<0||fflush(stdout)==EOF)output_failed=1;
    return ok;
}
static DWORD WINAPI run_cancel(void *unused)
{
    (void)unused;
    init_frame(&worker_frame,HOST_CLEANUP_RUN_CANCEL);
    worker_status=fixture(&worker_frame);
    return (DWORD)(UINT32)worker_status;
}
int main(int argc,char **argv)
{
    static const WCHAR name[]=L"CiderOwnedCellCancelRaceV1";
    WNDCLASSW wc={0};LARGE_INTEGER f;FILETIME created,exited,kernel,user;
    struct macdrv_remote_packet packet;
    struct macdrv_status_identity id={0};
    struct macdrv_status_cell *producer=NULL,*host=NULL;
    ATOM atom=0;NTSTATUS status;DWORD flags=0,code=0;
    BOOL cleanup=TRUE,released=FALSE,worker_joined=FALSE,closed_worker,closed_event;
    (void)argv;
    if(!check(argc==1,"no-options"))goto done; /*1*/
    if(!check(QueryPerformanceFrequency(&f)&&f.QuadPart>0&&f.QuadPart<INT64_MAX/4,"clock"))goto done; /*2*/
    frequency=f.QuadPart;wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=name;
    atom=RegisterClassW(&wc);if(!check(atom!=0,"register-root"))goto done; /*3*/
    root=CreateWindowW(name,L"Cider own resource cleanup",WS_OVERLAPPEDWINDOW|WS_VISIBLE,20,20,320,240,NULL,NULL,wc.hInstance,NULL);
    if(!check(root!=NULL,"real-root"))goto done; /*4*/
    if(!check(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)&&ProcessIdToSessionId(GetCurrentProcessId(),&session_id),"actual-process-identity"))goto done; /*5*/
    birth=(UINT64)created.dwLowDateTime|((UINT64)created.dwHighDateTime<<32);
    if(!check(send_packet(&packet,2,MACDRV_REMOTE_QUERY_ROOT,0,0,0)&&!packet.status&&packet.root_epoch>=HOST_CLEANUP_HIGH,"actual-root-query"))goto done; /*6*/
    epoch=packet.root_epoch;
    if(!check(send_packet(&packet,2,MACDRV_REMOTE_ACQUIRE_PRODUCER,0,0,0)&&!packet.status&&packet.lease_id>=HOST_CLEANUP_HIGH,"actual-producer-acquire"))goto done; /*7*/
    lease=packet.lease_id;
    id.kind=MACDRV_STATUS_PRODUCER;id.root_pid=id.producer_pid=GetCurrentProcessId();id.root_birth=id.producer_birth=birth;
    id.root_epoch=epoch;id.generation=HOST_CLEANUP_HIGH;id.lease=lease;id.session_id=session_id;
    status=macdrv_status_cell_open(&id,&producer);if(!check(!status&&producer,"actual-producer-ro"))goto done; /*8*/
    if(!check(send_packet(&packet,2,MACDRV_REMOTE_PREPARE_HOST,HOST_CLEANUP_HIGH+11,0,HOST_CLEANUP_METADATA_ID)&&!packet.status&&packet.host_ticket>=HOST_CLEANUP_HIGH,"reserve-own-metadata-host"))goto done; /*9*/
    ticket=packet.host_ticket;id.kind=MACDRV_STATUS_HOST;id.ticket=ticket;id.context_id=HOST_CLEANUP_METADATA_ID;
    status=macdrv_status_cell_open(&id,&host);if(!check(!status&&host,"actual-host-ro"))goto done; /*10*/
    {
        module=LoadLibraryW(L"winemac.drv");
        fixture=module?(fixture_fn)(uintptr_t)GetProcAddress(module,"cider_host_cleanup_fixture"):NULL;
    }
    if(!check(fixture!=NULL,"resolve-own-resource-observer"))goto done; /*11*/
    if(!check(setup_fixture(HOST_CLEANUP_BIND),"bind-real-host-and-writer-handle"))goto done; /*12*/
    writer=(HANDLE)(uintptr_t)frame.writer_handle;
    if(!check(frame.producer_birth==birth&&frame.root_birth==birth&&frame.cell_present&&frame.cell_matches&&frame.host_owners==1&&frame.host_registered&&writer,"exact-owner-identity"))goto done; /*13*/
    if(!check(GetHandleInformation(writer,&flags)&&!(flags&HANDLE_FLAG_PROTECT_FROM_CLOSE),"real-writer-initial-flags"))goto done; /*14*/
    protection_set=SetHandleInformation(writer,HANDLE_FLAG_PROTECT_FROM_CLOSE,HANDLE_FLAG_PROTECT_FROM_CLOSE);
    if(!check(protection_set&&GetHandleInformation(writer,&flags)&&(flags&HANDLE_FLAG_PROTECT_FROM_CLOSE),"real-own-handle-protection"))goto done; /*15*/
    if(!check(send_packet(&packet,2,MACDRV_REMOTE_CANCEL_HOST,HOST_CLEANUP_HIGH+12,ticket,0)&&(!packet.status||packet.status==ERROR_IO_PENDING),"actual-first-cancel"))goto done; /*16*/
    if(!check(poll_fixture(FAILED_CLOSE),"actual-close-failure-retains-cell"))goto done; /*17*/
    if(!check(frame.close_status==(UINT32)STATUS_HANDLE_NOT_CLOSABLE&&frame.close_partial==1&&frame.host_owners==1&&frame.registered_count==1&&frame.status_resource_done&&frame.status_terminal&&!frame.model_possible,"real-partial-nt-owner-not-physical-work"))goto done; /*18*/
    if(!check(read_safe(host,"host-model-safe-nt-owner-still-pending"),"actual-host-resource-permission"))goto done; /*19*/
    event=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(!check(event!=NULL&&(uintptr_t)event<=UINT32_MAX,"owned-event-handle"))goto done; /*20*/
    if(!check(setup_fixture(HOST_CLEANUP_ARM),"arm-owned-control-event"))goto done; /*21*/
    if(!check(once_fixture(HOST_CLEANUP_DISARM)&&!frame.hold_armed&&!frame.event_handle&&frame.host_registered&&frame.host_owners==1&&GetHandleInformation(writer,&flags)&&(flags&HANDLE_FLAG_PROTECT_FROM_CLOSE),"unused-arm-disarms-without-changing-owner"))goto done;
    if(!check(setup_fixture(HOST_CLEANUP_ARM),"rearm-after-unused-control"))goto done;
    worker=CreateThread(NULL,0,run_cancel,NULL,0,NULL);
    if(!check(worker!=NULL,"start-actual-concurrent-cancel"))goto done; /*22*/
    if(!check(poll_fixture(HELD_DECISION),"real-cancel-held-and-timer-retry-observed"))goto done; /*23*/
    if(!check(frame.decision_serial==1&&frame.decision_controls==1&&frame.decision_resource_done==1&&frame.decision_busy==1&&
        !frame.decision_owns_cell&&!frame.decision_attempted&&!frame.decision_removed&&frame.decision_cell_present&&frame.decision_cell_matches&&
        frame.decision_registered&&frame.decision_host_owners==1&&frame.decision_registry_count==1,"skipped-detach-preserves-exact-nt-owner"))goto done; /*24*/
    if(!check(GetHandleInformation(writer,&flags)&&(flags&HANDLE_FLAG_PROTECT_FROM_CLOSE),"same-real-writer-still-valid"))goto done; /*25*/
    if(!check(SetEvent(event),"release-owned-event"))goto done; /*26*/
    worker_joined=WaitForSingleObject(worker,4000)==WAIT_OBJECT_0;
    if(!check(worker_joined&&GetExitCodeThread(worker,&code)&&!code&&!worker_status,"actual-control-thread-return"))goto done; /*27*/
    emit_frame(&worker_frame,worker_status,"worker");
    if(!check(once_fixture(HOST_CLEANUP_SNAPSHOT)&&frame.hold_done&&frame.hold_timely&&!frame.hold_wait_status&&frame.control_returned&&
        (!frame.control_error||frame.control_error==ERROR_IO_PENDING),"owned-event-hold-timely"))goto done; /*28*/
    if(!check(once_fixture(HOST_CLEANUP_DISARM)&&!frame.hold_armed&&!frame.event_handle,"joined-control-releases-auxiliary-event-borrow"))goto done;
    if(!check(clear_protection(),"clear-real-protection-once"))goto done; /*29*/
    if(!check(poll_fixture(HOST_DRAINED),"autonomous-actual-host-nt-drain"))goto done; /*30*/
    if(!check(frame.close_successes&&!frame.last_close_status&&!frame.last_close_partial,"actual-nt-close-consumed-partial-owner"))goto done; /*31*/
    released=TRUE;
    if(!check(send_packet(&packet,2,MACDRV_REMOTE_RELEASE_PRODUCER,HOST_CLEANUP_HIGH+14,0,0)&&(!packet.status||packet.status==ERROR_IO_PENDING),"actual-producer-release"))goto done; /*32*/
    if(!check(read_safe(producer,"whole-producer-after-actual-host-nt-close")&&poll_fixture(ALL_DRAINED),"actual-producer-safe-after-nt-drain"))goto done; /*33*/
    if(!check(!macdrv_status_cell_close(&host)&&!host&&!macdrv_status_cell_close(&producer)&&!producer,"close-owned-ro-readers"))goto done; /*34*/
    if(!check(once_fixture(HOST_CLEANUP_FINISH)&&!frame.observer_owned,"drop-independent-observation-pins"))goto done; /*35*/
    bound_observer=FALSE;
    closed_worker=CloseHandle(worker);if(closed_worker)worker=NULL;
    closed_event=CloseHandle(event);if(closed_event)event=NULL;
    if(!check(closed_worker&&closed_event,"close-event-after-thread-exit"))goto done; /*36*/
    if(!check(DestroyWindow(root),"destroy-root"))goto done;
    root=NULL; /*37*/
    if(!check(UnregisterClassW(name,wc.hInstance),"unregister-root"))goto done;
    atom=0; /*38*/
    {
        BOOL closed_module=FreeLibrary(module);if(closed_module)module=NULL;
        if(!check(closed_module,"release-fixture-module-reference"))goto done; /*39*/
    }
done:
    if(event&&!SetEvent(event))cleanup=FALSE;
    if(worker&&!worker_joined)worker_joined=WaitForSingleObject(worker,5000)==WAIT_OBJECT_0;
    if(worker&&!worker_joined)cleanup=FALSE;
    if(fixture&&bound_observer&&(!worker||worker_joined)&&!once_fixture(HOST_CLEANUP_DISARM))cleanup=FALSE;
    if(!clear_protection())cleanup=FALSE;
    if(bound_observer&&cleanup)
    {
        if(!released&&lease)
        {released=TRUE;if(!send_packet(&packet,2,MACDRV_REMOTE_RELEASE_PRODUCER,HOST_CLEANUP_HIGH+14,0,0))cleanup=FALSE;}
        if(cleanup&&!poll_fixture(ALL_DRAINED))cleanup=FALSE;
        if(cleanup&&!once_fixture(HOST_CLEANUP_FINISH))cleanup=FALSE;
    }
    if(host&&macdrv_status_cell_close(&host))cleanup=FALSE;
    if(producer&&macdrv_status_cell_close(&producer))cleanup=FALSE;
    if(worker&&worker_joined&&!CloseHandle(worker))cleanup=FALSE;
    if(event&&(!worker||worker_joined)&&!CloseHandle(event))cleanup=FALSE;
    if(module&&(!worker||worker_joined)&&!FreeLibrary(module))cleanup=FALSE;
    if(root&&!DestroyWindow(root))cleanup=FALSE;
    if(atom&&!UnregisterClassW(name,wc.hInstance))cleanup=FALSE;
    if(printf("{\"type\":\"host-cleanup-race\",\"event\":\"summary\",\"bits\":%u,\"checks\":%u,\"expected_checks\":42,\"failures\":%u,"
              "\"lease\":%llu,\"ticket\":%llu,\"protection_cleared\":%s,\"worker_joined\":%s,\"cleanup\":%s,\"io_error\":%s}\n",
              (unsigned)(8*sizeof(void *)),checks,failures,(unsigned long long)lease,(unsigned long long)ticket,
              protection_cleared?"true":"false",worker_joined?"true":"false",cleanup?"true":"false",output_failed?"true":"false")<0||fflush(stdout)==EOF)output_failed=1;
    return failures||checks!=42||!cleanup||output_failed?1:0;
}

