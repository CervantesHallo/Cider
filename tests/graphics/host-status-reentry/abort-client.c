#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include "fixture.h"
#include "reentry.h"
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
    if(printf("{\"type\":\"host-status-reentry-abort\",\"event\":\"check\",\"check\":%u,\"name\":\"%s\",\"ok\":%s}\n",checks,name,value?"true":"false")<0||fflush(stdout)==EOF)io_error=TRUE;
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
    if(printf("{\"type\":\"host-status-reentry-abort\",\"event\":\"snapshot\",\"snapshot\":%u,\"op\":%u,\"ntstatus\":%u,\"status\":%u,"
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
static fixture_fn reentry_fixture;
static struct reentry_frame reentry_state;
static BOOL reentry_operation(unsigned op,LONGLONG start)
{
    NTSTATUS status;LONGLONG end;UINT32 words[64];unsigned i;
    memset(&reentry_state,0,sizeof(reentry_state));reentry_state.size=sizeof(reentry_state);
    reentry_state.version=1;reentry_state.operation=op;reentry_state.root=root_value;reentry_state.child=child_value;
    status=reentry_fixture(&reentry_state);memcpy(words,&reentry_state,sizeof(words));
    if(printf("{\"type\":\"host-status-reentry-abort\",\"event\":\"reentry\",\"ntstatus\":%u,\"words\":[",(unsigned)(UINT32)status)<0)io_error=TRUE;
    for(i=0;i<64;++i)if(printf("%s%u",i?",":"",words[i])<0)io_error=TRUE;
    if(printf("]}\n")<0||fflush(stdout)==EOF)io_error=TRUE;
    return now(&end)&&end>=start&&end-start<4*frequency&&!status&&reentry_state.size==sizeof(reentry_state)&&
           reentry_state.version==1&&reentry_state.operation==op;
}
static BOOL reentry_once(unsigned op)
{LONGLONG start;return now(&start)&&reentry_operation(op,start)&&!reentry_state.status;}
static BOOL finish_reentry(void)
{
    LONGLONG start,end;
    if(!now(&start))return FALSE;
    for(;;)
    {
        if(!reentry_operation(REENTRY_FINISH,start)||(reentry_state.status&&reentry_state.status!=ERROR_IO_PENDING))return FALSE;
        if(!reentry_state.status&&!reentry_state.observer&&!reentry_state.armed&&!reentry_state.registered&&!reentry_state.owners&&!reentry_state.cleanup&&!reentry_state.setups)return TRUE;
        Sleep(2);if(!now(&end)||end<start||end-start>=4*frequency)return FALSE;
    }
}
static HANDLE worker_handle,entry_gate;
static DWORD entry_wait=WAIT_FAILED;
#ifdef CIDER_ENTRY_TIMEOUT_CASE
static const BOOL gate_timeout_case=TRUE;
#else
static const BOOL gate_timeout_case=FALSE;
#endif
static BOOL joined;
static struct reentry_frame worker_frame;
static NTSTATUS worker_status;
static DWORD WINAPI cancel_thread(void *unused)
{
    (void)unused;
    entry_wait=WaitForSingleObject(entry_gate,gate_timeout_case?0:5000);
    if(entry_wait!=WAIT_OBJECT_0){worker_status=entry_wait==WAIT_TIMEOUT?STATUS_TIMEOUT:STATUS_UNSUCCESSFUL;return 0;}
    memset(&worker_frame,0,sizeof(worker_frame));worker_frame.size=sizeof(worker_frame);worker_frame.version=1;
    worker_frame.operation=REENTRY_RUN_CANCEL;worker_frame.root=root_value;worker_frame.child=child_value;
    worker_status=reentry_fixture(&worker_frame);return 0;
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
    const WCHAR *name=L"CiderHostReentryAbortV1";
    WNDCLASSW wc={0};
    LARGE_INTEGER freq;
    HWND root=NULL,child=NULL;
    HMODULE module=NULL;
    ATOM atom=0;
    LONGLONG start;
    UINT64 epoch=0,lease=0,ticket=0;
    BOOL cleanup=TRUE,setup_called=FALSE,armed=FALSE;
    (void)argv;
    if(!check(argc==1,"no-options"))goto done;
    if(!check(QueryPerformanceFrequency(&freq)&&freq.QuadPart>0&&freq.QuadPart<INT64_MAX/5,"clock"))goto done;
    frequency=freq.QuadPart;wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=name;
    atom=RegisterClassW(&wc);if(!check(atom!=0,"register-class"))goto done;
    root=CreateWindowW(name,L"Cider actual context",WS_OVERLAPPEDWINDOW|WS_VISIBLE,20,20,360,240,NULL,NULL,wc.hInstance,NULL);
    if(!check(root!=NULL,"real-root"))goto done;
    child=CreateWindowW(name,L"actual child",WS_CHILD|WS_BORDER|WS_VISIBLE,10,10,180,100,root,NULL,wc.hInstance,NULL);
    if(!check(child!=NULL&&GetAncestor(child,GA_ROOT)==root,"real-child"))goto done;
    root_value=(UINT32)(uintptr_t)root;child_value=(UINT32)(uintptr_t)child;
    module=LoadLibraryW(L"winemac.drv");if(!check(module!=NULL,"load-driver"))goto done;
    fixture=(fixture_fn)(uintptr_t)GetProcAddress(module,"cider_live_host_fixture");if(!check(fixture!=NULL,"fixture-export"))goto done;
    setup_called=TRUE;
    if(!check(once(LIVE_SETUP),"actual-exported-context-and-v2-lease"))goto done;
    epoch=frame.epoch;lease=frame.lease;
    if(!check(epoch&&lease&&frame.context_id&&frame.context_owned&&frame.producer_reader_owned,"actual-context-identity"))goto done;
    if(!check(once(LIVE_PREPARE),"actual-known-ticket-prepare"))goto done;
    ticket=frame.ticket;
    if(!check(ticket&&frame.host_reader_owned&&frame.host_read==MACDRV_STATUS_READ_SNAPSHOT&&frame.host_values_valid,"ro-map-before-import"))goto done;
    reentry_fixture=(fixture_fn)(uintptr_t)GetProcAddress(module,"cider_host_reentry_fixture");
    if(!check(reentry_fixture!=NULL,"reentry-export"))goto done;
    if(!now(&start))goto done;
    {BOOL timely=reentry_operation(REENTRY_ARM,start);armed=reentry_state.observer!=0;
     if(!check(timely&&!reentry_state.status&&armed,"arm-attached-cancel-interleaving"))goto done;}
    entry_gate=CreateEventW(NULL,TRUE,FALSE,NULL);if(!check(entry_gate!=NULL,"create-owned-entry-gate"))goto done;
    worker_handle=CreateThread(NULL,0,cancel_thread,NULL,0,NULL);if(!check(worker_handle!=NULL,"start-gated-wine-worker"))goto done;
    if(!check(reentry_once(REENTRY_ABORT)&&!reentry_state.worker_started&&!reentry_state.worker_done&&!reentry_state.fired&&!(reentry_state.flags&1),"abort-before-actual-native-entry"))goto done;
    if(!check(gate_timeout_case?WaitForSingleObject(entry_gate,0)==WAIT_TIMEOUT:SetEvent(entry_gate),"explicit-real-entry-gate-outcome"))goto done;
    if(!check(join_worker()&&(gate_timeout_case?(entry_wait==WAIT_TIMEOUT&&worker_status==STATUS_TIMEOUT):(entry_wait==WAIT_OBJECT_0&&!worker_status&&!worker_frame.status)),"actual-aborted-worker-kernel-join"))goto done;
    if(!check(reentry_once(REENTRY_SNAPSHOT)&&!reentry_state.fired&&!(reentry_state.flags&1)&&
              (gate_timeout_case?(!reentry_state.worker_started&&!reentry_state.worker_done):
              (reentry_state.worker_started&&reentry_state.worker_done&&reentry_state.worker_error==ERROR_CANCELLED&&reentry_state.worker_wait_error)),
              "actual-worker-entry-ledger-matches-gate-outcome"))goto done;
    if(!check(gate_timeout_case?!reentry_state.sequence_synced:
              (reentry_once(REENTRY_SYNC_SEQUENCE)&&reentry_state.sequence_synced),"cleanup-does-not-invent-an-unentered-native-completion"))goto done;
    if(!check(now(&start)&&operation(LIVE_RELEASE_PRODUCER,start)&&(frame.status==0||frame.status==ERROR_IO_PENDING)&&
              frame.sequence==reentry_state.sequence+1&&frame.sequence<reentry_state.cancel_sequence&&!(frame.flags&LIVE_INSTALL_SENT),
              "ordinary-release-did-not-consume-unused-cancel-sequence"))goto done;
    if(!check(poll(LIVE_SNAPSHOT,1),"actual-post-reentry-model-removal-and-host-safe"))goto done;
    if(!check(frame.context_owned&&frame.epoch==epoch&&frame.lease==lease&&frame.ticket==ticket,"original-context-retained-through-reentry"))goto done;
    if(!check(poll(LIVE_SNAPSHOT,2),"actual-producer-safe"))goto done;
    if(!check(poll(LIVE_CLEANUP,3),"real-context-and-local-nt-release"))goto done;
    if(!check((frame.flags&LIVE_CONTEXT_RELEASED)&&!frame.close_status,"release-effects-not-fake-counts"))goto done;
    if(!check(finish_reentry(),"drop-independent-observation-pins"))goto done;
    armed=FALSE;
    {BOOL closed=CloseHandle(entry_gate);if(closed)entry_gate=NULL;if(!check(closed,"close-entry-gate-after-kernel-join"))goto done;}
    {BOOL closed=CloseHandle(worker_handle);if(closed)worker_handle=NULL;if(!check(closed,"close-joined-worker"))goto done;}
    {BOOL destroyed=DestroyWindow(child);if(destroyed)child=NULL;if(!check(destroyed,"destroy-child-after-safe"))goto done;}
    {BOOL destroyed=DestroyWindow(root);if(destroyed)root=NULL;if(!check(destroyed,"destroy-root-after-safe"))goto done;}
    if(!check(FreeLibrary(module),"release-driver"))goto done;
    module=NULL;
    if(!check(UnregisterClassW(name,wc.hInstance),"unregister-class"))goto done;
    atom=0;
done:
    if(entry_gate&&!SetEvent(entry_gate))cleanup=FALSE;
    if(armed&&reentry_fixture&&module)
    {
        if(!reentry_once(REENTRY_ABORT))cleanup=FALSE;
        if(worker_handle&&!joined&&!join_worker())cleanup=FALSE;
        if(cleanup&&worker_handle&&joined&&entry_wait==WAIT_OBJECT_0&&!reentry_once(REENTRY_SYNC_SEQUENCE))cleanup=FALSE;
    }
    if(cleanup&&setup_called&&fixture&&module)cleanup=poll(LIVE_CLEANUP,3);
    if(cleanup&&armed&&reentry_fixture&&module)cleanup=finish_reentry();
    if(cleanup)
    {
        if(worker_handle&&!CloseHandle(worker_handle))cleanup=FALSE;
        if(entry_gate&&!CloseHandle(entry_gate))cleanup=FALSE;
        if(child&&!DestroyWindow(child))cleanup=FALSE;
        if(root&&!DestroyWindow(root))cleanup=FALSE;
        if(module&&!FreeLibrary(module))cleanup=FALSE;
        if(atom&&!UnregisterClassW(name,wc.hInstance))cleanup=FALSE;
    }
    if(printf("{\"type\":\"host-status-reentry-abort\",\"event\":\"summary\",\"bits\":%u,\"checks\":%u,\"expected_checks\":33,\"failures\":%u,"
              "\"epoch\":%llu,\"lease\":%llu,\"ticket\":%llu,\"joined\":%s,\"gate_timeout_case\":%s,\"cleanup\":%s,\"io_error\":%s}\n",
              (unsigned)(8*sizeof(void *)),checks,failures,(unsigned long long)epoch,(unsigned long long)lease,
              (unsigned long long)ticket,joined?"true":"false",gate_timeout_case?"true":"false",cleanup?"true":"false",io_error?"true":"false")<0||fflush(stdout)==EOF)io_error=TRUE;
    return failures||checks!=33||!cleanup||io_error?1:0;
}
