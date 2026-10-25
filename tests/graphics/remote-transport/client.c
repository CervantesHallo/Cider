/* Original foreign-host acceptance: real USER message roundtrips and process
 * object leases only. No fake CAContext, pixels, CEF or performance claim. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wine/macdrv_remote.h"
#include "probe.h"
struct shared_result
{
    uint32_t magic, ready, checks, failures;
    struct macdrv_remote_packet last;
    uint32_t pid, mode, bits, reserved;
};
_Static_assert(sizeof(struct shared_result) == 128, "shared result width");
static unsigned checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; printf("transport-fail %u line %u: %s\n", checks, (unsigned)__LINE__, #x); fflush(stdout); } } while(0)
static void init_packet(struct macdrv_remote_packet *p, unsigned op, HWND root)
{
    memset(p, 0, sizeof(*p)); p->size=sizeof(*p);p->version=MACDRV_REMOTE_PACKET_VERSION;
    p->operation=op;p->root=(uint32_t)(uintptr_t)root;p->status=ERROR_IO_PENDING;
}
static BOOL send_packet(HWND root, struct macdrv_remote_packet *p)
{
    DWORD_PTR handled=0;
    BOOL delivered=SendMessageTimeoutW(root,WM_MACDRV_REMOTE_HOST_PACKET,0,(LPARAM)p,
                                      SMTO_ABORTIFHUNG,3000,&handled)!=0;
    return delivered && handled==1 && p->flags==MACDRV_REMOTE_PACKET_RESPONSE && macdrv_remote_packet_header_valid(p);
}
static void lease_packet(struct macdrv_remote_packet *p, unsigned op, HWND root,
                         uint64_t epoch, uint64_t gen, uint64_t lease, uint64_t seq)
{
    init_packet(p,op,root);p->root_epoch=epoch;p->producer_generation=gen;p->lease_id=lease;p->sequence=seq;
}
static int producer(int argc, char **argv)
{
    HANDLE mapping=NULL,ready=NULL,quit=NULL;
    struct shared_result *shared=NULL;
    struct macdrv_remote_packet p;
    HWND root;
    unsigned mode=0;
    uint64_t epoch=0,lease=0,generation=UINT64_C(8589934656),sequence=UINT64_C(8589934700),second=0;
    if(argc!=7)return 64;
    root=(HWND)(uintptr_t)strtoul(argv[2],NULL,16);mode=(unsigned)strtoul(argv[6],NULL,10);
    mapping=OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,argv[3]);CHECK(mapping!=NULL);
    ready=OpenEventA(EVENT_MODIFY_STATE,FALSE,argv[4]);CHECK(ready!=NULL);
    quit=OpenEventA(SYNCHRONIZE,FALSE,argv[5]);CHECK(quit!=NULL);
    if(mapping) shared=MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*shared));
    CHECK(shared!=NULL);
    if(!shared||!ready||!quit)goto done;
    init_packet(&p,MACDRV_REMOTE_QUERY_ROOT,root);CHECK(send_packet(root,&p));CHECK(p.status==ERROR_SUCCESS);
    CHECK(p.root_epoch>=UINT64_C(8589934592));CHECK(p.root_pid!=0&&p.root_create_time!=0);
    CHECK(p.producer_pid==GetCurrentProcessId()&&p.producer_tid==GetCurrentThreadId());epoch=p.root_epoch;
    lease_packet(&p,MACDRV_REMOTE_ACQUIRE_PRODUCER,root,epoch,generation,0,sequence++);
    CHECK(send_packet(root,&p));CHECK(p.status==ERROR_SUCCESS);CHECK(p.lease_id>=UINT64_C(8589934592));
    CHECK(p.producer_generation==generation);lease=p.lease_id;
    if(mode!=2)
    {
        lease_packet(&p,MACDRV_REMOTE_RELEASE_PRODUCER,root,epoch,generation,lease,sequence++);
        CHECK(send_packet(root,&p));CHECK(p.status==ERROR_SUCCESS);
    }
    if(mode==3)
    {
        lease_packet(&p,MACDRV_REMOTE_ACQUIRE_PRODUCER,root,epoch,generation,0,sequence++);
        CHECK(send_packet(root,&p));CHECK(p.status==ERROR_INVALID_HANDLE);
        lease_packet(&p,MACDRV_REMOTE_ACQUIRE_PRODUCER,root,epoch,generation+UINT64_C(4294967296),0,sequence++);
        CHECK(send_packet(root,&p));CHECK(p.status==ERROR_SUCCESS);CHECK(p.lease_id>lease);second=p.lease_id;
        lease_packet(&p,MACDRV_REMOTE_RELEASE_PRODUCER,root,epoch,generation,lease,sequence++);
        CHECK(send_packet(root,&p));CHECK(p.status==ERROR_INVALID_HANDLE);
        lease_packet(&p,MACDRV_REMOTE_ACQUIRE_PRODUCER,root,epoch,generation+UINT64_C(4294967296),0,sequence++);
        CHECK(send_packet(root,&p));CHECK(p.status==ERROR_SUCCESS&&p.lease_id==second);
        lease_packet(&p,MACDRV_REMOTE_RELEASE_PRODUCER,root,epoch,generation+UINT64_C(4294967296),second,sequence++);
        CHECK(send_packet(root,&p));CHECK(p.status==ERROR_SUCCESS);
    }
    /* Retain a known live child until the owner has observed the roundtrip. */
    lease_packet(&shared->last,MACDRV_REMOTE_ACQUIRE_PRODUCER,root,epoch,generation,lease,sequence);
    shared->pid=GetCurrentProcessId();shared->mode=mode;shared->bits=8*sizeof(void*);
    shared->checks=checks;shared->failures=failures;shared->magic=0x3152504d;
    InterlockedExchange((LONG *)&shared->ready,1);CHECK(SetEvent(ready));
    CHECK(WaitForSingleObject(quit,8000)==WAIT_OBJECT_0);
 done:
    if(shared)UnmapViewOfFile(shared);
    if(mapping)CloseHandle(mapping);
    if(ready)CloseHandle(ready);
    if(quit)CloseHandle(quit);
    printf("{\"type\":\"transport-producer\",\"bits\":%u,\"mode\":%u,\"checks\":%u,\"failures\":%u,\"lease\":%llu}\n",(unsigned)(8*sizeof(void*)),mode,checks,failures,(unsigned long long)lease);
    fflush(stdout);return failures?1:0;
}
static BOOL pump_until(HANDLE handle,DWORD timeout)
{
    ULONGLONG end=GetTickCount64()+timeout;
    for(;;)
    {
        MSG msg;ULONGLONG now=GetTickCount64();DWORD result;
        if(now>=end)return FALSE;
        result=MsgWaitForMultipleObjects(1,&handle,FALSE,(DWORD)(end-now),QS_ALLINPUT);
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
static BOOL read_probe(uint64_t target,struct transport_probe *state)
{
    typedef LONG (WINAPI *probe_fn)(struct transport_probe *);
    HMODULE driver=GetModuleHandleW(L"winemac.drv");
    probe_fn probe=driver?(probe_fn)(uintptr_t)GetProcAddress(driver,"cider_remote_transport_probe"):NULL;
    memset(state,0,sizeof(*state));state->size=sizeof(*state);state->version=2;state->target_lease=target;
    return probe&&probe(state)==0;
}
static BOOL held_lease(uint64_t target)
{
    struct transport_probe state;
    return read_probe(target,&state)&&state.target_active==1&&state.target_watched==1&&
           state.producers==1&&state.watch_pins==1&&state.pe_watches==1&&state.hosts==0;
}
static BOOL wait_idle(void)
{
    ULONGLONG end=GetTickCount64()+3000;
    for(;;)
    {
        struct transport_probe state;MSG msg;ULONGLONG now;
        if(GetTickCount64()>=end)return FALSE;
        if(!read_probe(0,&state))return FALSE;
        if(GetTickCount64()>=end)return FALSE;
        if(!state.producers&&!state.watch_pins&&!state.hosts&&!state.pe_watches&&!state.cleanup_pins)return TRUE;
        now=GetTickCount64();if(now>=end)return FALSE;
        MsgWaitForMultipleObjectsEx(0,NULL,(DWORD)((end-now)<25?(end-now):25),QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(GetTickCount64()>=end)return FALSE;
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE))
        {
            if(GetTickCount64()>=end)return FALSE;
            TranslateMessage(&msg);DispatchMessageW(&msg);
            if(GetTickCount64()>=end)return FALSE;
        }
    }
}
static void run_child(HWND root,const char *executable,unsigned mode)
{
    char prefix[96],mapname[120],readyname[120],quitname[120],command[1400];
    HANDLE mapping=NULL,ready=NULL,quit=NULL;struct shared_result *shared=NULL;
    STARTUPINFOA startup={0};PROCESS_INFORMATION child={0};DWORD code=STILL_ACTIVE;
    startup.cb=sizeof(startup);
    snprintf(prefix,sizeof(prefix),"Local\\CiderRemote-%lu-%llu-%u",GetCurrentProcessId(),(unsigned long long)GetTickCount64(),mode);
    snprintf(mapname,sizeof(mapname),"%s-map",prefix);snprintf(readyname,sizeof(readyname),"%s-ready",prefix);snprintf(quitname,sizeof(quitname),"%s-quit",prefix);
    mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(*shared),mapname);CHECK(mapping!=NULL);
    ready=CreateEventA(NULL,TRUE,FALSE,readyname);quit=CreateEventA(NULL,TRUE,FALSE,quitname);CHECK(ready&&quit);
    if(mapping) shared=MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*shared));
    CHECK(shared!=NULL);
    if(!shared||!ready||!quit)goto done;
    memset(shared,0,sizeof(*shared));
    snprintf(command,sizeof(command),"\"%s\" --producer %lx %s %s %s %u",executable,(unsigned long)(uintptr_t)root,mapname,readyname,quitname,mode);
    CHECK(CreateProcessA(NULL,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&child));if(!child.hProcess)goto done;
    CHECK(pump_until(ready,6000));CHECK(shared->magic==0x3152504d&&shared->ready==1&&shared->pid==child.dwProcessId);
    CHECK(shared->bits==(sizeof(void*)==8?32u:64u)&&shared->reserved==0);
    CHECK(shared->failures==0&&shared->checks>=13);CHECK(shared->last.lease_id>=UINT64_C(8589934592));
    CHECK(WaitForSingleObject(child.hProcess,0)==WAIT_TIMEOUT);
    if(mode==2)
    {
        CHECK(held_lease(shared->last.lease_id));
        CHECK(WaitForSingleObject(child.hProcess,0)==WAIT_TIMEOUT);
        CHECK(TerminateProcess(child.hProcess,77));
    }
    else
    {
        CHECK(wait_idle()); /* explicit release must clean up while peer is alive */
        CHECK(WaitForSingleObject(child.hProcess,0)==WAIT_TIMEOUT);
        CHECK(SetEvent(quit));
    }
    CHECK(pump_until(child.hProcess,4000));CHECK(GetExitCodeProcess(child.hProcess,&code));CHECK(code==(mode==2?77u:0u));
    CHECK(wait_idle());
    printf("{\"type\":\"transport-owner-case\",\"mode\":%u,\"producer_pid\":%lu,\"producer_bits\":%u,\"producer_checks\":%u,\"producer_failures\":%u,\"lease\":%llu,\"exit\":%lu}\n",mode,child.dwProcessId,shared->bits,shared->checks,shared->failures,(unsigned long long)shared->last.lease_id,code);
 done:
    if(child.hProcess)
    {
        DWORD before=WaitForSingleObject(child.hProcess,0),after=before;
        DWORD before_error=before==WAIT_FAILED?GetLastError():0;
        DWORD query_error=0,final_query_error=0,terminate_error=0;
        BOOL queried=GetExitCodeProcess(child.hProcess,&code),stopped=FALSE,attempted=FALSE,final_queried;
        if(!queried)query_error=GetLastError();
        if(before!=WAIT_OBJECT_0)
        {
            attempted=TRUE;stopped=TerminateProcess(child.hProcess,99);
            if(!stopped)terminate_error=GetLastError();
            (void)pump_until(child.hProcess,3000);
            after=WaitForSingleObject(child.hProcess,0);
        }
        final_queried=GetExitCodeProcess(child.hProcess,&code);
        if(!final_queried)final_query_error=GetLastError();
        /* Persist the exact cleanup receipt before checks/handle release. */
        printf("{\"type\":\"transport-child-cleanup\",\"mode\":%u,\"pid\":%lu,\"wait_before\":%lu,\"wait_before_error\":%lu,\"wait_after\":%lu,\"query_before\":%u,\"query_before_error\":%lu,\"query_after\":%u,\"query_after_error\":%lu,\"terminate_attempted\":%u,\"terminate_ok\":%u,\"terminate_error\":%lu,\"confirmed\":%u,\"exit\":%lu}\n",
               mode,child.dwProcessId,before,before_error,after,(unsigned)queried,query_error,(unsigned)final_queried,final_query_error,(unsigned)attempted,(unsigned)stopped,terminate_error,(unsigned)(after==WAIT_OBJECT_0),code);
        fflush(stdout);
        CHECK(after==WAIT_OBJECT_0);
        CHECK(CloseHandle(child.hThread));CHECK(CloseHandle(child.hProcess));
    }
    if(shared) UnmapViewOfFile(shared);
    if(mapping) CloseHandle(mapping);
    if(ready) CloseHandle(ready);
    if(quit) CloseHandle(quit);
}
int main(int argc,char **argv)
{
    HINSTANCE instance=GetModuleHandleW(NULL);WNDCLASSW cls={0};HWND root;char executable[MAX_PATH];
    if(argc>1&&!strcmp(argv[1],"--producer"))return producer(argc,argv);
    CHECK(GetModuleFileNameA(NULL,executable,sizeof(executable))!=0);
    cls.lpfnWndProc=DefWindowProcW;cls.hInstance=instance;cls.lpszClassName=L"CiderRemotePacketFixture";
    CHECK(RegisterClassW(&cls)!=0);
    root=CreateWindowW(cls.lpszClassName,L"Cider private protocol",WS_OVERLAPPEDWINDOW|WS_VISIBLE,20,20,240,160,NULL,NULL,instance,NULL);CHECK(root!=NULL);
    if(root)
    {
        unsigned mode;struct macdrv_remote_packet self;
        init_packet(&self,MACDRV_REMOTE_QUERY_ROOT,root);
        CHECK(send_packet(root,&self));CHECK(self.status==ERROR_SUCCESS);
        CHECK(self.root_pid==GetCurrentProcessId()&&self.root_epoch>=UINT64_C(8589934592));
        for(mode=1;mode<=3;++mode)run_child(root,argc>1?argv[1]:executable,mode);
        CHECK(DestroyWindow(root));
    }
    CHECK(UnregisterClassW(cls.lpszClassName,instance));
    printf("{\"type\":\"transport-owner\",\"bits\":%u,\"checks\":%u,\"failures\":%u,\"cases\":3}\n",(unsigned)(8*sizeof(void*)),checks,failures);fflush(stdout);
    return failures?1:0;
}
