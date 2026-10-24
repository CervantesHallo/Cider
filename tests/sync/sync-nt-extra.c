/* Remaining bounded N API contracts. No native Windows oracle or benchmark.
 * Pulse delivery and close-during-wait are not assigned invented guarantees. */
#define _WIN32_WINNT 0x0602
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define LIMIT 5000
static unsigned checks,failures;
static void check(const char *name,int ok,int64_t actual,int64_t expected)
{
    checks++;failures+=!ok;
    printf("{\"type\":\"check\",\"name\":\"%s\",\"ok\":%s,\"actual\":%" PRId64 ",\"expected\":%" PRId64 "}\n",name,ok?"true":"false",actual,expected);
    if(fflush(stdout)||ferror(stdout)){TerminateProcess(GetCurrentProcess(),120);ExitProcess(120);}
}
static void fatal(const char *name)
{
    check(name,0,GetLastError(),0);TerminateProcess(GetCurrentProcess(),121);ExitProcess(121);
}
static void close_one(HANDLE handle){if(!CloseHandle(handle))fatal("N.close");}
static void message_wait(void)
{
    HANDLE event=CreateEventW(NULL,FALSE,FALSE,NULL);MSG msg;DWORD status;int found=0;BOOL posted;
    ULONGLONG deadline;
    if(!event)fatal("N.MsgWait.event");
    /* Create this thread's queue, then clear only this isolated probe's queue. */
    PeekMessageW(&msg,NULL,0,0,PM_NOREMOVE);
    while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){}
    posted=PostThreadMessageW(GetCurrentThreadId(),WM_APP+0x417,42,24);
    check("N.MsgWait.post",posted!=FALSE,posted!=FALSE,1);
    deadline=GetTickCount64()+LIMIT;
    do
    {
        ULONGLONG now=GetTickCount64();
        if(now>=deadline)break;
        status=MsgWaitForMultipleObjectsEx(1,&event,(DWORD)(deadline-now),QS_POSTMESSAGE,MWMO_INPUTAVAILABLE);
        if(status==WAIT_FAILED)fatal("N.MsgWait.wait");
        if(status!=WAIT_OBJECT_0+1)break;
        /* A queue result can be a system-event wake without matching input. */
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE))
            if(msg.message==WM_APP+0x417){found=msg.wParam==42&&msg.lParam==24;}
        if(found)break;
    }while(1);
    check("N.MsgWait.message_payload",found,found,1);
    if(!SetEvent(event))fatal("N.MsgWait.signal_event");
    status=MsgWaitForMultipleObjectsEx(1,&event,0,QS_POSTMESSAGE,MWMO_INPUTAVAILABLE);
    check("N.MsgWait.event_index",status==WAIT_OBJECT_0,status,WAIT_OBJECT_0);
    status=WaitForSingleObject(event,0);
    check("N.MsgWait.auto_consumed",status==WAIT_TIMEOUT,status,WAIT_TIMEOUT);
    close_one(event);
}
struct signal_pair {HANDLE signal,reply;};
static DWORD WINAPI signal_peer(void *opaque)
{
    struct signal_pair *p=opaque;
    if(WaitForSingleObject(p->signal,LIMIT)!=WAIT_OBJECT_0)return 10;
    return SetEvent(p->reply)?0:11;
}
static void signal_wait(void)
{
    struct signal_pair p;HANDLE thread;DWORD status,code=999;
    p.signal=CreateEventW(NULL,FALSE,FALSE,NULL);p.reply=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(!p.signal||!p.reply)fatal("N.SignalWait.events");
    thread=CreateThread(NULL,0,signal_peer,&p,0,NULL);if(!thread)fatal("N.SignalWait.thread");
    status=SignalObjectAndWait(p.signal,p.reply,LIMIT,FALSE);
    check("N.SignalWait.handshake",status==WAIT_OBJECT_0,status,WAIT_OBJECT_0);
    if(WaitForSingleObject(thread,LIMIT)!=WAIT_OBJECT_0)fatal("N.SignalWait.join");
    if(!GetExitCodeThread(thread,&code))fatal("N.SignalWait.exit_api");
    check("N.SignalWait.peer_exit",code==0,code,0);
    status=WaitForSingleObject(p.reply,0);check("N.SignalWait.reply_consumed",status==WAIT_TIMEOUT,status,WAIT_TIMEOUT);
    close_one(thread);close_one(p.signal);close_one(p.reply);
}
static void pulse_state(void)
{
    HANDLE event=CreateEventW(NULL,TRUE,TRUE,NULL);DWORD status;
    if(!event)fatal("N.Pulse.event");
    status=WaitForSingleObject(event,0);
    check("N.Pulse.initial_signaled",status==WAIT_OBJECT_0,status,WAIT_OBJECT_0);
    if(!PulseEvent(event))fatal("N.Pulse.api");
    status=WaitForSingleObject(event,0);
    check("N.Pulse.final_reset_only",status==WAIT_TIMEOUT,status,WAIT_TIMEOUT);
    /* This checks final state with no waiters, not reliable pulse delivery. */
    close_one(event);
}
typedef LONG (WINAPI *nt_wait_fn)(HANDLE,BOOLEAN,PLARGE_INTEGER);
static void timeout_returns(void)
{
    HMODULE dll=GetModuleHandleW(L"ntdll.dll");FARPROC symbol=dll?GetProcAddress(dll,"NtWaitForSingleObject"):NULL;
    nt_wait_fn wait=NULL;HANDLE event;LARGE_INTEGER frequency;unsigned i;
    const LONGLONG due[]={0,-10000,-156000,-1000000};
    if(sizeof(symbol)==sizeof(wait))memcpy(&wait,&symbol,sizeof(symbol));
    check("N.Timeout.native_api",wait!=NULL,wait!=NULL,1);if(!wait)return;
    event=CreateEventW(NULL,TRUE,FALSE,NULL);if(!event)fatal("N.Timeout.event");
    if(!QueryPerformanceFrequency(&frequency)||frequency.QuadPart<=0)fatal("N.Timeout.frequency");
    for(i=0;i<4;i++)
    {
        LARGE_INTEGER deadline,before,after;LONG status;
        deadline.QuadPart=due[i];if(!QueryPerformanceCounter(&before))fatal("N.Timeout.before");
        status=wait(event,FALSE,&deadline);
        if(!QueryPerformanceCounter(&after))fatal("N.Timeout.after");
        check("N.Timeout.status",status==0x102,status,0x102);
        check("N.Timeout.nonnegative_clock",after.QuadPart>=before.QuadPart,after.QuadPart-before.QuadPart,0);
        printf("{\"type\":\"timeout_observation\",\"requested_100ns\":%" PRId64 ",\"qpc_ticks\":%" PRId64 ",\"qpc_frequency\":%" PRId64 ",\"precision_gate_applied\":false}\n",(int64_t)-due[i],(int64_t)(after.QuadPart-before.QuadPart),(int64_t)frequency.QuadPart);
    }
    close_one(event);
}
int main(void)
{
    printf("{\"type\":\"begin\",\"mode\":\"nt-extra-correctness\",\"pointer_bits\":%u,\"pid\":%lu}\n",(unsigned)(8*sizeof(void*)),GetCurrentProcessId());
    if(fflush(stdout)||ferror(stdout))return 2;
    message_wait();signal_wait();pulse_state();timeout_returns();
    printf("{\"type\":\"complete\",\"mode\":\"nt-extra-correctness\",\"ok\":%s,\"checks\":%u,\"failures\":%u,\"output_error\":%s}\n",failures?"false":"true",checks,failures,ferror(stdout)?"true":"false");
    return failures||fflush(stdout)||ferror(stdout)?1:0;
}
