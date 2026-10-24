/* New-base GS/FS ABI regression, not a test of private host QoS or all signals.
 * Only switches distinct, valid fibers; no reads of GetCurrentFiber before
 * conversion/after conversion back. Uses API setters and read-only raw slots.
 * Mandatory owned-prefix cleanup is provided by run-mac.py --suite gs-boundary. */
#define _WIN32_WINNT 0x0602
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

static volatile LONG checks, failures;
static DWORD tls_index;
#define EXCEPTION_OWN 0xe0424349u
struct context {const char *role;DWORD tid;void *main_fiber,*child_fiber;volatile LONG child_seen,window_seen,exception_seen,apc_seen;};
static void fatal(void){TerminateProcess(GetCurrentProcess(),121);ExitProcess(121);}
static void check(const char *role,const char *name,int ok,uint64_t actual,uint64_t expected)
{
    InterlockedIncrement(&checks);if(!ok)InterlockedIncrement(&failures);
    printf("{\"type\":\"check\",\"role\":\"%s\",\"name\":\"%s\",\"ok\":%s,\"actual\":%" PRIu64 ",\"expected\":%" PRIu64 "}\n",role,name,ok?"true":"false",actual,expected);
    if(fflush(stdout)||ferror(stdout))fatal();
}
static void named(struct context *c,const char *stage,const char *part,int ok,uint64_t actual,uint64_t expected)
{
    char name[80];int n=snprintf(name,sizeof(name),"%s.%s",stage,part);if(n<0||(unsigned)n>=sizeof(name))fatal();
    check(c->role,name,ok,actual,expected);
}
static void *raw_fiber(void)
{
    void *p;
#if defined(__x86_64__)
    __asm__ volatile("movq %%gs:0x20,%0":"=r"(p));
#elif defined(__i386__)
    __asm__ volatile("movl %%fs:0x10,%0":"=r"(p));
#else
#error This probe currently covers only PE x86 and x64.
#endif
    return p;
}
static DWORD raw_error(void)
{
    DWORD value;
#ifdef __x86_64__
    __asm__ volatile("movl %%gs:0x68,%0":"=r"(value));
#else
    __asm__ volatile("movl %%fs:0x34,%0":"=r"(value));
#endif
    return value;
}
static void fiber_state(struct context *c,const char *stage,void *expected)
{
    void *api=GetCurrentFiber(),*slot=raw_fiber();
    unsigned offset=sizeof(void*)==8?0x20:0x10;
    void *teb=*(void**)((unsigned char*)NtCurrentTeb()+offset);
    void *data;
    named(c,stage,"fiber_api",api==expected,(uintptr_t)api,(uintptr_t)expected);
    named(c,stage,"fiber_slot",slot==expected,(uintptr_t)slot,(uintptr_t)expected);
    named(c,stage,"fiber_teb",teb==expected,(uintptr_t)teb,(uintptr_t)expected);
    /* GetFiberData dereferences the slot. Reject a bad slot before using it. */
    if(!expected||api!=expected||slot!=expected||teb!=expected)fatal();
    data=GetFiberData();
    named(c,stage,"fiber_data",data==c,(uintptr_t)data,(uintptr_t)c);
}
static void callback_thread(struct context *c,const char *name)
{
    DWORD actual=GetCurrentThreadId();
    check(c->role,name,actual==c->tid,actual,c->tid);
    /* GetCurrentFiber has no contract on an unconverted foreign thread. */
    if(actual!=c->tid)fatal();
}
static void error_state(struct context *c,const char *stage,DWORD expected)
{
    DWORD api=GetLastError(),slot=raw_error();unsigned offset=sizeof(void*)==8?0x68:0x34;
    DWORD teb=*(DWORD*)((unsigned char*)NtCurrentTeb()+offset);
    named(c,stage,"api",api==expected,api,expected);
    named(c,stage,"slot",slot==expected,slot,expected);
    named(c,stage,"teb",teb==expected,teb,expected);
}
static VOID WINAPI fiber_entry(void *opaque)
{
    struct context *c=opaque;
    callback_thread(c,"child.thread");
    fiber_state(c,"child",c->child_fiber);
    SetLastError(0x2468);error_state(c,"child_error",0x2468);
    c->child_seen=1;SwitchToFiber(c->main_fiber);
    fatal(); /* Main deletes this suspended child; it must never be resumed. */
}
static LRESULT CALLBACK window_proc(HWND window,UINT msg,WPARAM wp,LPARAM lp)
{
    if(msg==WM_NCCREATE)
    {
        struct context *c=((CREATESTRUCTW*)lp)->lpCreateParams;
        if(!c)fatal();
        callback_thread(c,"window.thread");
        fiber_state(c,"window",c->main_fiber);
        SetLastError(0x3579);error_state(c,"window_error",0x3579);c->window_seen=1;
    }
    return DefWindowProcW(window,msg,wp,lp);
}
static LONG CALLBACK exception_handler(EXCEPTION_POINTERS *p)
{
    struct context *c;
    if(p->ExceptionRecord->ExceptionCode!=EXCEPTION_OWN)return EXCEPTION_CONTINUE_SEARCH;
    c=TlsGetValue(tls_index);if(!c)return EXCEPTION_CONTINUE_SEARCH;
    callback_thread(c,"exception.thread");
    fiber_state(c,"exception",c->main_fiber);
    SetLastError(0x468a);error_state(c,"exception_error",0x468a);c->exception_seen=1;
    return EXCEPTION_CONTINUE_EXECUTION;
}
static VOID CALLBACK apc_entry(ULONG_PTR opaque)
{
    struct context *c=(void*)opaque;
    callback_thread(c,"apc.thread");
    fiber_state(c,"apc",c->main_fiber);
    SetLastError(0x579b);error_state(c,"apc_error",0x579b);c->apc_seen=1;
}
static void extra_boundaries(struct context *c)
{
    WNDCLASSW wc={0};ATOM atom;HWND window;HINSTANCE instance=GetModuleHandleW(NULL);void *veh;DWORD queued;
    wc.lpfnWndProc=window_proc;wc.hInstance=instance;wc.lpszClassName=L"Cider.GSBoundary";
    atom=RegisterClassW(&wc);check(c->role,"window.register",atom!=0,atom!=0,1);if(!atom)fatal();
    window=CreateWindowW(wc.lpszClassName,L"Cider ABI probe",WS_OVERLAPPEDWINDOW,0,0,128,128,NULL,NULL,instance,c);
    check(c->role,"window.create",window!=NULL,window!=NULL,1);if(!window)fatal();
    check(c->role,"window.seen",c->window_seen==1,c->window_seen,1);
    if(!DestroyWindow(window)){check(c->role,"window.destroy",0,0,1);fatal();}
    check(c->role,"window.destroy",1,1,1);
    if(!UnregisterClassW(wc.lpszClassName,instance)){check(c->role,"window.unregister",0,0,1);fatal();}
    check(c->role,"window.unregister",1,1,1);
    veh=AddVectoredExceptionHandler(1,exception_handler);check(c->role,"exception.add",veh!=NULL,veh!=NULL,1);if(!veh)fatal();
    RaiseException(EXCEPTION_OWN,0,0,NULL);
    check(c->role,"exception.seen",c->exception_seen==1,c->exception_seen,1);
    if(!RemoveVectoredExceptionHandler(veh)){check(c->role,"exception.remove",0,0,1);fatal();}
    check(c->role,"exception.remove",1,1,1);
    queued=QueueUserAPC(apc_entry,GetCurrentThread(),(ULONG_PTR)c);check(c->role,"apc.queue",queued!=0,queued!=0,1);if(!queued)fatal();
    {
        ULONGLONG deadline=GetTickCount64()+5000;
        while(!c->apc_seen)
        {
            ULONGLONG now=GetTickCount64();if(now>=deadline)fatal();
            SleepEx((DWORD)(deadline-now),TRUE);
        }
    }
    check(c->role,"apc.seen",c->apc_seen==1,c->apc_seen,1);
}
static DWORD run_case(const char *role,int extra)
{
    struct context c={0};HANDLE file;c.role=role;c.tid=GetCurrentThreadId();
    if(!TlsSetValue(tls_index,&c)){check(role,"tls.set",0,0,1);fatal();}
    check(role,"tls.set",1,1,1);
    check(role,"initial_plain_thread",!IsThreadAFiber(),IsThreadAFiber(),0);if(IsThreadAFiber())fatal();
    c.main_fiber=ConvertThreadToFiberEx(&c,0);check(role,"convert",c.main_fiber!=NULL,c.main_fiber!=NULL,1);if(!c.main_fiber)fatal();
    fiber_state(&c,"main",c.main_fiber);
    SetLastError(0x1357);error_state(&c,"set_error",0x1357);
    Sleep(0);error_state(&c,"after_sleep",GetLastError()); /* Consistency, not an undocumented preservation promise. */
    file=CreateFileW(L"C:\\CiderTests\\cider-abi-definitely-missing.none",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(file!=INVALID_HANDLE_VALUE){CloseHandle(file);check(role,"missing.handle",0,0,1);fatal();}
    {
        DWORD error=GetLastError();error_state(&c,"missing_error",error);
        check(role,"missing.handle",error==ERROR_FILE_NOT_FOUND,error,ERROR_FILE_NOT_FOUND);
    }
    c.child_fiber=CreateFiber(0,fiber_entry,&c);check(role,"fiber.create",c.child_fiber!=NULL,c.child_fiber!=NULL,1);if(!c.child_fiber)fatal();
    SwitchToFiber(c.child_fiber);
    check(role,"child.seen",c.child_seen==1,c.child_seen,1);fiber_state(&c,"restored",c.main_fiber);DeleteFiber(c.child_fiber);
    if(extra)extra_boundaries(&c);
    if(!ConvertFiberToThread()){check(role,"convert_back",0,0,1);fatal();}
    check(role,"convert_back",1,1,1);check(role,"plain_after_convert",!IsThreadAFiber(),IsThreadAFiber(),0);
    if(!TlsSetValue(tls_index,NULL)){check(role,"tls.clear",0,0,1);fatal();}
    check(role,"tls.clear",1,1,1);return 0;
}
static DWORD WINAPI worker(void *opaque){(void)opaque;return run_case("worker",0);}
int main(void)
{
    HANDLE thread;DWORD status,code=999;BOOL freed;
    printf("{\"type\":\"begin\",\"mode\":\"correctness\",\"subject\":\"gs-boundary\",\"pointer_bits\":%u,\"pid\":%lu}\n",(unsigned)(8*sizeof(void*)),GetCurrentProcessId());
    if(fflush(stdout)||ferror(stdout))fatal();
    tls_index=TlsAlloc();check("outer","tls.alloc",tls_index!=TLS_OUT_OF_INDEXES,tls_index!=TLS_OUT_OF_INDEXES,1);if(tls_index==TLS_OUT_OF_INDEXES)fatal();
    run_case("main",1);
    thread=CreateThread(NULL,0,worker,NULL,0,NULL);if(!thread){check("outer","worker.create",0,0,1);fatal();}
    status=WaitForSingleObject(thread,5000);if(status!=WAIT_OBJECT_0){check("outer","worker.join",0,status,WAIT_OBJECT_0);fatal();}
    check("outer","worker.create",1,1,1);check("outer","worker.join",1,status,WAIT_OBJECT_0);
    if(!GetExitCodeThread(thread,&code))fatal();
    check("outer","worker.exit",code==0,code,0);
    if(!CloseHandle(thread))fatal();
    check("outer","worker.close",1,1,1);
    freed=TlsFree(tls_index);
    check("outer","tls.free",freed!=FALSE,freed!=FALSE,1);
    printf("{\"type\":\"complete\",\"mode\":\"correctness\",\"ok\":%s,\"checks\":%ld,\"failures\":%ld,\"output_error\":%s}\n",failures?"false":"true",checks,failures,ferror(stdout)?"true":"false");
    return failures||fflush(stdout)||ferror(stdout)?1:0;
}
