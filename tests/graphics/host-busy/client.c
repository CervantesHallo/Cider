#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include "fixture.h"
#include "generation.h"
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
    if(printf("{\"type\":\"host-busy\",\"event\":\"check\",\"check\":%u,\"name\":\"%s\",\"ok\":%s}\n",checks,name,value?"true":"false")<0||fflush(stdout)==EOF)io_error=TRUE;
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
    if(printf("{\"type\":\"host-busy\",\"event\":\"snapshot\",\"snapshot\":%u,\"op\":%u,\"ntstatus\":%u,\"status\":%u,"
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
static fixture_fn generation_fixture;
static struct generation_frame generation_state;
static BOOL generation_operation(unsigned op,LONGLONG start)
{
    NTSTATUS status;LONGLONG end;UINT32 words[64];unsigned i;
    memset(&generation_state,0,sizeof(generation_state));generation_state.size=sizeof(generation_state);
    generation_state.version=1;generation_state.operation=op;generation_state.root=root_value;generation_state.child=child_value;
    status=generation_fixture(&generation_state);memcpy(words,&generation_state,sizeof(words));
    if(printf("{\"type\":\"host-busy\",\"event\":\"generation\",\"ntstatus\":%u,\"words\":[",(unsigned)(UINT32)status)<0)io_error=TRUE;
    for(i=0;i<64;++i)if(printf("%s%u",i?",":"",words[i])<0)io_error=TRUE;
    if(printf("]}\n")<0||fflush(stdout)==EOF)io_error=TRUE;
    return now(&end)&&end>=start&&end-start<4*frequency&&!status&&generation_state.size==sizeof(generation_state)&&
           generation_state.version==1&&generation_state.operation==op;
}
static BOOL generation_once(unsigned op)
{LONGLONG start;return now(&start)&&generation_operation(op,start)&&!generation_state.status;}
static BOOL generation_cleanup(void)
{
    LONGLONG start,end;if(!now(&start))return FALSE;
    for(;;)
    {
        if(!generation_operation(GENERATION_CLEANUP,start)||(generation_state.status&&generation_state.status!=ERROR_IO_PENDING))return FALSE;
        if(!generation_state.status&&generation_state.cleanup_done&&!generation_state.context_owned&&!generation_state.window_owned&&
           !generation_state.producer_reader_owned&&!generation_state.close_status&&
           (!generation_state.owner_mask||(generation_state.done_mask==3&&!generation_state.busy_mask&&generation_state.absent&&
             generation_state.callbacks_old&&generation_state.callbacks_new)))return TRUE;
        Sleep(2);if(!now(&end)||end<start||end-start>=4*frequency)return FALSE;
    }
}
int main(int argc,char **argv)
{
    const WCHAR *name=L"CiderBusyGenerationV1";
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
    generation_fixture=(fixture_fn)(uintptr_t)GetProcAddress(module,"cider_generation_fixture");
    if(!check(generation_fixture!=NULL,"generation-export"))goto done;
    if(!check(generation_once(GENERATION_RUN),"actual-outer-and-nested-cocoa-creation"))goto done;
    if(!check(generation_state.outer_called&&generation_state.outer_result&&generation_state.inner_called&&!generation_state.inner_result&&generation_state.hook_seen==1&&generation_state.hook_main&&!generation_state.hook_error,"actual-older-selected-newer-rejected-while-busy"))goto done;
    if(!check(generation_state.before_old_attached==1&&generation_state.before_old_ledger==1&&generation_state.before_old_indexed==1,"actual-older-selected-and-pinned-before-reentry"))goto done;
    if(!check(generation_state.after_old_attached==1&&generation_state.after_old_ledger==1&&generation_state.after_old_indexed==1&&!generation_state.after_new_attached&&!generation_state.after_new_ledger&&!generation_state.after_new_indexed,"actual-busy-predecessor-kept-newer-rollback-complete"))goto done;
    if(!check(generation_state.cocoa_valid&&!generation_state.final_old_attached&&!generation_state.final_old_ledger&&!generation_state.final_old_indexed&&!generation_state.final_new_attached&&!generation_state.final_new_ledger&&!generation_state.final_new_indexed&&generation_state.context_owned,"last-use-removes-retirement-requested-predecessor"))goto done;
    if(!check(generation_state.old_generation>UINT32_MAX&&generation_state.new_generation==generation_state.old_generation+1&&generation_state.owner_mask==3&&generation_state.hook_users==1&&!generation_state.retire_before&&generation_state.retire_after,"actual-use-pin-and-retirement-request-with-caller-labels"))goto done;
    if(!check(now(&start)&&operation(LIVE_RELEASE_PRODUCER,start)&&(frame.status==0||frame.status==ERROR_IO_PENDING),"ordinary-producer-release"))goto done;
    if(!check(poll(LIVE_SNAPSHOT,2),"actual-producer-safe"))goto done;
    if(!check(generation_once(GENERATION_SNAPSHOT)&&generation_state.producer_read==MACDRV_STATUS_READ_SAFE_ONLY&&!generation_state.producer_valid&&generation_state.context_owned&&!generation_state.final_new_attached&&!generation_state.final_new_ledger&&!generation_state.final_new_indexed,"original-context-still-held-after-models-and-producer-complete"))goto done;
    if(!check(generation_cleanup(),"actual-both-generation-retirements-before-local-release"))goto done;
    if(!check((generation_state.flags&LIVE_CONTEXT_RELEASED)&&(generation_state.flags&64)&&generation_state.done_mask==3&&generation_state.absent&&generation_state.callbacks_old&&generation_state.callbacks_new&&generation_state.cleanup_done,"actual-absence-disarm-and-context-release"))goto done;
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
        if(generation_fixture)cleanup=generation_cleanup();
        else cleanup=poll(LIVE_CLEANUP,3);
    }
    if(cleanup)
    {
        if(child&&!DestroyWindow(child))cleanup=FALSE;
        if(root&&!DestroyWindow(root))cleanup=FALSE;
        if(module&&!FreeLibrary(module))cleanup=FALSE;
        if(atom&&!UnregisterClassW(name,wc.hInstance))cleanup=FALSE;
    }
    if(printf("{\"type\":\"host-busy\",\"event\":\"summary\",\"bits\":%u,\"checks\":%u,\"expected_checks\":25,\"failures\":%u,"
              "\"epoch\":%llu,\"lease\":%llu,\"ticket\":%llu,\"cleanup\":%s,\"io_error\":%s}\n",
              (unsigned)(8*sizeof(void *)),checks,failures,(unsigned long long)epoch,(unsigned long long)lease,
              (unsigned long long)ticket,cleanup?"true":"false",io_error?"true":"false")<0||fflush(stdout)==EOF)io_error=TRUE;
    return failures||checks!=25||!cleanup||io_error?1:0;
}
