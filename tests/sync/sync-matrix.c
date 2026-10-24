/* Additional planned U/S correctness cases. Not a benchmark or Windows oracle.
 * Every owned thread is joined before stack storage/locks are released. */
#include "sync-matrix.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define THREADS 4
#define ITERATIONS 5000
#define LIMIT_MS 5000
static unsigned checks,failures;
static void check(const char *name,int ok,int64_t actual,int64_t expected)
{
    checks++;failures+=!ok;
    printf("{\"type\":\"check\",\"name\":\"%s\",\"ok\":%s,\"actual\":%" PRId64 ",\"expected\":%" PRId64 "}\n",
           name,ok?"true":"false",actual,expected);
    if(fflush(stdout)||ferror(stdout)){TerminateProcess(GetCurrentProcess(),120);ExitProcess(120);}
}
static void fatal(const char *name)
{
    check(name,0,GetLastError(),0);
    TerminateProcess(GetCurrentProcess(),121);ExitProcess(121);
}
typedef BOOL (WINAPI *wait_address_fn)(volatile VOID *,PVOID,SIZE_T,DWORD);
typedef VOID (WINAPI *wake_address_fn)(PVOID);
static wait_address_fn wait_address;
static wake_address_fn wake_address;

struct group {
    HANDLE start;
    CRITICAL_SECTION cs;
    SRWLOCK srw;
    CONDITION_VARIABLE full_cv,empty_cv;
    INIT_ONCE once;
    LONG count,full,value,abort,once_count,address,ready,address_calls;
};
struct argument {struct group *group;unsigned role,kind;};
enum { CASE_CS,CASE_SRW,CASE_CV,CASE_ADDRESS,CASE_ONCE };

static DWORD remaining(ULONGLONG deadline)
{
    ULONGLONG now=GetTickCount64();
    return now>=deadline?0:(DWORD)(deadline-now);
}
static BOOL CALLBACK initialize_once(PINIT_ONCE once,PVOID opaque,PVOID *context)
{
    struct group *g=opaque;
    (void)once;
    InterlockedIncrement(&g->once_count);
    *context=(PVOID)(uintptr_t)0x1000; /* reserved context bits are zero */
    return TRUE;
}
static DWORD WINAPI worker(void *opaque)
{
    struct argument *a=opaque;struct group *g=a->group;
    unsigned i;LONG last=0;
    if(WaitForSingleObject(g->start,LIMIT_MS)!=WAIT_OBJECT_0)return 10;
    if(a->kind==CASE_CS)
    {
        for(i=0;i<ITERATIONS;i++){EnterCriticalSection(&g->cs);g->count++;LeaveCriticalSection(&g->cs);}
    }
    else if(a->kind==CASE_SRW)
    {
        for(i=0;i<ITERATIONS;i++)
        {
            if(a->role<2){AcquireSRWLockExclusive(&g->srw);g->count++;ReleaseSRWLockExclusive(&g->srw);}
            else
            {
                LONG value;
                AcquireSRWLockShared(&g->srw);value=g->count;ReleaseSRWLockShared(&g->srw);
                if(value<last||value>2*ITERATIONS)return 11;
                last=value;
            }
        }
    }
    else if(a->kind==CASE_CV)
    {
        for(i=1;i<=ITERATIONS;i++)
        {
            ULONGLONG deadline=GetTickCount64()+LIMIT_MS;
            EnterCriticalSection(&g->cs);
            while((a->role==0?g->full:!g->full)&&!g->abort)
            {
                DWORD left=remaining(deadline),error;
                if(!left){g->abort=1;break;}
                if(!SleepConditionVariableCS(a->role==0?&g->empty_cv:&g->full_cv,&g->cs,left))
                {
                    error=GetLastError();
                    if(error!=ERROR_TIMEOUT){g->abort=1;break;}
                    /* Recheck predicate even on timeout/spurious/stolen wake. */
                }
            }
            if(g->abort)
            {
                LeaveCriticalSection(&g->cs);WakeAllConditionVariable(&g->full_cv);WakeAllConditionVariable(&g->empty_cv);return 12;
            }
            if(a->role==0){g->value=(LONG)i;g->full=1;}
            else
            {
                if(g->value!=(LONG)i){g->abort=1;LeaveCriticalSection(&g->cs);WakeAllConditionVariable(&g->empty_cv);return 13;}
                g->full=0;g->count++;
            }
            LeaveCriticalSection(&g->cs);
            WakeConditionVariable(a->role==0?&g->full_cv:&g->empty_cv);
        }
    }
    else if(a->kind==CASE_ADDRESS)
    {
        LONG compare=0;ULONGLONG deadline=GetTickCount64()+LIMIT_MS;
        InterlockedIncrement(&g->ready);
        do
        {
            DWORD left=remaining(deadline);
            if(!left)return 14;
            InterlockedIncrement(&g->address_calls);
            if(!wait_address(&g->address,&compare,sizeof(compare),left)&&GetLastError()!=ERROR_TIMEOUT)return 15;
        }while(InterlockedCompareExchange(&g->address,0,0)==0);
    }
    else if(a->kind==CASE_ONCE)
    {
        for(i=0;i<ITERATIONS;i++)
        {
            PVOID context=NULL;
            if(!InitOnceExecuteOnce(&g->once,initialize_once,g,&context)||context!=(PVOID)(uintptr_t)0x1000)return 16;
        }
    }
    return 0;
}
struct held_srw {SRWLOCK lock;HANDLE held,release;int exclusive;};
static DWORD WINAPI hold_srw(void *opaque)
{
    struct held_srw *g=opaque;DWORD status=0;
    if(g->exclusive)AcquireSRWLockExclusive(&g->lock);else AcquireSRWLockShared(&g->lock);
    if(!SetEvent(g->held))status=20;
    else if(WaitForSingleObject(g->release,LIMIT_MS)!=WAIT_OBJECT_0)status=21;
    if(g->exclusive)ReleaseSRWLockExclusive(&g->lock);else ReleaseSRWLockShared(&g->lock);
    return status;
}
static void srw_exclusion(void)
{
    int exclusive;
    for(exclusive=0;exclusive<=1;exclusive++)
    {
        struct held_srw g;HANDLE thread;DWORD code=999;BOOLEAN obtained;
        memset(&g,0,sizeof(g));InitializeSRWLock(&g.lock);g.exclusive=exclusive;
        g.held=CreateEventW(NULL,TRUE,FALSE,NULL);g.release=CreateEventW(NULL,TRUE,FALSE,NULL);
        if(!g.held||!g.release)fatal("U.SRW.hold-events");
        thread=CreateThread(NULL,0,hold_srw,&g,0,NULL);if(!thread)fatal("U.SRW.hold-thread");
        if(WaitForSingleObject(g.held,LIMIT_MS)!=WAIT_OBJECT_0)fatal("U.SRW.hold-ready");
        obtained=exclusive?TryAcquireSRWLockShared(&g.lock):TryAcquireSRWLockExclusive(&g.lock);
        check(exclusive?"U.SRW.exclusive_blocks_shared":"U.SRW.shared_blocks_exclusive",!obtained,obtained,0);
        if(obtained){if(exclusive)ReleaseSRWLockShared(&g.lock);else ReleaseSRWLockExclusive(&g.lock);}
        if(!SetEvent(g.release))fatal("U.SRW.release-holder");
        if(WaitForSingleObject(thread,LIMIT_MS)!=WAIT_OBJECT_0)fatal("U.SRW.join-holder");
        if(!GetExitCodeThread(thread,&code))fatal("U.SRW.holder-status");
        check("U.SRW.holder-exit",code==0,code,0);
        obtained=exclusive?TryAcquireSRWLockShared(&g.lock):TryAcquireSRWLockExclusive(&g.lock);
        check(exclusive?"U.SRW.uncontended_shared":"U.SRW.uncontended_exclusive",obtained,obtained,1);
        if(obtained){if(exclusive)ReleaseSRWLockShared(&g.lock);else ReleaseSRWLockExclusive(&g.lock);}
        if(!CloseHandle(thread)||!CloseHandle(g.held)||!CloseHandle(g.release))fatal("U.SRW.close-holder");
    }
}
static void run_group(unsigned kind,const char *name,unsigned count)
{
    struct group g;struct argument args[THREADS];HANDLE threads[THREADS]={0};unsigned i;
    memset(&g,0,sizeof(g));InitializeCriticalSection(&g.cs);InitializeSRWLock(&g.srw);
    InitializeConditionVariable(&g.full_cv);InitializeConditionVariable(&g.empty_cv);InitOnceInitialize(&g.once);
    g.start=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(!g.start)fatal("U.create-start");
    for(i=0;i<count;i++)
    {
        args[i]=(struct argument){&g,i,kind};
        threads[i]=CreateThread(NULL,0,worker,&args[i],0,NULL);
        if(!threads[i])fatal("U.create-owned-worker");
    }
    if(!SetEvent(g.start))fatal("U.start-workers");
    if(kind==CASE_ADDRESS)
    {
        ULONGLONG deadline=GetTickCount64()+LIMIT_MS;
        while(InterlockedCompareExchange(&g.ready,0,0)!=(LONG)count)
        {
            if(!remaining(deadline))fatal("U.address-ready");
            SwitchToThread();
        }
        InterlockedExchange(&g.address,1);wake_address(&g.address);
    }
    for(i=0;i<count;i++)
    {
        DWORD code=999;
        if(WaitForSingleObject(threads[i],LIMIT_MS)!=WAIT_OBJECT_0)fatal("U.join-owned-worker");
        if(!GetExitCodeThread(threads[i],&code))fatal("U.worker-exit-code");
        check(name,code==0,code,0);
        if(!CloseHandle(threads[i]))fatal("U.close-worker");
    }
    if(kind==CASE_CS)check("U.CS.final",g.count==(LONG)(count*ITERATIONS),g.count,count*ITERATIONS);
    if(kind==CASE_SRW)check("U.SRW.final",g.count==2*ITERATIONS,g.count,2*ITERATIONS);
    if(kind==CASE_CV)check("U.CV.final",g.count==ITERATIONS&&!g.full&&!g.abort,g.count,ITERATIONS);
    if(kind==CASE_ADDRESS)check("U.WaitOnAddress.actual-calls",g.address_calls>=(LONG)count,g.address_calls,count);
    if(kind==CASE_ONCE)check("U.InitOnce.exactly-one",g.once_count==1,g.once_count,1);
    if(!CloseHandle(g.start))fatal("U.close-start");
    DeleteCriticalSection(&g.cs);
}
int main(void)
{
    HMODULE base=GetModuleHandleW(L"kernelbase.dll");FARPROC address;
    printf("{\"type\":\"begin\",\"mode\":\"sync-matrix-correctness\",\"pointer_bits\":%u,\"pid\":%lu,\"scope\":\"additional U/S correctness only, no benchmark or Windows reference\"}\n",(unsigned)(8*sizeof(void*)),GetCurrentProcessId());
    if(fflush(stdout)||ferror(stdout))return 2;
    address=base?GetProcAddress(base,"WaitOnAddress"):NULL;
    if(sizeof(address)==sizeof(wait_address))memcpy(&wait_address,&address,sizeof(address));
    address=base?GetProcAddress(base,"WakeByAddressAll"):NULL;
    if(sizeof(address)==sizeof(wake_address))memcpy(&wake_address,&address,sizeof(address));
    run_group(CASE_CS,"U.CS.worker",THREADS);run_group(CASE_SRW,"U.SRW.worker",THREADS);
    srw_exclusion();
    run_group(CASE_CV,"U.CV.worker",2);
    check("U.WaitOnAddress.available",wait_address&&wake_address,wait_address&&wake_address,1);
    if(wait_address&&wake_address)run_group(CASE_ADDRESS,"U.WaitOnAddress.worker",THREADS);
    run_group(CASE_ONCE,"U.InitOnce.worker",THREADS);
    matrix_server_cases(check,LIMIT_MS);
    printf("{\"type\":\"complete\",\"mode\":\"sync-matrix-correctness\",\"ok\":%s,\"checks\":%u,\"failures\":%u,\"output_error\":%s}\n",failures?"false":"true",checks,failures,ferror(stdout)?"true":"false");
    return failures||fflush(stdout)||ferror(stdout)?1:0;
}
