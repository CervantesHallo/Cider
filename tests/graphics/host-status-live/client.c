#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include "fixture.h"
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
    if(printf("{\"type\":\"host-status-live\",\"event\":\"check\",\"check\":%u,\"name\":\"%s\",\"ok\":%s}\n",checks,name,value?"true":"false")<0||fflush(stdout)==EOF)io_error=TRUE;
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
    if(printf("{\"type\":\"host-status-live\",\"event\":\"snapshot\",\"snapshot\":%u,\"op\":%u,\"ntstatus\":%u,\"status\":%u,"
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
int main(int argc,char **argv)
{
    const WCHAR *name=L"CiderLiveHostStatusV1";
    WNDCLASSW wc={0};
    LARGE_INTEGER freq;
    HWND root=NULL,child=NULL;
    HMODULE module=NULL;
    ATOM atom=0;
    LONGLONG start;
    UINT64 epoch=0,lease=0,ticket=0;
    BOOL cleanup=TRUE,setup_called=FALSE;
    (void)argv;
    if(!check(argc==1,"no-options"))goto done; /*1*/
    if(!check(QueryPerformanceFrequency(&freq)&&freq.QuadPart>0&&freq.QuadPart<INT64_MAX/4,"clock"))goto done; /*2*/
    frequency=freq.QuadPart;wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=name;
    atom=RegisterClassW(&wc);if(!check(atom!=0,"register-class"))goto done; /*3*/
    root=CreateWindowW(name,L"Cider actual context",WS_OVERLAPPEDWINDOW|WS_VISIBLE,20,20,360,240,NULL,NULL,wc.hInstance,NULL);
    if(!check(root!=NULL,"real-root"))goto done; /*4*/
    child=CreateWindowW(name,L"actual child",WS_CHILD|WS_BORDER|WS_VISIBLE,10,10,180,100,root,NULL,wc.hInstance,NULL);
    if(!check(child!=NULL&&GetAncestor(child,GA_ROOT)==root,"real-child"))goto done; /*5*/
    root_value=(UINT32)(uintptr_t)root;child_value=(UINT32)(uintptr_t)child;
    module=LoadLibraryW(L"winemac.drv");if(!check(module!=NULL,"load-driver"))goto done; /*6*/
    fixture=(fixture_fn)(uintptr_t)GetProcAddress(module,"cider_live_host_fixture");if(!check(fixture!=NULL,"fixture-export"))goto done; /*7*/
    setup_called=TRUE;
    if(!check(once(LIVE_SETUP),"actual-exported-context-and-v2-lease"))goto done; /*8*/
    epoch=frame.epoch;lease=frame.lease;
    if(!check(epoch&&lease&&frame.context_id&&frame.context_owned&&frame.producer_reader_owned,"actual-context-identity"))goto done; /*9*/
    if(!check(once(LIVE_PREPARE),"actual-known-ticket-prepare"))goto done; /*10*/
    ticket=frame.ticket;
    if(!check(ticket&&frame.host_reader_owned&&frame.host_read==MACDRV_STATUS_READ_SNAPSHOT&&frame.host_values_valid,"ro-map-before-import"))goto done; /*11*/
    if(!check(once(LIVE_INSTALL),"actual-known-ticket-install"))goto done; /*12*/
    if(!check(frame.cocoa_valid&&frame.attached==1&&frame.ledger==1&&frame.indexed==1&&frame.context_owned,"actual-layer-ledger-selection"))goto done; /*13*/
    if(!check(frame.host_read==MACDRV_STATUS_READ_SNAPSHOT&&frame.host_values_valid,"live-page-not-safe"))goto done; /*14*/
    if(!check(now(&start)&&operation(LIVE_CANCEL,start)&&(frame.status==0||frame.status==ERROR_IO_PENDING),"actual-known-ticket-cancel"))goto done; /*15*/
    if(!check(poll(LIVE_SNAPSHOT,1),"actual-model-absence-and-ro-safe"))goto done; /*16*/
    if(!check(frame.context_owned&&frame.epoch==epoch&&frame.lease==lease&&frame.ticket==ticket,"context-retained-through-removal"))goto done; /*17*/
    if(!check(now(&start)&&operation(LIVE_RELEASE_PRODUCER,start)&&(frame.status==0||frame.status==ERROR_IO_PENDING),"actual-producer-release"))goto done; /*18*/
    if(!check(poll(LIVE_SNAPSHOT,2),"actual-producer-safe"))goto done; /*19*/
    if(!check(poll(LIVE_CLEANUP,3),"real-context-and-local-nt-release"))goto done; /*20*/
    if(!check((frame.flags&LIVE_CONTEXT_RELEASED)&&!frame.close_status,"release-effects-not-fake-counts"))goto done; /*21*/
    if(!check(DestroyWindow(child),"destroy-child"))goto done;
    child=NULL; /*22*/
    if(!check(DestroyWindow(root),"destroy-root"))goto done;
    root=NULL; /*23*/
    if(!check(FreeLibrary(module),"release-driver"))goto done;
    module=NULL; /*24*/
    if(!check(UnregisterClassW(name,wc.hInstance),"unregister-class"))goto done;
    atom=0; /*25*/
done:
    if(setup_called&&fixture&&module)cleanup=poll(LIVE_CLEANUP,3);
    if(cleanup)
    {
        if(child&&!DestroyWindow(child))cleanup=FALSE;
        if(root&&!DestroyWindow(root))cleanup=FALSE;
        if(module&&!FreeLibrary(module))cleanup=FALSE;
        if(atom&&!UnregisterClassW(name,wc.hInstance))cleanup=FALSE;
    }
    if(printf("{\"type\":\"host-status-live\",\"event\":\"summary\",\"bits\":%u,\"checks\":%u,\"expected_checks\":25,\"failures\":%u,"
              "\"epoch\":%llu,\"lease\":%llu,\"ticket\":%llu,\"cleanup\":%s,\"io_error\":%s}\n",
              (unsigned)(8*sizeof(void *)),checks,failures,(unsigned long long)epoch,(unsigned long long)lease,
              (unsigned long long)ticket,cleanup?"true":"false",io_error?"true":"false")<0||fflush(stdout)==EOF)io_error=TRUE;
    return failures||checks!=25||!cleanup||io_error?1:0;
}
