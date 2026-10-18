/* Exact client getter and cleanup bodies, bounded object/RPC/SEH dependencies.
 * This checks control flow and ownership; it is not Wine SEH/native parity. */
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int32_t NTSTATUS, LONG;
typedef int BOOL;
typedef void *HANDLE, *PEPROCESS, *PACCESS_TOKEN, *POBJECT_TYPE;
#define WINAPI
#define CALLBACK
#define TRACE(...) ((void)0)
#define WARN(...) ((void)0)
#define FALSE 0
#define TRUE 1
#define KernelMode 0
#define MAXLONG INT32_MAX
#define MEMORY_ALLOCATION_ALIGNMENT 16
#define DECLSPEC_ALIGN(n) __attribute__((aligned(n)))
#define STATUS_SUCCESS ((NTSTATUS)0)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xc0000008)
#define STATUS_ACCESS_DENIED ((NTSTATUS)0xc0000022)
#define STATUS_NO_MEMORY ((NTSTATUS)0xc0000017)
#define STATUS_OBJECT_TYPE_MISMATCH ((NTSTATUS)0xc0000024)
#define STATUS_INTEGER_OVERFLOW ((NTSTATUS)0xc0000095)
#define STATUS_INVALID_DEVICE_STATE ((NTSTATUS)0xc0000184)
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000u
struct seh_frame { jmp_buf env; struct seh_frame *prev; NTSTATUS code; };
static struct seh_frame *active_frame;
static void RtlRaiseStatus(NTSTATUS status) {
    assert(status && active_frame); active_frame->code=status; longjmp(active_frame->env,1);
}
#define __TRY do { struct seh_frame *_frame=calloc(1,sizeof(*_frame)); int _caught; assert(_frame); \
    _frame->prev=active_frame; active_frame=_frame; _caught=setjmp(_frame->env); if(!_caught)
#define __FINALLY_CTX(fn,ctx) active_frame=_frame->prev; fn(!_caught,ctx); \
    { NTSTATUS _code=_frame->code; free(_frame); if(_caught) RtlRaiseStatus(_code); } } while(0)
#define __FINALLY(fn) active_frame=_frame->prev; fn(!_caught); \
    { NTSTATUS _code=_frame->code; free(_frame); if(_caught) RtlRaiseStatus(_code); } } while(0)
#define __EXCEPT_ALL active_frame=_frame->prev; if(_caught)
#define __ENDTRY active_frame=_frame->prev; free(_frame); } while(0);
#define GetExceptionCode() (_frame->code)
static int process_type, token_type, kernel_process, handle_map_cs, obref_cs;
static POBJECT_TYPE PsProcessType=&process_type, SeTokenObjectType=&token_type;
#include "object-header.h"
static int map_depth, ref_depth, handles, allocated, freed, closes, derefs, rpc_refs, rpc_releases, backend_owned;
static NTSTATUS open_status, alloc_status, rpc_status, rpc_raise, close_status, release_status;
static int close_raises;
static void *cached;
static void EnterCriticalSection(int *lock) {
    if(lock==&handle_map_cs) { assert(!map_depth && !ref_depth); map_depth=1; }
    else { assert(lock==&obref_cs && map_depth && !ref_depth); ref_depth=1; }
}
static void LeaveCriticalSection(int *lock) {
    if(lock==&obref_cs) { assert(map_depth && ref_depth); ref_depth=0; }
    else { assert(lock==&handle_map_cs && map_depth && !ref_depth); map_depth=0; }
}
static void CALLBACK unlock_handle_map(BOOL normal,void *context) {
    (void)normal; (void)context; LeaveCriticalSection(&handle_map_cs);
}
static HANDLE get_device_manager(void) { return (HANDLE)(uintptr_t)0x111; }
#define wine_server_obj_handle(h) ((uintptr_t)(h))
#define wine_server_client_ptr(p) ((uintptr_t)(p))
#define wine_server_get_ptr(p) ((void *)(uintptr_t)(p))
enum { OP_reference_process_primary_token, OP_release_kernel_object };
struct model_reply { uintptr_t user_ptr; int new_owned; };
struct model_request { int operation; uintptr_t manager,process,process_ptr,candidate_ptr,user_ptr; struct model_reply *reply; };
#define SERVER_START_REQ(name) do { struct model_reply _reply={0}; \
    struct model_request _request={0}, *req=&_request; struct model_reply *reply=&_reply; \
    _request.operation=OP_##name; _request.reply=reply;
#define SERVER_END_REQ } while(0)
static NTSTATUS wine_server_call(struct model_request *req) {
    assert(map_depth && ref_depth && req->manager==0x111);
    if(req->operation==OP_reference_process_primary_token) {
        ++rpc_refs;
        assert(req->process==0x222 && req->process_ptr==(uintptr_t)&kernel_process && req->candidate_ptr);
        if(rpc_raise) RtlRaiseStatus(rpc_raise);
        if(rpc_status) return rpc_status;
        if(!cached) cached=(void *)req->candidate_ptr;
        req->reply->user_ptr=(uintptr_t)cached;
        req->reply->new_owned=!backend_owned; backend_owned=1;
    } else {
        ++rpc_releases; assert(req->user_ptr==(uintptr_t)cached && backend_owned);
        if(release_status) return release_status;
        backend_owned=0;
    }
    return 0;
}
static NTSTATUS ObOpenObjectByPointer(void *process,unsigned int attr,void *access_state,
    unsigned int access,POBJECT_TYPE type,int mode,HANDLE *out) {
    assert(!map_depth && !ref_depth && process==&kernel_process && !attr && !access_state);
    assert(access==PROCESS_QUERY_LIMITED_INFORMATION && type==PsProcessType && mode==KernelMode);
    if(open_status) return open_status;
    ++handles; *out=(HANDLE)(uintptr_t)0x222; return 0;
}
static NTSTATUS alloc_kernel_object_checked(POBJECT_TYPE type,HANDLE handle,size_t size,LONG ref,void **out) {
    struct object_header *header;
    assert(type==SeTokenObjectType && !handle && !size && !ref);
    if(alloc_status) return alloc_status;
    header=calloc(1,sizeof(*header)); assert(header); header->type=type; ++allocated;
    *out=header+1; return 0;
}
static void free_kernel_object(void *object) { ++freed; free((struct object_header *)object-1); }
static NTSTATUS NtClose(HANDLE handle) {
    assert(!map_depth && !ref_depth && handle==(HANDLE)(uintptr_t)0x222); ++closes;
    if(close_raises) { assert(handles==1); handles=0; RtlRaiseStatus(STATUS_INVALID_HANDLE); }
    if(close_status) return close_status;
    assert(handles==1); handles=0; return 0;
}
static void ObDereferenceObject(void *object) {
    struct object_header *h=(struct object_header *)object-1;
    assert(!map_depth && !ref_depth && object==cached && h->ref>0); ++derefs;
    if(!--h->ref) { assert(backend_owned); backend_owned=0; }
}
#include "client-extracted.h"
#define CHECK(value) do { ++checks; assert(value); } while(0)
static int checks;
static NTSTATUS invoke(void *process,void **out) {
    struct seh_frame *frame=calloc(1,sizeof(*frame)); NTSTATUS result;
    assert(frame); frame->prev=active_frame; active_frame=frame;
    if(!setjmp(frame->env)) { *out=PsReferencePrimaryToken(process); result=0; }
    else { *out=NULL; result=frame->code; }
    active_frame=frame->prev; free(frame); CHECK(!map_depth && !ref_depth); return result;
}
static void reset(void) {
    if(cached) free_kernel_object(cached);
    cached=NULL; handles=allocated=freed=closes=derefs=rpc_refs=rpc_releases=backend_owned=0;
    open_status=alloc_status=rpc_status=rpc_raise=close_status=release_status=0; close_raises=0;
    assert(!map_depth && !ref_depth && !active_frame);
}
int main(void) {
    void *a=NULL,*b=NULL; struct object_header *h;
    CHECK(invoke(NULL,&a)==STATUS_INVALID_PARAMETER && !a && !handles);
    CHECK(invoke(&kernel_process,&a)==0 && a==cached && handles==0 && closes==1 && backend_owned);
    h=(struct object_header *)a-1; CHECK(h->ref==1 && allocated==1 && !freed);
    CHECK(invoke(&kernel_process,&b)==0 && b==a && h->ref==2 && rpc_refs==2 && backend_owned);
    CHECK(allocated==2 && freed==1 && handles==0);
    ObDereferenceObject(a); CHECK(h->ref==1 && backend_owned);
    ObDereferenceObject(b); CHECK(!h->ref && !backend_owned);
    CHECK(invoke(&kernel_process,&b)==0 && b==a && h->ref==1 && backend_owned);
    ObDereferenceObject(b); reset();
    open_status=STATUS_ACCESS_DENIED; CHECK(invoke(&kernel_process,&a)==open_status && !a && !handles && !allocated); reset();
    alloc_status=STATUS_NO_MEMORY; CHECK(invoke(&kernel_process,&a)==alloc_status && !a && !handles && closes==1 && !rpc_refs); reset();
    rpc_status=STATUS_INVALID_HANDLE; CHECK(invoke(&kernel_process,&a)==rpc_status && !a && !handles && allocated==freed); reset();
    rpc_raise=STATUS_NO_MEMORY; CHECK(invoke(&kernel_process,&a)==rpc_raise && !a && !handles && allocated==freed); reset();
    CHECK(invoke(&kernel_process,&a)==0); h=(struct object_header *)a-1; h->ref=MAXLONG;
    CHECK(invoke(&kernel_process,&b)==STATUS_INTEGER_OVERFLOW && !b && h->ref==MAXLONG && !rpc_releases);
    h->ref=1; ObDereferenceObject(a); reset();
    CHECK(invoke(&kernel_process,&a)==0); ObDereferenceObject(a); h=(struct object_header *)a-1;
    h->reference_error=STATUS_ACCESS_DENIED;
    CHECK(invoke(&kernel_process,&b)==STATUS_ACCESS_DENIED && !b && !h->ref && !backend_owned && rpc_releases==1); reset();
    CHECK(invoke(&kernel_process,&a)==0); ObDereferenceObject(a); h=(struct object_header *)a-1;
    h->type=PsProcessType; release_status=STATUS_NO_MEMORY;
    CHECK(invoke(&kernel_process,&b)==STATUS_OBJECT_TYPE_MISMATCH && !b && h->reference_error==release_status && backend_owned && rpc_releases==1); reset();
    close_status=STATUS_ACCESS_DENIED;
    CHECK(invoke(&kernel_process,&a)==close_status && !a && handles==1 && derefs==1 && !backend_owned);
    /* A failed close remains unconfirmed, not retried or reported as closed. */
    handles=0; reset();
    close_raises=1;
    CHECK(invoke(&kernel_process,&a)==STATUS_INVALID_HANDLE && !a && handles==0 && closes==1 && derefs==1 && !backend_owned); reset();
    CHECK(invoke(&kernel_process,&a)==0);
    {
        struct primary_token_reference_context state={0};
        state.result=a;
        finish_primary_token_reference(FALSE,&state);
        CHECK(!state.result && derefs==1 && !backend_owned);
    }
    reset();
    printf("PASS: %d client adoption/alias/rollback/close/SEH checks; modeled dependencies only\n",checks);
    return 0;
}
