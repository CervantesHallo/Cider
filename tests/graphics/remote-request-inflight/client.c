#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "fixture.h"
#include "wine/macdrv_remote.h"
typedef LONG(WINAPI *fixture_fn)(void *);
static unsigned checks,failures;
#define CHECK(x) check_value(!!(x),#x,__LINE__)
static BOOL check_value(BOOL value,const char *text,unsigned line)
{
    ++checks;
    if(!value)++failures;
    printf("{\"type\":\"request-pin-check\",\"check\":%u,\"ok\":%u,\"line\":%u}\n",checks,(unsigned)value,line);
    if(!value)printf("request-pin-fail %s\n",text);
    fflush(stdout);return value;
}
static LONG call(fixture_fn fn,struct request_fixture *f,unsigned op,HWND root,HWND child)
{
    f->size=sizeof(*f);f->version=2;f->operation=op;f->status=ERROR_INVALID_DATA;
    f->root=(uint32_t)(uintptr_t)root;f->child=(uint32_t)(uintptr_t)child;f->reserved=0;
    return fn(f);
}
static BOOL poll(fixture_fn fn,struct request_fixture *f,unsigned op,HWND root,HWND child,BOOL completion)
{
    ULONGLONG end=GetTickCount64()+3000;
    for(;;)
    {
        MSG msg;BOOL ready;
        if(GetTickCount64()>=end||call(fn,f,op,root,child)!=0||GetTickCount64()>=end)return FALSE;
        if(completion)ready=f->status==0&&f->completion_seen==1&&f->decision_seen==1&&f->cleanup_inflight==0&&f->process_open==0;
        else ready=f->status==0&&!f->request_inflight&&!f->cleanup_inflight&&!f->watched&&!f->pending_models&&!f->pe_watches&&!f->pe_timers;
        if(GetTickCount64()>=end)return FALSE;
        if(ready)return TRUE;
        if(f->status!=0&&f->status!=ERROR_IO_PENDING)return FALSE;
        MsgWaitForMultipleObjectsEx(0,NULL,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(GetTickCount64()>=end)return FALSE;
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE))
        {
            if(GetTickCount64()>=end)return FALSE;
            TranslateMessage(&msg);DispatchMessageW(&msg);
            if(GetTickCount64()>=end)return FALSE;
        }
    }
}
int main(int argc,char **argv)
{
    HINSTANCE instance=GetModuleHandleW(NULL);WNDCLASSW cls={0};ATOM atom=0;
    HWND root=NULL,child=NULL;HMODULE driver=NULL;fixture_fn fn=NULL;struct request_fixture f={0},primary={0};
    struct macdrv_remote_packet packet={0};DWORD_PTR handled=0;BOOL clean=TRUE,primary_seen=FALSE,finished=FALSE;
    (void)argv;
    if(!CHECK(argc==1))goto done;
    cls.hInstance=instance;cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=L"CiderRequestPinRealRoot";
    atom=RegisterClassW(&cls);if(!CHECK(atom!=0))goto done;
    root=CreateWindowW(cls.lpszClassName,L"Cider request pin",WS_OVERLAPPEDWINDOW|WS_VISIBLE,20,20,320,240,NULL,NULL,instance,NULL);if(!CHECK(root!=NULL))goto done;
    child=CreateWindowW(cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_BORDER,10,10,120,80,root,NULL,instance,NULL);if(!CHECK(child!=NULL))goto done;
    driver=LoadLibraryW(L"winemac.drv");if(!CHECK(driver!=NULL))goto done;
    fn=(fixture_fn)(uintptr_t)GetProcAddress(driver,"cider_request_fixture");if(!CHECK(fn!=NULL))goto done;
    packet.size=sizeof(packet);packet.version=MACDRV_REMOTE_PACKET_VERSION;packet.operation=MACDRV_REMOTE_QUERY_ROOT;packet.root=(uint32_t)(uintptr_t)root;
    if(!CHECK(SendMessageTimeoutW(root,WM_MACDRV_REMOTE_HOST_PACKET,0,(LPARAM)&packet,SMTO_ABORTIFHUNG,3000,&handled)&&handled==1&&packet.status==0&&packet.flags==1&&packet.root_epoch>=UINT64_C(8589934592)&&packet.root_pid==GetCurrentProcessId()&&packet.root_create_time!=0))goto done;
    if(!CHECK(call(fn,&f,1,root,child)==0&&f.status==0&&f.request_inflight==1&&f.process_open==1&&f.watched==1&&!f.finalized&&f.lease>=UINT64_C(8589934592)))goto done;
    packet.operation=MACDRV_REMOTE_RELEASE_PRODUCER;packet.producer_pid=GetCurrentProcessId();packet.producer_tid=GetCurrentThreadId();packet.producer_generation=UINT64_C(8589934592);packet.lease_id=f.lease;packet.sequence=f.sequence+1;packet.root_pid=0;packet.root_create_time=0;packet.flags=0;packet.status=ERROR_IO_PENDING;
    if(!CHECK(SendMessageTimeoutW(root,WM_MACDRV_REMOTE_HOST_PACKET,0,(LPARAM)&packet,SMTO_ABORTIFHUNG,3000,&handled)&&handled==1&&packet.status==0&&packet.flags==1))goto done;
    if(!CHECK(poll(fn,&f,2,root,child,TRUE)))goto done;
    primary=f;primary_seen=TRUE;
    CHECK(f.request_inflight==1&&f.cleanup_inflight==0&&f.pending_models==0&&f.process_open==0);
    CHECK(f.watched==1&&f.finalized==0);
    CHECK(f.lease>=UINT64_C(8589934592)&&f.sequence>=UINT64_C(8589934592)&&f.completion_seen==1);
    CHECK(call(fn,&f,3,root,child)==0&&f.status==ERROR_PROCESS_ABORTED&&f.request_inflight==0);
    if(!CHECK(poll(fn,&f,4,root,child,FALSE)))goto done;
    finished=TRUE;
    if(!CHECK(DestroyWindow(child)))goto done;
    child=NULL;
    if(!CHECK(DestroyWindow(root)))goto done;
    root=NULL;
    if(!CHECK(FreeLibrary(driver)))goto done;
    driver=NULL;
    if(!CHECK(UnregisterClassW(cls.lpszClassName,instance)))goto done;
    atom=0;
    CHECK(f.pe_watches==0&&f.pe_timers==0&&f.watched==0&&f.request_inflight==0&&f.cleanup_inflight==0&&f.pending_models==0);
done:
    if(fn&&driver&&!finished)clean=poll(fn,&f,4,root,child,FALSE)&&clean;
    if(child)clean=DestroyWindow(child)&&clean;
    if(root)clean=DestroyWindow(root)&&clean;
    if(driver)clean=FreeLibrary(driver)&&clean;
    if(atom)clean=UnregisterClassW(cls.lpszClassName,instance)&&clean;
    printf("{\"type\":\"request-pin-summary\",\"bits\":%u,\"checks\":%u,\"failures\":%u,\"primary_seen\":%u,\"lease\":%llu,\"sequence\":%llu,\"request_inflight\":%u,\"cleanup_inflight\":%u,\"pending_models\":%u,\"process_open\":%u,\"watched\":%u,\"finalized\":%u,\"completion_seen\":%u,\"decision_seen\":%u,\"decision_detached\":%u,\"cleanup\":%u,\"finished\":%u}\n",(unsigned)(8*sizeof(void*)),checks,failures,(unsigned)primary_seen,(unsigned long long)primary.lease,(unsigned long long)primary.sequence,primary.request_inflight,primary.cleanup_inflight,primary.pending_models,primary.process_open,primary.watched,primary.finalized,primary.completion_seen,primary.decision_seen,primary.decision_detached,(unsigned)clean,(unsigned)finished);fflush(stdout);
    return failures||!clean||!finished?1:0;
}
