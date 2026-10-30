#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include "fixture.h"
#include "wine/macdrv_remote.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(x) check_value(!!(x),#x,__LINE__)
static BOOL check_value(BOOL value,const char *text,unsigned line)
{
    ++checks;
    if(!value){++failures;printf("producer-cell-fail %u line %u: %s\n",checks,line,text);fflush(stdout);}
    return value;
}
static BOOL birth_of(HANDLE process,uint64_t *birth)
{
    FILETIME c,e,k,u;
    if(!GetProcessTimes(process,&c,&e,&k,&u))return FALSE;
    *birth=(uint64_t)c.dwLowDateTime|((uint64_t)c.dwHighDateTime<<32);return TRUE;
}
static BOOL wait_before_deadline(HANDLE h,DWORD ms)
{
    ULONGLONG end=GetTickCount64()+ms;
    DWORD result=WaitForSingleObject(h,ms);
    return result==WAIT_OBJECT_0&&GetTickCount64()<end;
}
static BOOL pump_until(HANDLE h,DWORD ms)
{
    ULONGLONG end=GetTickCount64()+ms;
    for(;;)
    {
        MSG msg;ULONGLONG now=GetTickCount64();DWORD result;
        if(now>=end)return FALSE;
        result=MsgWaitForMultipleObjects(1,&h,FALSE,(DWORD)(end-now),QS_ALLINPUT);
        if(GetTickCount64()>=end)return FALSE;
        if(result==WAIT_OBJECT_0)return TRUE;
        if(result!=WAIT_OBJECT_0+1)return FALSE;
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE))
        {
            if(GetTickCount64()>=end)return FALSE;
            TranslateMessage(&msg);DispatchMessageW(&msg);
            if(GetTickCount64()>=end)return FALSE;
        }
    }
}
static BOOL close_controls(HANDLE *events,HANDLE mapping,void *view)
{
    BOOL clean=TRUE;unsigned i;
    if(view)clean=UnmapViewOfFile(view)&&clean;
    for(i=0;i<7;++i)if(events[i])clean=CloseHandle(events[i])&&clean;
    if(mapping)clean=CloseHandle(mapping)&&clean;
    return clean;
}
#include "reader.inc"
typedef NTSTATUS(WINAPI *fixture_fn)(void *);
static BOOL call(fixture_fn fn,struct producer_cell_fixture *f,unsigned op,HWND root,HWND child)
{
    f->size=sizeof(*f);f->version=1;f->operation=op;f->root=(uint32_t)(uintptr_t)root;f->child=(uint32_t)(uintptr_t)child;f->reserved=0;f->status=ERROR_INVALID_DATA;
    return fn(f)==0;
}
static BOOL poll(fixture_fn fn,struct producer_cell_fixture *f,unsigned op,HWND root,HWND child,unsigned goal)
{
    ULONGLONG end=GetTickCount64()+4000;
    for(;;)
    {
        MSG msg;BOOL ready;
        if(GetTickCount64()>=end||!call(fn,f,op,root,child)||GetTickCount64()>=end)return FALSE;
        if(goal==1)ready=f->status==0&&f->decision_seen==1;
        else if(goal==2)ready=f->status==0&&f->watched==1&&f->status_owner==1&&f->status_cleanup==1&&!f->cleanup_inflight;
        else ready=f->status==0&&!f->request_inflight&&!f->cleanup_inflight&&!f->watched&&!f->pending_models&&!f->pe_watches&&!f->pe_timers&&!f->status_owner&&!f->status_cleanup;
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
static BOOL clear_protection(fixture_fn fn,struct producer_cell_fixture *frame,HWND root,HWND child,BOOL *protected_section)
{
    ULONGLONG end=GetTickCount64()+4000;
    for(;;)
    {
        BOOL returned;
        if(GetTickCount64()>=end)return FALSE;
        returned=call(fn,frame,6,root,child);
        if(returned&&frame->status==0)
        {
            *protected_section=FALSE; /* actual mutation precedes timing acceptance */
            return GetTickCount64()<end;
        }
        if(!returned||GetTickCount64()>=end)return FALSE;
        if(frame->status!=ERROR_IO_PENDING)return FALSE;
        Sleep(10); /* retry only a known non-mutating transient detach response */
    }
}
int main(int argc,char **argv)
{
    HANDLE mapping=NULL,events[7]={0};struct producer_cell_control *control=NULL;
    PROCESS_INFORMATION peer={0};STARTUPINFOA startup={0};WNDCLASSW cls={0};ATOM atom=0;
    HINSTANCE instance=GetModuleHandleW(NULL);HWND root=NULL,child=NULL;HMODULE driver=NULL;fixture_fn fn=NULL;
    struct producer_cell_fixture frame={0},primary={0};struct macdrv_remote_packet query={0},release={0};
    struct macdrv_status_identity id={0};struct macdrv_status_values bootstrap={0};DWORD_PTR handled=0;
    char base[96],mapname[120],names[7][128],command[1800];uint64_t birth=0;unsigned i,child_failures=0;DWORD code=STILL_ACTIVE;
    BOOL clean=TRUE,done=FALSE,root_destroyed=FALSE,protected_section=FALSE,close_failure=FALSE;
    if(argc>1&&!strcmp(argv[1],"--reader"))return producer_cell_reader(argc,argv);
    if(!CHECK(argc==2||(argc==3&&!strcmp(argv[2],"--close-failure"))))goto cleanup;
    close_failure=argc==3;
    cls.lpfnWndProc=DefWindowProcW;cls.hInstance=instance;cls.lpszClassName=L"CiderProducerCellRoot";
    atom=RegisterClassW(&cls);if(!CHECK(atom!=0))goto cleanup;
    root=CreateWindowW(cls.lpszClassName,L"Cider producer cell",WS_OVERLAPPEDWINDOW|WS_VISIBLE,20,20,320,240,NULL,NULL,instance,NULL);if(!CHECK(root!=NULL))goto cleanup;
    child=CreateWindowW(cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_BORDER,10,10,120,80,root,NULL,instance,NULL);if(!CHECK(child!=NULL))goto cleanup;
    driver=LoadLibraryW(L"winemac.drv");if(!CHECK(driver!=NULL))goto cleanup;
    fn=(fixture_fn)(uintptr_t)GetProcAddress(driver,"cider_producer_cell_fixture");if(!CHECK(fn!=NULL))goto cleanup;
    query.size=sizeof(query);query.version=MACDRV_REMOTE_PACKET_VERSION;query.operation=MACDRV_REMOTE_QUERY_ROOT;query.root=(uint32_t)(uintptr_t)root;
    if(!CHECK(SendMessageTimeoutW(root,WM_MACDRV_REMOTE_HOST_PACKET,0,(LPARAM)&query,SMTO_ABORTIFHUNG,3000,&handled)&&handled==1&&!query.status&&query.flags==1&&query.root_pid==GetCurrentProcessId()&&birth_of(GetCurrentProcess(),&birth)&&birth==query.root_create_time))goto cleanup;
    if(!CHECK(call(fn,&frame,1,root,child)&&frame.status==0&&frame.request_inflight==1&&frame.status_owner==1&&frame.lease>=UINT64_C(8589934592)))goto cleanup;
    id.kind=MACDRV_STATUS_PRODUCER;id.root_pid=query.root_pid;id.root_birth=query.root_create_time;id.root_epoch=query.root_epoch;id.producer_pid=query.root_pid;id.producer_birth=query.root_create_time;id.generation=UINT64_C(8589934592);id.lease=frame.lease;
    if(!CHECK(NtQueryInformationProcess(GetCurrentProcess(),ProcessSessionInformation,&id.session_id,sizeof(id.session_id),NULL)==0))goto cleanup;
    snprintf(base,sizeof(base),"Local\\CiderProducerCell-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());snprintf(mapname,sizeof(mapname),"%s-map",base);
    mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(*control),mapname);if(!CHECK(mapping!=NULL))goto cleanup;
    control=MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*control));if(!CHECK(control!=NULL))goto cleanup;
    memset(control,0,sizeof(*control));control->magic=0x50435343;control->version=1;control->close_failure=close_failure;
    for(i=0;i<7;++i){snprintf(names[i],sizeof(names[i]),"%s-%u",base,i);events[i]=CreateEventA(NULL,TRUE,FALSE,names[i]);}
    if(!CHECK(events[0]&&events[1]&&events[2]&&events[3]&&events[4]&&events[5]&&events[6]))goto cleanup;
    startup.cb=sizeof(startup);snprintf(command,sizeof(command),"\"%s\" --reader %s %s %s %s %s %s %s %s",argv[1],mapname,names[0],names[1],names[2],names[3],names[4],names[5],names[6]);
    if(!CHECK(CreateProcessA(NULL,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&peer)))goto cleanup;
    if(!CHECK(pump_until(events[0],6000)))goto cleanup;
    CHECK(control->pid==peer.dwProcessId&&control->bits==(sizeof(void*)==8?32u:64u));
    CHECK(birth_of(peer.hProcess,&birth)&&birth==((uint64_t)control->birth_low|((uint64_t)control->birth_high<<32)));
    bootstrap.state=MACDRV_STATUS_ACTIVE;if(!CHECK(macdrv_status_initialize(&control->identity,&id,&bootstrap)))goto cleanup;
    if(close_failure){if(!CHECK(call(fn,&frame,5,root,child)&&frame.status==0))goto cleanup;protected_section=TRUE;}
    if(!CHECK(SetEvent(events[1])&&pump_until(events[2],6000)))goto cleanup;
    release.size=sizeof(release);release.version=MACDRV_REMOTE_PACKET_VERSION;release.operation=MACDRV_REMOTE_RELEASE_PRODUCER;release.root=(uint32_t)(uintptr_t)root;release.producer_pid=GetCurrentProcessId();release.producer_tid=GetCurrentThreadId();release.producer_generation=id.generation;release.root_epoch=id.root_epoch;release.lease_id=id.lease;release.sequence=frame.sequence+1;
    if(!CHECK(SendMessageTimeoutW(root,WM_MACDRV_REMOTE_HOST_PACKET,0,(LPARAM)&release,SMTO_ABORTIFHUNG,3000,&handled)&&handled==1&&release.status==0&&release.flags==1))goto cleanup;
    if(!CHECK(poll(fn,&frame,2,root,child,1)))goto cleanup;
    primary=frame;CHECK(primary.request_inflight==1&&!primary.cleanup_inflight&&!primary.pending_models&&!primary.process_open&&primary.completion_seen==1);
    if(!CHECK(SetEvent(events[3])&&pump_until(events[4],6000)))goto cleanup;
    if(!CHECK(DestroyWindow(child)))goto cleanup;
    child=NULL;
    if(!CHECK(DestroyWindow(root)))goto cleanup;
    root=NULL;root_destroyed=TRUE;control->root_destroyed=1;
    if(!CHECK(call(fn,&frame,3,root,child)&&frame.status==ERROR_PROCESS_ABORTED))goto cleanup;
    control->owner_finished=1;if(!CHECK(SetEvent(events[5])&&pump_until(events[6],6000)))goto cleanup;
    if(!CHECK(pump_until(peer.hProcess,4000)&&GetExitCodeProcess(peer.hProcess,&code)))goto cleanup;
    child_failures=control->failures;CHECK(code==(child_failures?1u:0u)&&control->reader_closed==1);
    if(close_failure)
    {
        if(!CHECK(poll(fn,&frame,7,root,child,2)))goto cleanup;
        if(!CHECK(clear_protection(fn,&frame,root,child,&protected_section)))goto cleanup;
        protected_section=FALSE;
        /* Op7 is pure: it cannot itself drain failed native work. */
        if(!CHECK(poll(fn,&frame,7,root,child,0)))goto cleanup;
    }
    if(!CHECK(poll(fn,&frame,4,root,child,0)))goto cleanup;
    done=TRUE;
cleanup:
    if(protected_section&&fn&&driver)clean=clear_protection(fn,&frame,root,child,&protected_section)&&clean;
    if(fn&&driver&&!done)clean=poll(fn,&frame,4,root,child,0)&&clean;
    if(peer.hProcess)
    {
        DWORD state=WaitForSingleObject(peer.hProcess,0);BOOL terminated=FALSE;
        if(state!=WAIT_OBJECT_0){terminated=TerminateProcess(peer.hProcess,99);state=WaitForSingleObject(peer.hProcess,3000);}
        clean=state==WAIT_OBJECT_0&&clean;
        printf("{\"type\":\"producer-cell-peer-cleanup\",\"pid\":%lu,\"wait\":%lu,\"terminate\":%u,\"confirmed\":%u}\n",peer.dwProcessId,state,(unsigned)terminated,(unsigned)(state==WAIT_OBJECT_0));
        clean=CloseHandle(peer.hThread)&&clean;clean=CloseHandle(peer.hProcess)&&clean;
    }
    if(child)clean=DestroyWindow(child)&&clean;
    if(root)clean=DestroyWindow(root)&&clean;
    if(driver)clean=FreeLibrary(driver)&&clean;
    if(atom)clean=UnregisterClassW(cls.lpszClassName,instance)&&clean;
    clean=close_controls(events,mapping,control)&&clean;
    printf("{\"type\":\"producer-cell-owner\",\"bits\":%u,\"checks\":%u,\"failures\":%u,\"child_failures\":%u,\"root_destroyed\":%u,\"done\":%u,\"cleanup\":%u,\"lease\":%llu,\"primary_watch\":%u,\"primary_finalized\":%u,\"primary_status_owner\":%u,\"close_failure\":%u}\n",(unsigned)(8*sizeof(void*)),checks,failures,child_failures,(unsigned)root_destroyed,(unsigned)done,(unsigned)clean,(unsigned long long)id.lease,primary.watched,primary.finalized,primary.status_owner,(unsigned)close_failure);fflush(stdout);
    return failures||child_failures||!done||!clean?1:0;
}
