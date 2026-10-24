/* Bounded owned-thread diagnostic under Wine, NOT a native Windows reference.
 * The external observer controls START/TERMINATE/CLEARED through one owned file.
 * A reported worker-ready state is NOT kernel-registration evidence. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

struct context { HANDLE target, ready; volatile LONG returned; };
static const char *control_path, *nonce;
static unsigned current_round;

static void die(unsigned code,const char *stage)
{
    printf("{\"type\":\"failure\",\"stage\":\"%s\",\"round\":%u,\"error\":%lu,\"nonce\":\"%s\"}\n",
           stage,current_round,GetLastError(),nonce);
    (void)fflush(stdout);
    /* Never return past storage referenced by an owned live worker. */
    TerminateProcess(GetCurrentProcess(),code);
    ExitProcess(code);
}
static void flush(void)
{
    if(fflush(stdout)||ferror(stdout))die(80,"output");
}
static int read_command(const char *expected)
{
    HANDLE file;
    char bytes[160];
    DWORD received=0;
    LARGE_INTEGER size;
    file=CreateFileA(control_path,GENERIC_READ,
                     FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                     NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    if(!GetFileSizeEx(file,&size)||size.QuadPart<=0||size.QuadPart>=(LONGLONG)sizeof(bytes))
    {
        if(!CloseHandle(file))die(81,"control-close");
        return 0;
    }
    if(!ReadFile(file,bytes,(DWORD)size.QuadPart,&received,NULL))
    {
        if(!CloseHandle(file))die(81,"control-close");
        return 0;
    }
    if(!CloseHandle(file))die(81,"control-close");
    if(received!=(DWORD)size.QuadPart||memchr(bytes,0,received))return 0;
    bytes[received]=0;
    while(received&&(bytes[received-1]=='\r'||bytes[received-1]=='\n'))bytes[--received]=0;
    return !strcmp(bytes,expected);
}
static void await_command(const char *kind)
{
    char expected[128],stop[128];
    ULONGLONG deadline=GetTickCount64()+5000;
    int size=snprintf(expected,sizeof(expected),"%s|%s|%u",nonce,kind,current_round);
    if(size<0||(size_t)size>=sizeof(expected))die(82,"command-format");
    size=snprintf(stop,sizeof(stop),"%s|STOP|%u",nonce,current_round);
    if(size<0||(size_t)size>=sizeof(stop))die(82,"stop-format");
    while(!read_command(expected))
    {
        if(read_command(stop))die(93,"observer-stop");
        if(GetTickCount64()>=deadline)die(83,"control-timeout");
        Sleep(5);
    }
}
static DWORD WINAPI wait_target(void *opaque)
{
    struct context *ctx=opaque;
    DWORD result;
    if(!SetEvent(ctx->ready))return 70;
    result=WaitForSingleObject(ctx->target,INFINITE);
    InterlockedExchange(&ctx->returned,1);
    (void)result;
    return 71; /* This worker must end through its parent's TerminateThread. */
}
int main(int argc,char **argv)
{
    struct context ctx={0};
    char *end;
    unsigned long requested;
    unsigned created=0,terminated=0,joined=0;
    if(argc!=4||!argv[1][0]||strlen(argv[2])!=32||strspn(argv[2],"0123456789abcdef")!=32)return 64;
    errno=0;requested=strtoul(argv[3],&end,10);
    if(errno||end==argv[3]||*end||!requested||requested>64)return 64;
    control_path=argv[1];nonce=argv[2];
    printf("{\"type\":\"begin\",\"mode\":\"live-hint-reap\",\"scope\":\"owned Win32 calls under Wine only\","
           "\"pointer_bits\":%u,\"pid\":%lu,\"main_tid\":%lu,\"rounds\":%lu,\"nonce\":\"%s\"}\n",
           (unsigned)(8*sizeof(void*)),GetCurrentProcessId(),GetCurrentThreadId(),requested,nonce);
    flush();
    ctx.target=CreateEventW(NULL,TRUE,FALSE,NULL);
    ctx.ready=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(!ctx.target||!ctx.ready)die(84,"create-events");
    printf("{\"type\":\"object\",\"target_handle\":%" PRIu64 ",\"nonce\":\"%s\"}\n",(uint64_t)(uintptr_t)ctx.target,nonce);
    flush();
    for(current_round=0;current_round<(unsigned)requested;current_round++)
    {
        HANDLE worker;
        DWORD tid,exit_code=0;
        ctx.returned=0;
        if(!ResetEvent(ctx.target))die(85,"reset-target");
        printf("{\"type\":\"phase\",\"stage\":\"mapped\",\"round\":%u,\"nonce\":\"%s\"}\n",current_round,nonce);flush();
        await_command("START");
        worker=CreateThread(NULL,0,wait_target,&ctx,0,&tid);
        if(!worker)die(86,"create-worker");
        created++;
        printf("{\"type\":\"phase\",\"stage\":\"created\",\"round\":%u,\"tid\":%lu,\"nonce\":\"%s\"}\n",current_round,tid,nonce);flush();
        if(WaitForSingleObject(ctx.ready,5000)!=WAIT_OBJECT_0)die(87,"worker-ready");
        printf("{\"type\":\"phase\",\"stage\":\"ready\",\"round\":%u,\"tid\":%lu,\"nonce\":\"%s\"}\n",current_round,tid,nonce);flush();
        await_command("TERMINATE");
        if(!TerminateThread(worker,42))die(88,"terminate-worker");
        terminated++;
        if(WaitForSingleObject(worker,5000)!=WAIT_OBJECT_0)die(89,"join-worker");
        joined++;
        if(!GetExitCodeThread(worker,&exit_code)||exit_code!=42||ctx.returned)die(90,"worker-exit-code");
        if(!CloseHandle(worker))die(91,"close-worker");
        printf("{\"type\":\"phase\",\"stage\":\"joined\",\"round\":%u,\"tid\":%lu,\"exit_code\":%lu,\"nonce\":\"%s\"}\n",current_round,tid,exit_code,nonce);flush();
        await_command("CLEARED");
    }
    if(!CloseHandle(ctx.ready)||!CloseHandle(ctx.target))die(92,"close-events");
    printf("{\"type\":\"complete\",\"mode\":\"live-hint-reap\",\"ok\":true,"
           "\"rounds\":%lu,\"created\":%u,\"terminated\":%u,\"joined\":%u,"
           "\"unexpected_returns\":0,\"output_error\":false,\"nonce\":\"%s\"}\n",
           requested,created,terminated,joined,nonce);
    flush();
    return 0;
}
