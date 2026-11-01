#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include "fixture.h"
#include "overlap.h"
#include <stdio.h>
#include <string.h>
typedef NTSTATUS (WINAPI *fixture_fn)(void *);
static unsigned checks,failures,snapshots;
static BOOL io_error;
static struct live_host_fixture frame;
static fixture_fn fixture;
static UINT32 root_value,child_value;
static LONGLONG frequency;
static BOOL check(BOOL value,const char *name)
{
    ++checks;if(!value)++failures;
    if(printf("{\"type\":\"host-status-timeout\",\"event\":\"check\",\"check\":%u,\"name\":\"%s\",\"ok\":%s}\n",checks,name,value?"true":"false")<0||fflush(stdout)==EOF)io_error=TRUE;
    return value;
}
static BOOL now(LONGLONG *n)
{
    LARGE_INTEGER value;if(!QueryPerformanceCounter(&value)||value.QuadPart<0)return FALSE;*n=value.QuadPart;return TRUE;
}
static BOOL operation(unsigned op,LONGLONG start)
{
    NTSTATUS status;
    LONGLONG end;
    memset(&frame,0,sizeof(frame));frame.size=sizeof(frame);frame.version=1;frame.operation=op;
    frame.root=root_value;frame.child=child_value;frame.status=ERROR_INVALID_DATA;
    status=fixture(&frame);
    ++snapshots;
    if(printf("{\"type\":\"host-status-timeout\",\"event\":\"snapshot\",\"snapshot\":%u,\"op\":%u,\"ntstatus\":%u,\"status\":%u,"
              "\"root\":%u,\"child\":%u,\"context\":%u,\"flags\":%u,\"epoch\":%llu,\"lease\":%llu,\"ticket\":%llu,\"sequence\":%llu,\"birth\":%llu,"
              "\"host_read\":%u,\"producer_read\":%u,\"host_values_valid\":%u,\"producer_values_valid\":%u,\"cocoa_valid\":%u,\"attached\":%u,\"ledger\":%u,\"indexed\":%u,"
              "\"context_owned\":%u,\"host_reader_owned\":%u,\"producer_reader_owned\":%u,\"window_owned\":%u,\"close_status\":%u,\"reply_status\":%u}\n",
              snapshots,op,(unsigned)(UINT32)status,frame.status,frame.root,frame.child,frame.context_id,frame.flags,
              (unsigned long long)frame.epoch,(unsigned long long)frame.lease,(unsigned long long)frame.ticket,
              (unsigned long long)frame.sequence,(unsigned long long)frame.birth,frame.host_read,frame.producer_read,
              frame.host_values_valid,frame.producer_values_valid,frame.cocoa_valid,frame.attached,frame.ledger,frame.indexed,
              frame.context_owned,frame.host_reader_owned,frame.producer_reader_owned,frame.window_owned,frame.close_status,frame.reply_status)<0||fflush(stdout)==EOF)io_error=TRUE;
    return now(&end)&&end>=start&&end-start<4*frequency&&!status&&frame.size==sizeof(frame)&&frame.version==1&&
        frame.operation==op&&frame.root==root_value&&frame.child==child_value;
}
static BOOL once(unsigned op)
{
    LONGLONG start;return now(&start)&&operation(op,start)&&!frame.status;
}
static BOOL poll(unsigned op,unsigned kind)
{
    LONGLONG start,end;
    if(!now(&start))return FALSE;
    for(;;)
    {
        if(!operation(op,start)||(frame.status&&frame.status!=ERROR_IO_PENDING))return FALSE;
        if(kind==1&&frame.host_read==MACDRV_STATUS_READ_SAFE_ONLY&&!frame.host_values_valid&&frame.cocoa_valid&&!frame.attached&&!frame.ledger&&!frame.indexed)return TRUE;
        if(kind==2&&frame.producer_read==MACDRV_STATUS_READ_SAFE_ONLY&&!frame.producer_values_valid)return TRUE;
        if(kind==3&&!frame.status&&!frame.context_owned&&!frame.host_reader_owned&&!frame.producer_reader_owned&&!frame.window_owned)return TRUE;
        Sleep(5);
        if(!now(&end)||end<start||end-start>=4*frequency)return FALSE;
    }
}
static fixture_fn overlap_fixture;
static HANDLE event_handle,worker_handle;
static BOOL joined;
static struct overlap_frame overlap_state,worker_state;
static NTSTATUS worker_status,worker_live_status;
static struct live_host_fixture worker_live;
static BOOL worker_released;
static void initialize_overlap(struct overlap_frame *f,unsigned op)
{
    memset(f,0,sizeof(*f));f->size=sizeof(*f);f->version=1;f->operation=op;
    f->root=root_value;f->child=child_value;f->event=(UINT32)(uintptr_t)event_handle;
}
static void print_overlap(const struct overlap_frame *f,NTSTATUS status,const char *label)
{
    unsigned i;UINT32 words[44];memcpy(words,f,sizeof(words));
    if(printf("{\"type\":\"host-status-timeout\",\"event\":\"overlap\",\"label\":\"%s\",\"ntstatus\":%u,\"words\":[",label,(unsigned)(UINT32)status)<0)io_error=TRUE;
    for(i=0;i<44;++i)if(printf("%s%u",i?",":"",words[i])<0)io_error=TRUE;
    if(printf("]}\n")<0||fflush(stdout)==EOF)io_error=TRUE;
}
static BOOL overlap_operation(unsigned op,LONGLONG start)
{
    NTSTATUS status;LONGLONG end;
    initialize_overlap(&overlap_state,op);status=overlap_fixture(&overlap_state);
    print_overlap(&overlap_state,status,"main");
    return now(&end)&&end>=start&&end-start<4*frequency&&!status&&overlap_state.size==sizeof(overlap_state)&&
        overlap_state.version==1&&overlap_state.operation==op;
}
static BOOL overlap_once(unsigned op)
{
    LONGLONG start;return now(&start)&&overlap_operation(op,start)&&!overlap_state.status;
}
static BOOL overlap_poll(unsigned op,BOOL finishing)
{
    LONGLONG start,end;
    if(!now(&start))return FALSE;
    for(;;)
    {
        if(!overlap_operation(op,start)||(overlap_state.status&&overlap_state.status!=ERROR_IO_PENDING))return FALSE;
        if(finishing?(!overlap_state.status&&!overlap_state.observer&&!overlap_state.event_borrow):
                     (overlap_state.held&&overlap_state.jobs==1&&overlap_state.controls==1&&overlap_state.requests==1))return TRUE;
        Sleep(2);if(!now(&end)||end<start||end-start>=4*frequency)return FALSE;
    }
}
static DWORD WINAPI install_thread(void *unused)
{
    (void)unused;
    initialize_overlap(&worker_state,OVERLAP_RUN);
    worker_status=overlap_fixture(&worker_state);
    memset(&worker_live,0,sizeof(worker_live));worker_live.size=sizeof(worker_live);worker_live.version=1;
    worker_live.operation=LIVE_SNAPSHOT;worker_live.root=root_value;worker_live.child=child_value;
    worker_live_status=fixture(&worker_live);
    worker_released=SetEvent(event_handle); /* release actual receiver only after timeout/readback */
    return 0;
}
static BOOL join_worker(void)
{
    LONGLONG start,end;MSG msg;DWORD wait;
    if(!worker_handle)return TRUE;
    if(!now(&start))return FALSE;
    for(;;)
    {
        wait=WaitForSingleObject(worker_handle,0);
        if(wait==WAIT_OBJECT_0){joined=TRUE;return TRUE;}
        if(wait!=WAIT_TIMEOUT)return FALSE;
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        if(!now(&end)||end<start||end-start>=5*frequency)return FALSE;
        Sleep(2);
    }
}
int main(int argc,char **argv)
{
    const WCHAR *name=L"CiderHostTimeoutV1";
    WNDCLASSW wc={0};LARGE_INTEGER freq;HWND root=NULL,child=NULL;
    HMODULE module=NULL;ATOM atom=0;LONGLONG start;
    UINT64 epoch=0,lease=0,ticket=0;
    BOOL cleanup=TRUE,setup_called=FALSE,armed=FALSE;
    (void)argv;
    if(!check(argc==1,"no-options"))goto done;
    if(!check(QueryPerformanceFrequency(&freq)&&freq.QuadPart>0&&freq.QuadPart<INT64_MAX/5,"clock"))goto done;
    frequency=freq.QuadPart;wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=name;
    atom=RegisterClassW(&wc);if(!check(atom!=0,"register-class"))goto done;
    root=CreateWindowW(name,L"Cider actual USER timeout",WS_OVERLAPPEDWINDOW|WS_VISIBLE,20,20,360,240,NULL,NULL,wc.hInstance,NULL);
    if(!check(root!=NULL,"real-root"))goto done;
    child=CreateWindowW(name,L"actual child",WS_CHILD|WS_BORDER|WS_VISIBLE,10,10,180,100,root,NULL,wc.hInstance,NULL);
    if(!check(child!=NULL&&GetAncestor(child,GA_ROOT)==root,"real-child"))goto done;
    root_value=(UINT32)(uintptr_t)root;child_value=(UINT32)(uintptr_t)child;
    module=LoadLibraryW(L"winemac.drv");if(!check(module!=NULL,"load-driver"))goto done;
    fixture=(fixture_fn)(uintptr_t)GetProcAddress(module,"cider_live_host_fixture");if(!check(fixture!=NULL,"live-export"))goto done;
    setup_called=TRUE;
    if(!check(once(LIVE_SETUP),"actual-context-and-lease"))goto done;
    epoch=frame.epoch;lease=frame.lease;
    if(!check(epoch&&lease&&frame.context_id&&frame.context_owned&&frame.producer_reader_owned,"actual-context-identity"))goto done;
    if(!check(once(LIVE_PREPARE),"ordinary-user-prepare"))goto done;
    ticket=frame.ticket;
    if(!check(ticket&&frame.host_reader_owned&&frame.host_read==MACDRV_STATUS_READ_SNAPSHOT&&frame.host_values_valid,"ro-before-import"))goto done;
    overlap_fixture=(fixture_fn)(uintptr_t)GetProcAddress(module,"cider_host_overlap_fixture");
    if(!check(overlap_fixture!=NULL,"overlap-export"))goto done;
    event_handle=CreateEventW(NULL,TRUE,FALSE,NULL);if(!check(event_handle!=NULL,"owned-manual-event"))goto done;
    /* Commit the ARM effect before applying a post-call elapsed test. */
    if(!now(&start))goto done;
    {BOOL timely=overlap_operation(OVERLAP_ARM,start);armed=overlap_state.bound!=0;
     if(!check(timely&&!overlap_state.status&&armed,"arm-before-worker"))goto done;}
    worker_handle=CreateThread(NULL,0,install_thread,NULL,0,NULL);
    if(!check(worker_handle!=NULL,"start-ordinary-install-function"))goto done;
    /* The Root owner must pump the actual incoming USER request. The receiver
     * hook blocks this thread; the sender returns on its unchanged 3s timeout,
     * captures its RO/context witness, then signals the owned event. */
    if(!check(join_worker(),"actual-sender-thread-join-with-root-pump"))goto done;
    print_overlap(&worker_state,worker_status,"worker");
    if(printf("{\"type\":\"host-status-timeout\",\"event\":\"unknown-context\",\"ntstatus\":%u,\"status\":%u,\"host_read\":%u,\"host_values_valid\":%u,"
              "\"context_owned\":%u,\"host_reader_owned\":%u,\"producer_reader_owned\":%u,\"cocoa_valid\":%u,\"attached\":%u,\"ledger\":%u,\"indexed\":%u,\"receiver_released\":%s}\n",
              (unsigned)(UINT32)worker_live_status,worker_live.status,worker_live.host_read,worker_live.host_values_valid,
              worker_live.context_owned,worker_live.host_reader_owned,worker_live.producer_reader_owned,worker_live.cocoa_valid,
              worker_live.attached,worker_live.ledger,worker_live.indexed,worker_released?"true":"false")<0||fflush(stdout)==EOF)io_error=TRUE;
    if(!check(!worker_status&&!worker_state.status&&worker_state.returned&&!worker_state.delivered&&
              worker_state.error==ERROR_IO_PENDING&&worker_state.last_error==ERROR_TIMEOUT&&worker_state.sender_clock&&
              worker_state.elapsed_ms>=2500&&worker_state.elapsed_ms<4000,"actual-user-timeout-outcome"))goto done;
    if(!check(worker_state.held&&worker_state.controls==1&&worker_state.jobs==1&&worker_state.requests==1&&
              !worker_state.terminal&&!worker_state.resource_done,"actual-install-still-in-flight-at-timeout"))goto done;
    if(!check(!worker_live_status&&!worker_live.status&&worker_live.context_owned&&worker_live.host_reader_owned&&
              worker_live.epoch==epoch&&worker_live.lease==lease&&worker_live.ticket==ticket&&
              worker_live.host_read==MACDRV_STATUS_READ_SNAPSHOT&&worker_live.host_values_valid&&worker_live.cocoa_valid&&
              !worker_live.attached&&!worker_live.ledger&&!worker_live.indexed,"unknown-reply-retains-actual-context-and-ro"))goto done;
    if(!check(worker_released,"sender-released-owned-receiver-event"))goto done;
    if(!check(overlap_once(OVERLAP_SNAPSHOT)&&overlap_state.done&&overlap_state.timely&&!overlap_state.wait_status&&
              !overlap_state.jobs&&!overlap_state.controls,"actual-receiver-finished-after-timeout"))goto done;
    if(!check(once(LIVE_SNAPSHOT)&&frame.cocoa_valid&&frame.attached==1&&frame.ledger==1&&frame.indexed==1&&
              frame.context_owned&&frame.host_read==MACDRV_STATUS_READ_SNAPSHOT&&frame.host_values_valid,
              "actual-install-effects-exist-despite-unknown-reply"))goto done;
    if(!check(now(&start)&&operation(LIVE_CANCEL,start)&&(frame.status==0||frame.status==ERROR_IO_PENDING),
              "one-known-ticket-cancel-after-unknown-install"))goto done;
    if(!check(poll(LIVE_SNAPSHOT,1),"actual-host-safe-after-job-finishes"))goto done;
    if(!check(frame.context_owned&&frame.epoch==epoch&&frame.lease==lease&&frame.ticket==ticket,"context-held-through-safe"))goto done;
    if(!check(now(&start)&&operation(LIVE_RELEASE_PRODUCER,start)&&(frame.status==0||frame.status==ERROR_IO_PENDING),"ordinary-producer-release"))goto done;
    if(!check(poll(LIVE_SNAPSHOT,2),"actual-producer-safe"))goto done;
    if(!check(poll(LIVE_CLEANUP,3),"real-context-and-nt-release"))goto done;
    if(!check((frame.flags&LIVE_CONTEXT_RELEASED)&&!frame.close_status,"actual-release-effect"))goto done;
    if(!check(overlap_poll(OVERLAP_FINISH,TRUE),"drop-observation-pins-after-real-drain"))goto done;
    armed=FALSE;
    {BOOL closed=CloseHandle(worker_handle);if(closed)worker_handle=NULL;if(!check(closed,"close-joined-worker"))goto done;}
    {BOOL closed=CloseHandle(event_handle);if(closed)event_handle=NULL;if(!check(closed,"close-event-after-borrow-ended"))goto done;}
    {BOOL destroyed=DestroyWindow(child);if(destroyed)child=NULL;if(!check(destroyed,"destroy-child"))goto done;}
    {BOOL destroyed=DestroyWindow(root);if(destroyed)root=NULL;if(!check(destroyed,"destroy-root"))goto done;}
    {BOOL released=FreeLibrary(module);if(released)module=NULL;if(!check(released,"release-driver"))goto done;}
    {BOOL unregistered=UnregisterClassW(name,wc.hInstance);if(unregistered)atom=0;if(!check(unregistered,"unregister-class"))goto done;}
 done:
    if(event_handle&&!SetEvent(event_handle))cleanup=FALSE;
    if(worker_handle&&!joined&&!join_worker())cleanup=FALSE;
    if(cleanup&&setup_called&&fixture&&module)cleanup=poll(LIVE_CLEANUP,3);
    if(cleanup&&armed&&overlap_fixture&&module)cleanup=overlap_poll(OVERLAP_FINISH,TRUE);
    if(cleanup)
    {
        if(worker_handle&&!CloseHandle(worker_handle))cleanup=FALSE;
        if(event_handle&&!CloseHandle(event_handle))cleanup=FALSE;
        if(child&&!DestroyWindow(child))cleanup=FALSE;
        if(root&&!DestroyWindow(root))cleanup=FALSE;
        if(module&&!FreeLibrary(module))cleanup=FALSE;
        if(atom&&!UnregisterClassW(name,wc.hInstance))cleanup=FALSE;
    }
    if(printf("{\"type\":\"host-status-timeout\",\"event\":\"summary\",\"bits\":%u,\"checks\":%u,\"expected_checks\":36,\"failures\":%u,"
              "\"epoch\":%llu,\"lease\":%llu,\"ticket\":%llu,\"joined\":%s,\"cleanup\":%s,\"io_error\":%s}\n",
              (unsigned)(8*sizeof(void *)),checks,failures,(unsigned long long)epoch,(unsigned long long)lease,
              (unsigned long long)ticket,joined?"true":"false",cleanup?"true":"false",io_error?"true":"false")<0||fflush(stdout)==EOF)io_error=TRUE;
    return failures||checks!=36||!cleanup||io_error?1:0;
}
