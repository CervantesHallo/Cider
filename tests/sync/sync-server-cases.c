/* Bounded S cases for the original U/N/S matrix; no standalone entry point.
 * Source contracts and evidence limits: out/sync-matrix-20261024/server-source-notes.json.
 * All mixed waits use supported timer/event handles, never an IOCP or pipe handle.
 */
#include "sync-matrix.h"
#include <stdio.h>
#include <string.h>

static int s_equal(matrix_check_fn check,const char *scope,const char *step,
                   int64_t actual,int64_t expected)
{
    char name[160];
    snprintf(name,sizeof(name),"%s.%s",scope,step);
    check(name,actual==expected,actual,expected);
    return actual==expected;
}

/* Check the actual API result and preserve its error on failure. Call before
 * any callback can change LastError; even FALSE + error zero must fail. */
static int s_api(matrix_check_fn check,const char *scope,const char *step,BOOL ok)
{
    DWORD status=ok ? ERROR_SUCCESS : GetLastError();
    s_equal(check,scope,step,ok!=FALSE,TRUE);
    if (!ok) {
        char error_step[120];
        snprintf(error_step,sizeof(error_step),"%s.error",step);
        s_equal(check,scope,error_step,status,ERROR_SUCCESS);
    }
    return ok!=FALSE;
}

static void s_close(matrix_check_fn check,const char *scope,const char *step,HANDLE h)
{
    if (h && h!=INVALID_HANDLE_VALUE) s_api(check,scope,step,CloseHandle(h));
}

static void s_timer(matrix_check_fn check,DWORD timeout_ms,BOOL manual)
{
    const char *scope=manual ? "S.timer.manual" : "S.timer.synchronization";
    HANDLE timer=NULL,ordinary=NULL,handles[2];
    LARGE_INTEGER due;
    DWORD result,after_wait=manual ? WAIT_OBJECT_0+1u : WAIT_TIMEOUT;

    timer=CreateWaitableTimerW(NULL,manual,NULL);
    if (!s_api(check,scope,"create_timer",timer!=NULL)) goto done;
    ordinary=CreateEventW(NULL,FALSE,FALSE,NULL);
    if (!s_api(check,scope,"create_ordinary_event",ordinary!=NULL)) goto done;
    handles[0]=ordinary;
    handles[1]=timer;
    result=WaitForMultipleObjects(2,handles,FALSE,0);
    s_equal(check,scope,"initial_mixed_timeout",result,WAIT_TIMEOUT);

    /* Relative 10 ms, one shot, no APC and no request to wake the computer.
     * Assert eventual signaling within the caller's bound, not timer precision. */
    due.QuadPart=-100000;
    if (!s_api(check,scope,"arm",SetWaitableTimer(timer,&due,0,NULL,NULL,FALSE)))
        goto done;
    result=WaitForMultipleObjects(2,handles,FALSE,timeout_ms);
    if (!s_equal(check,scope,"mixed_timer_index",result,WAIT_OBJECT_0+1u)) goto done;
    result=WaitForSingleObject(ordinary,0);
    s_equal(check,scope,"ordinary_still_unsignaled",result,WAIT_TIMEOUT);
    result=WaitForMultipleObjects(2,handles,FALSE,0);
    s_equal(check,scope,"timer_reset_semantics",result,after_wait);

    if (!s_api(check,scope,"signal_ordinary",SetEvent(ordinary))) goto done;
    result=WaitForMultipleObjects(2,handles,FALSE,0);
    s_equal(check,scope,"ordinary_index_and_lowest_ready_index",result,WAIT_OBJECT_0);
    result=WaitForMultipleObjects(2,handles,FALSE,0);
    s_equal(check,scope,"ordinary_consumed_timer_unchanged",result,after_wait);
    if (!s_api(check,scope,"cancel",CancelWaitableTimer(timer))) goto done;
    result=WaitForMultipleObjects(2,handles,FALSE,0);
    s_equal(check,scope,"cancel_does_not_reset_signaled_state",result,after_wait);
done:
    /* No completion routine was registered, so closing an armed timer cannot
     * leave a callback referencing this frame. No wait is outstanding here. */
    s_close(check,scope,"close_ordinary",ordinary);
    s_close(check,scope,"close_timer",timer);
}

static void s_iocp_empty(matrix_check_fn check,HANDLE port,const char *scope)
{
    OVERLAPPED sentinel={0},*overlapped=&sentinel;
    DWORD bytes=0;
    ULONG_PTR key=0;
    BOOL ok=GetQueuedCompletionStatus(port,&bytes,&key,&overlapped,0);
    DWORD status=ok ? ERROR_SUCCESS : GetLastError();
    s_equal(check,scope,"status",status,WAIT_TIMEOUT);
    s_equal(check,scope,"overlapped_is_null",overlapped==NULL,TRUE);
    /* On FALSE + NULL the byte count and key are indeterminate; do not inspect. */
}

static void s_iocp(matrix_check_fn check,DWORD timeout_ms)
{
    const char *scope="S.iocp";
    HANDLE port=CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,1);
    OVERLAPPED token={0};
    OVERLAPPED *tokens[2]={&token,NULL};
#ifdef _WIN64
    const ULONG_PTR keys[2]={(ULONG_PTR)UINT64_C(0x123456789abcdef0),0};
#else
    const ULONG_PTR keys[2]={(ULONG_PTR)UINT32_C(0x7a5bcdef),0};
#endif
    const DWORD lengths[2]={37,0};
    const char *scopes[2]={"S.iocp.first","S.iocp.null_token"};
    unsigned i;

    if (!s_api(check,scope,"create",port!=NULL)) return;
    s_iocp_empty(check,port,"S.iocp.initial_empty");
    for (i=0;i<2;++i) {
        if (!s_api(check,scopes[i],"post",
                   PostQueuedCompletionStatus(port,lengths[i],keys[i],tokens[i])))
            goto done;
    }
    /* Posted packets contain opaque user values, not live asynchronous I/O.
     * Queue both before retrieval to check FIFO and pointer-width preservation. */
    for (i=0;i<2;++i) {
        OVERLAPPED *overlapped=NULL;
        DWORD bytes=0;
        ULONG_PTR key=0;
        BOOL ok=GetQueuedCompletionStatus(port,&bytes,&key,&overlapped,timeout_ms);
        if (!s_api(check,scopes[i],"dequeue",ok)) goto done;
        s_equal(check,scopes[i],"bytes",bytes,lengths[i]);
        s_equal(check,scopes[i],"key",(int64_t)key,(int64_t)keys[i]);
        s_equal(check,scopes[i],"token",(int64_t)(intptr_t)overlapped,
                (int64_t)(intptr_t)tokens[i]);
    }
    /* TRUE + NULL above is a successful posted packet, not an empty queue. */
    s_iocp_empty(check,port,"S.iocp.drained_empty");
done:
    s_close(check,scope,"close",port);
}

typedef struct s_operation {
    OVERLAPPED overlapped;
    HANDLE file;
    const char *scope;
    int pending;
} s_operation;

typedef BOOL (WINAPI *s_cancel_fn)(HANDLE,LPOVERLAPPED);

/* Only call for an operation that actually returned ERROR_IO_PENDING.
 * Neither an event wake nor an arbitrary API failure alone releases storage. */
static int s_result(matrix_check_fn check,s_operation *op,DWORD *bytes)
{
    BOOL ok=GetOverlappedResult(op->file,&op->overlapped,bytes,FALSE);
    DWORD status=ok ? ERROR_SUCCESS : GetLastError();
    /* The macro is specified only for operations that returned IO_PENDING.
     * Otherwise the original WriteFile(TRUE) already proved completion. */
    int complete=!op->pending || HasOverlappedIoCompleted(&op->overlapped)!=FALSE;
    if (complete) op->pending=0;
    s_equal(check,op->scope,"result_api_success",ok!=FALSE,TRUE);
    s_equal(check,op->scope,"result_status",status,ERROR_SUCCESS);
    s_equal(check,op->scope,"result_completed",complete,TRUE);
    return ok && complete;
}

static void s_cancel(matrix_check_fn check,s_operation *op,s_cancel_fn cancel_io)
{
    BOOL ok;
    DWORD status;
    if (!op->pending) return;
    ok=cancel_io(op->file,&op->overlapped);
    status=ok ? ERROR_SUCCESS : GetLastError();
    s_equal(check,op->scope,"cleanup_cancel_accepted_or_raced",
            ok!=FALSE || status==ERROR_NOT_FOUND,TRUE);
    if (!ok && status==ERROR_NOT_FOUND)
        s_equal(check,op->scope,"cleanup_cancel_raced",status,ERROR_NOT_FOUND);
    else
        s_equal(check,op->scope,"cleanup_cancel",status,ERROR_SUCCESS);
    /* ERROR_NOT_FOUND is not completion proof; still drain below. */
}

static void s_drain(matrix_check_fn check,s_operation *op,
                    ULONGLONG cleanup_start,DWORD timeout_ms)
{
    ULONGLONG elapsed;
    DWORD remaining,result,bytes=0,status;
    BOOL ok;
    int complete;
    if (!op->pending) return;
    elapsed=GetTickCount64()-cleanup_start;
    remaining=elapsed>=timeout_ms ? 0 : timeout_ms-(DWORD)elapsed;
    result=WaitForSingleObject(op->overlapped.hEvent,remaining);
    s_equal(check,op->scope,"cleanup_event",result,WAIT_OBJECT_0);
    ok=GetOverlappedResult(op->file,&op->overlapped,&bytes,FALSE);
    status=ok ? ERROR_SUCCESS : GetLastError();
    complete=HasOverlappedIoCompleted(&op->overlapped)!=FALSE;
    s_equal(check,op->scope,"cleanup_completed",complete,TRUE);
    if (!complete) {
        /* Preserve this frame, every handle and both backing buffers until the
         * kernel tears down this OWN process. Never return with live stack I/O. */
        fflush(stdout);
        TerminateProcess(GetCurrentProcess(),125);
        ExitProcess(125); /* Nonreturning fallback if TerminateProcess fails. */
    }
    op->pending=0;
    if (ok)
        s_equal(check,op->scope,"cleanup_completed_normally",status,ERROR_SUCCESS);
    else
        s_equal(check,op->scope,"cleanup_completed_with_error",status,ERROR_OPERATION_ABORTED);
    /* Any other terminal error is reported as failure, but storage is now safe. */
}

static void s_pipe(matrix_check_fn check,DWORD timeout_ms)
{
    static LONG serial;
    const char *scope="S.pipe";
    char name[160];
    const unsigned char payload[]={0x43,0x69,0x64,0x65,0x72,0,0x53,0x80,0xff,0x0a,0x31};
    unsigned char received[sizeof(payload)]={0};
    HANDLE server=INVALID_HANDLE_VALUE,client=INVALID_HANDLE_VALUE,ordinary=NULL;
    HANDLE handles[2];
    s_operation connect_op={0},read_op={0},write_op={0};
    s_operation *ops[3]={&connect_op,&read_op,&write_op};
    union { FARPROC address; s_cancel_fn function; } cancel_io={0};
    HMODULE kernel;
    ULONGLONG cleanup_start;
    DWORD status,result,bytes=0,written=0;
    BOOL ok;
    int length;
    unsigned i;

    connect_op.scope="S.pipe.connect";
    read_op.scope="S.pipe.read";
    write_op.scope="S.pipe.write";
    /* Resolve the cancellation prerequisite before issuing any I/O, so an
     * unavailable API produces an explicit failed check instead of a load error. */
    kernel=GetModuleHandleW(L"kernel32.dll");
    if (!s_api(check,scope,"kernel32_available",kernel!=NULL)) goto done;
    cancel_io.address=GetProcAddress(kernel,"CancelIoEx");
    if (!s_api(check,scope,"CancelIoEx_available",cancel_io.address!=NULL)) goto done;
    length=snprintf(name,sizeof(name),"\\\\.\\pipe\\CiderSyncS_%lu_%llu_%lu",
                    (unsigned long)GetCurrentProcessId(),
                    (unsigned long long)GetTickCount64(),
                    (unsigned long)InterlockedIncrement(&serial));
    if (!s_equal(check,scope,"unique_name_fits",length>0 && (size_t)length<sizeof(name),TRUE))
        goto done;
    ordinary=CreateEventW(NULL,FALSE,FALSE,NULL);
    if (!s_api(check,scope,"create_ordinary_event",ordinary!=NULL)) goto done;
    for (i=0;i<3;++i) {
        ops[i]->overlapped.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
        if (!s_api(check,ops[i]->scope,"create_manual_event",ops[i]->overlapped.hEvent!=NULL))
            goto done;
    }
    server=CreateNamedPipeA(name,PIPE_ACCESS_INBOUND|FILE_FLAG_OVERLAPPED|
                          FILE_FLAG_FIRST_PIPE_INSTANCE,
                          PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT,1,4096,4096,timeout_ms,NULL);
    if (!s_api(check,scope,"create_owned_server",server!=INVALID_HANDLE_VALUE)) goto done;
    connect_op.file=server;
    read_op.file=server;
    handles[0]=ordinary;
    handles[1]=connect_op.overlapped.hEvent;
    ok=ConnectNamedPipe(server,&connect_op.overlapped);
    status=ok ? ERROR_SUCCESS : GetLastError();
    connect_op.pending=!ok && status==ERROR_IO_PENDING;
    /* No client has been opened: require a real pending connect. A preconnected
     * race is a failed fixture precondition, not permission to synthesize a wake. */
    if (!s_equal(check,connect_op.scope,"start_pending",status,ERROR_IO_PENDING)) goto done;
    result=WaitForMultipleObjects(2,handles,FALSE,0);
    if (!s_equal(check,connect_op.scope,"mixed_before_client",result,WAIT_TIMEOUT)) goto done;

    client=CreateFileA(name,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
    if (!s_api(check,scope,"open_owned_client",client!=INVALID_HANDLE_VALUE)) goto done;
    write_op.file=client;
    result=WaitForMultipleObjects(2,handles,FALSE,timeout_ms);
    if (!s_equal(check,connect_op.scope,"mixed_completion_index",result,WAIT_OBJECT_0+1u))
        goto done;
    if (!s_result(check,&connect_op,&bytes)) goto done;
    /* bytes from ConnectNamedPipe completion is undefined; never assert it. */
    result=WaitForSingleObject(ordinary,0);
    s_equal(check,connect_op.scope,"ordinary_still_unsignaled",result,WAIT_TIMEOUT);

    handles[1]=read_op.overlapped.hEvent;
    ok=ReadFile(server,received,(DWORD)sizeof(received),NULL,&read_op.overlapped);
    status=ok ? ERROR_SUCCESS : GetLastError();
    read_op.pending=!ok && status==ERROR_IO_PENDING;
    if (!s_equal(check,read_op.scope,"start_pending",status,ERROR_IO_PENDING)) goto done;
    result=WaitForMultipleObjects(2,handles,FALSE,0);
    if (!s_equal(check,read_op.scope,"mixed_before_data",result,WAIT_TIMEOUT)) goto done;
    /* Exercise the N side while S is pending, then the S side with N unsignaled. */
    if (!s_api(check,scope,"signal_ordinary_while_read_pending",SetEvent(ordinary))) goto done;
    result=WaitForMultipleObjects(2,handles,FALSE,0);
    if (!s_equal(check,read_op.scope,"ordinary_wins_while_pending",result,WAIT_OBJECT_0)) goto done;
    result=WaitForMultipleObjects(2,handles,FALSE,0);
    if (!s_equal(check,read_op.scope,"ordinary_consumed_read_pending",result,WAIT_TIMEOUT)) goto done;

    /* Message-read mode preserves this single payload unit. All write byte
     * counts come from GetOverlappedResult, including synchronous completion. */
    ok=WriteFile(client,payload,(DWORD)sizeof(payload),NULL,&write_op.overlapped);
    status=ok ? ERROR_SUCCESS : GetLastError();
    write_op.pending=!ok && status==ERROR_IO_PENDING;
    if (ok) {
        s_equal(check,write_op.scope,"start_immediate",status,ERROR_SUCCESS);
    } else if (!s_equal(check,write_op.scope,"start_pending",status,ERROR_IO_PENDING)) {
        goto done;
    }
    if (write_op.pending) {
        result=WaitForSingleObject(write_op.overlapped.hEvent,timeout_ms);
        if (!s_equal(check,write_op.scope,"completion_event",result,WAIT_OBJECT_0)) goto done;
    }
    if (!s_result(check,&write_op,&written)) goto done;
    if (!s_equal(check,write_op.scope,"bytes",written,sizeof(payload))) goto done;
    result=WaitForMultipleObjects(2,handles,FALSE,timeout_ms);
    if (!s_equal(check,read_op.scope,"mixed_completion_index",result,WAIT_OBJECT_0+1u)) goto done;
    if (!s_result(check,&read_op,&bytes)) goto done;
    if (s_equal(check,read_op.scope,"bytes",bytes,sizeof(payload)))
        s_equal(check,read_op.scope,"payload_memcmp",memcmp(received,payload,sizeof(payload)),0);
    result=WaitForSingleObject(ordinary,0);
    s_equal(check,read_op.scope,"ordinary_still_unsignaled",result,WAIT_TIMEOUT);
    result=WaitForMultipleObjects(2,handles,FALSE,0);
    s_equal(check,read_op.scope,"manual_completion_event_stays_signaled",result,WAIT_OBJECT_0+1u);
done:
    /* Cancel every outstanding operation before waiting for any of them. All
     * cancellation drains share ONE caller-sized budget, not three fresh ones.
     * Never close/disconnect the endpoints or events to manufacture completion. */
    cleanup_start=GetTickCount64();
    for (i=0;i<3;++i) s_cancel(check,ops[i],cancel_io.function);
    for (i=0;i<3;++i) s_drain(check,ops[i],cleanup_start,timeout_ms);
    s_close(check,scope,"close_client",client);
    s_close(check,scope,"close_server",server);
    for (i=0;i<3;++i) s_close(check,ops[i]->scope,"close_event",ops[i]->overlapped.hEvent);
    s_close(check,scope,"close_ordinary",ordinary);
}

void matrix_server_cases(matrix_check_fn check,DWORD timeout_ms)
{
    if (timeout_ms==INFINITE) {
        s_equal(check,"S","finite_timeout_required",timeout_ms,5000);
        return;
    }
    s_timer(check,timeout_ms,FALSE);
    s_timer(check,timeout_ms,TRUE);
    s_iocp(check,timeout_ms);
    s_pipe(check,timeout_ms);
}
