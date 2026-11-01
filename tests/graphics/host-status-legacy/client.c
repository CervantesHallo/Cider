#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include "fixture.h"
#include "legacy.h"
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
    if(printf("{\"type\":\"host-status-legacy\",\"event\":\"check\",\"check\":%u,\"name\":\"%s\",\"ok\":%s}\n",checks,name,value?"true":"false")<0||fflush(stdout)==EOF)io_error=TRUE;
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
    if(printf("{\"type\":\"host-status-legacy\",\"event\":\"snapshot\",\"snapshot\":%u,\"op\":%u,\"ntstatus\":%u,\"status\":%u,"
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
#ifdef CIDER_LEGACY_ONLY
static const BOOL legacy_only=TRUE;
#else
static const BOOL legacy_only=FALSE;
#endif
static fixture_fn legacy_fixture;
static struct legacy_frame legacy_state;
static BOOL legacy_operation(unsigned op,LONGLONG start)
{
    NTSTATUS status;LONGLONG end;UINT32 words[48];unsigned i;
    memset(&legacy_state,0,sizeof(legacy_state));legacy_state.size=sizeof(legacy_state);legacy_state.version=1;
    legacy_state.operation=op;legacy_state.root=root_value;legacy_state.child=child_value;
    status=legacy_fixture(&legacy_state);memcpy(words,&legacy_state,sizeof(words));
    if(printf("{\"type\":\"host-status-legacy\",\"event\":\"legacy\",\"ntstatus\":%u,\"words\":[",(unsigned)(UINT32)status)<0)io_error=TRUE;
    for(i=0;i<48;++i)if(printf("%s%u",i?",":"",words[i])<0)io_error=TRUE;
    if(printf("]}\n")<0||fflush(stdout)==EOF)io_error=TRUE;
    return now(&end)&&end>=start&&end-start<4*frequency&&!status&&legacy_state.size==sizeof(legacy_state)&&
           legacy_state.version==1&&legacy_state.operation==op;
}
static BOOL legacy_once(unsigned op)
{LONGLONG start;return now(&start)&&legacy_operation(op,start)&&!legacy_state.status;}
static BOOL legacy_poll(unsigned op,BOOL cleaning)
{
    LONGLONG start,end;if(!now(&start))return FALSE;
    for(;;)
    {
        if(!legacy_operation(op,start)||(legacy_state.status&&legacy_state.status!=ERROR_IO_PENDING))return FALSE;
        if(!legacy_state.status&&((!legacy_state.legacy_attempted&&cleaning)||
           (legacy_state.legacy_absent&&legacy_state.drop_done&&!legacy_state.drop_busy&&legacy_state.drop_callbacks&&legacy_state.drop_result))&&
           (!cleaning||(legacy_state.cleanup_done&&!legacy_state.context_owned&&!legacy_state.window_owned&&
                       !legacy_state.host_reader_owned&&!legacy_state.producer_reader_owned&&!legacy_state.close_status)))return TRUE;
        Sleep(2);if(!now(&end)||end<start||end-start>=4*frequency)return FALSE;
    }
}
int main(int argc,char **argv)
{
    const WCHAR *name=L"CiderHostLegacyV1";
    unsigned expected=legacy_only?22:30;
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
    legacy_fixture=(fixture_fn)(uintptr_t)GetProcAddress(module,"cider_host_legacy_fixture");
    if(!check(legacy_fixture!=NULL,"legacy-owner-export"))goto done;
    if(!check(legacy_once(LEGACY_CREATE),"actual-legacy-generation-zero-import"))goto done;
    if(!check(legacy_state.cocoa_valid&&legacy_state.legacy_attached==1&&legacy_state.legacy_ledger==1&&
              legacy_state.legacy_indexed==1&&legacy_state.context_owned&&legacy_state.create_called&&legacy_state.create_result,
              "actual-legacy-layer-ledger-selection"))goto done;
    if(legacy_only)
    {
        if(!check(now(&start)&&operation(LIVE_RELEASE_PRODUCER,start)&&(frame.status==0||frame.status==ERROR_IO_PENDING),"ordinary-producer-release-with-legacy-still-owned"))goto done;
        if(!check(poll(LIVE_SNAPSHOT,2),"actual-producer-safe-excludes-legacy-owner"))goto done;
        if(!check(legacy_once(LEGACY_SNAPSHOT)&&legacy_state.producer_read==MACDRV_STATUS_READ_SAFE_ONLY&&
                  !legacy_state.producer_values_valid&&legacy_state.context_owned&&legacy_state.cocoa_valid&&
                  legacy_state.legacy_attached==1&&legacy_state.legacy_ledger==1&&legacy_state.legacy_indexed==1,
                  "producer-safe-is-not-legacy-resource-permission"))goto done;
        if(!check(!legacy_state.ticket&&!(legacy_state.flags&LIVE_INSTALL_SENT)&&!(legacy_state.flags&LIVE_PREPARE_SENT),"no-managed-host-owner-in-this-case"))goto done;
    }
    else
    {
        if(!check(once(LIVE_PREPARE),"ordinary-known-ticket-prepare"))goto done;
        ticket=frame.ticket;
        if(!check(ticket&&frame.host_reader_owned&&frame.host_read==MACDRV_STATUS_READ_SNAPSHOT&&frame.host_values_valid,"actual-ro-before-managed-import"))goto done;
        if(!check(once(LIVE_INSTALL),"ordinary-managed-install-replaces-legacy"))goto done;
        if(!check(legacy_once(LEGACY_SNAPSHOT)&&legacy_state.cocoa_valid&&!legacy_state.legacy_attached&&!legacy_state.legacy_ledger&&!legacy_state.legacy_indexed&&
                  legacy_state.managed_attached==1&&legacy_state.managed_ledger==1&&legacy_state.managed_indexed==1&&legacy_state.managed_flag==1,
                  "actual-legacy-predecessor-gone-before-new-publication"))goto done;
        if(!check(legacy_state.context_owned&&legacy_state.legacy_attempted&&legacy_state.create_result&&legacy_state.ticket==ticket,"original-context-held-across-handoff"))goto done;
        if(!check(legacy_poll(LEGACY_DROP,FALSE),"checked-retirement-confirms-legacy-absence"))goto done;
        if(!check(legacy_state.cocoa_valid&&legacy_state.managed_attached==1&&legacy_state.managed_ledger==1&&legacy_state.managed_indexed==1&&
                  legacy_state.context_owned&&legacy_state.host_read==MACDRV_STATUS_READ_SNAPSHOT,"legacy-retirement-does-not-release-managed-context"))goto done;
        if(!check(now(&start)&&operation(LIVE_CANCEL,start)&&(frame.status==0||frame.status==ERROR_IO_PENDING),"ordinary-known-ticket-cancel"))goto done;
        if(!check(poll(LIVE_SNAPSHOT,1),"actual-managed-model-removal-and-host-safe"))goto done;
        if(!check(frame.context_owned&&frame.epoch==epoch&&frame.lease==lease&&frame.ticket==ticket,"same-context-held-through-managed-removal"))goto done;
        if(!check(now(&start)&&operation(LIVE_RELEASE_PRODUCER,start)&&(frame.status==0||frame.status==ERROR_IO_PENDING),"ordinary-producer-release"))goto done;
        if(!check(poll(LIVE_SNAPSHOT,2),"actual-managed-producer-safe"))goto done;
    }
    if(!check(legacy_poll(LEGACY_CLEANUP,TRUE),"actual-legacy-and-managed-owners-before-local-release"))goto done;
    if(!check((legacy_state.flags&LIVE_CONTEXT_RELEASED)&&legacy_state.legacy_absent&&legacy_state.drop_done&&legacy_state.cleanup_done,
              "actual-context-release-after-independent-legacy-permission"))goto done;
    if(!check(DestroyWindow(child),"destroy-child"))goto done;
    child=NULL; /*22*/
    if(!check(DestroyWindow(root),"destroy-root"))goto done;
    root=NULL; /*23*/
    if(!check(FreeLibrary(module),"release-driver"))goto done;
    module=NULL; /*24*/
    if(!check(UnregisterClassW(name,wc.hInstance),"unregister-class"))goto done;
    atom=0; /*25*/
done:
    if(setup_called&&fixture&&module)
    {
        if(legacy_fixture)cleanup=legacy_poll(LEGACY_CLEANUP,TRUE);
        else cleanup=poll(LIVE_CLEANUP,3);
    }
    if(cleanup)
    {
        if(child&&!DestroyWindow(child))cleanup=FALSE;
        if(root&&!DestroyWindow(root))cleanup=FALSE;
        if(module&&!FreeLibrary(module))cleanup=FALSE;
        if(atom&&!UnregisterClassW(name,wc.hInstance))cleanup=FALSE;
    }
    if(printf("{\"type\":\"host-status-legacy\",\"event\":\"summary\",\"bits\":%u,\"checks\":%u,\"expected_checks\":%u,\"failures\":%u,"
              "\"epoch\":%llu,\"lease\":%llu,\"ticket\":%llu,\"legacy_only\":%s,\"cleanup\":%s,\"io_error\":%s}\n",
              (unsigned)(8*sizeof(void *)),checks,expected,failures,(unsigned long long)epoch,(unsigned long long)lease,
              (unsigned long long)ticket,legacy_only?"true":"false",cleanup?"true":"false",io_error?"true":"false")<0||fflush(stdout)==EOF)io_error=TRUE;
    return failures||checks!=expected||!cleanup||io_error?1:0;
}
