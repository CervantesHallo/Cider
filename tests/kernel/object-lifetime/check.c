/* Exact C functions are extracted into core.h. This backend models object
 * ownership only; it is not Wine, a driver, or a Windows reference result. */
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int32_t NTSTATUS, LONG; typedef unsigned long ACCESS_MASK;
typedef void *HANDLE; typedef int BOOL, KPROCESSOR_MODE;
#define WINAPI
#define CALLBACK
#define STATUS_SUCCESS 0
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xc0000008)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_NOT_IMPLEMENTED ((NTSTATUS)0xc0000002)
#define STATUS_INTEGER_OVERFLOW ((NTSTATUS)0xc0000095)
#define STATUS_NO_MEMORY ((NTSTATUS)0xc0000017)
#define KernelMode 0
#define UserMode 1
#define DUPLICATE_SAME_ACCESS 2
#define MAXLONG INT32_MAX
#define TRACE(...) ((void)0)
#define WARN(...) ((void)0)
#define FIXME(...) ((void)0)
#define ERR(...) ((void)0)
struct _OBJECT_TYPE { void (*release)(void *); }; typedef struct _OBJECT_TYPE *POBJECT_TYPE;
#define MEMORY_ALLOCATION_ALIGNMENT (2 * sizeof(void *))
#define DECLSPEC_ALIGN(n) __attribute__((aligned(n)))
#include "header.h"
_Static_assert(sizeof(struct object_header) % MEMORY_ALLOCATION_ALIGNMENT == 0, "header payload alignment");
typedef void *POBJECT_HANDLE_INFORMATION;
struct dispatch_context { struct { struct { void *obj; } free; } params; };
static pthread_mutex_t handle_map_cs, obref_cs, state_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
static _Thread_local int map_depth, ref_depth;
static _Thread_local jmp_buf *raise_target;
static _Thread_local NTSTATUS raised;
static int original_open, stable_open, backend_owned, freed, dup_calls, close_calls,
    grab_calls, release_calls, force_grab_error, force_release_error, force_dup_error,
    lookup_error, constructor_raise, race, at_lookup, resumed;
static struct _OBJECT_TYPE object_type;
static void *object;
static NTSTATUS race_status; static void *race_output;
static void EnterCriticalSection(pthread_mutex_t *lock) {
 assert(pthread_mutex_lock(lock)==0); if(lock==&handle_map_cs)++map_depth;else ++ref_depth;
}
static void LeaveCriticalSection(pthread_mutex_t *lock) {
 if(lock==&handle_map_cs){assert(map_depth>0);--map_depth;}else{assert(ref_depth>0);--ref_depth;}
 assert(pthread_mutex_unlock(lock)==0);
}
static _Noreturn void RtlRaiseStatus(NTSTATUS status) {
 assert(!map_depth && !ref_depth); assert(raise_target); raised=status; longjmp(*raise_target,1);
}
#define __TRY do { jmp_buf frame; jmp_buf *outer=raise_target; int caught; raise_target=&frame; caught=setjmp(frame); if(!caught)
#define __FINALLY_CTX(fn,ctx) raise_target=outer; fn(!caught,ctx); if(caught)longjmp(*outer,1); }while(0)
static HANDLE GetCurrentProcess(void){return (HANDLE)(intptr_t)-1;}
static HANDLE get_device_manager(void){return (HANDLE)1;}
#define wine_server_obj_handle(x) (x)
#define wine_server_client_ptr(x) (x)
#define wine_server_get_ptr(x) (x)
static void *GetProcessHeap(void){return NULL;}
static void HeapFree(void *heap,int flags,void *ptr){(void)heap;(void)flags;++freed;free(ptr);}
enum { grab_kernel_object, release_kernel_object };
struct request { HANDLE manager; void *user_ptr; int op; };
#define SERVER_START_REQ(kind) do { struct request value={.op=kind}; struct request *req=&value;
#define SERVER_END_REQ }while(0)
static NTSTATUS wine_server_call(struct request *req) {
 NTSTATUS status=0; assert(req->user_ptr==object); pthread_mutex_lock(&state_lock);
 if(req->op==grab_kernel_object) {
  ++grab_calls;
  if(force_grab_error||(!original_open&&!stable_open)||backend_owned)status=STATUS_INVALID_HANDLE;
  else backend_owned=1;
 } else {
  ++release_calls;
  if(force_release_error==2 && backend_owned){backend_owned=0;status=STATUS_NO_MEMORY;}
  else if(force_release_error||!backend_owned)status=STATUS_INVALID_HANDLE;else backend_owned=0;
 }
 pthread_mutex_unlock(&state_lock);return status;
}
static NTSTATUS NtDuplicateObject(HANDLE source,HANDLE handle,HANDLE dest,HANDLE *out,
                                 ACCESS_MASK access,unsigned attrs,unsigned options) {
 (void)source;(void)dest;assert(access==0&&attrs==0&&options==DUPLICATE_SAME_ACCESS);
 ++dup_calls;pthread_mutex_lock(&state_lock);
 NTSTATUS status=(force_dup_error||!handle||!original_open)?STATUS_INVALID_HANDLE:0;
 if(!status){++stable_open;*out=(HANDLE)22;}
 pthread_mutex_unlock(&state_lock);return status;
}
static NTSTATUS NtClose(HANDLE handle) {
 assert(handle==(HANDLE)22);++close_calls;pthread_mutex_lock(&state_lock);
 assert(stable_open>0);--stable_open;pthread_mutex_unlock(&state_lock);return 0;
}
static NTSTATUS dispatch_free(struct dispatch_context *);
static NTSTATUS kernel_object_from_handle(HANDLE,POBJECT_TYPE,void **);
#ifdef OLD_CORE
#define kernel_object_from_handle_locked kernel_object_from_handle
#endif
static NTSTATUS kernel_object_from_handle_locked(HANDLE handle,POBJECT_TYPE type,void **out) {
 (void)handle;(void)type;
 if(race){pthread_mutex_lock(&state_lock);at_lookup=1;pthread_cond_broadcast(&condition);
  while(!resumed)pthread_cond_wait(&condition,&state_lock);pthread_mutex_unlock(&state_lock);}
 if(constructor_raise==1){constructor_raise=2;return kernel_object_from_handle(handle,type,out);}
 if(constructor_raise==2){assert(raise_target);raised=STATUS_NO_MEMORY;longjmp(*raise_target,1);}
 if(lookup_error)return STATUS_INVALID_HANDLE;
 *out=object;return 0;
}
#include "core.h"
static void reset(int ref) {
 if(object&&!freed)free((struct object_header*)object-1);
 struct object_header *header=calloc(1,sizeof(*header)+16);assert(header);
 header->ref=ref;header->type=&object_type;object=header+1;
 assert((uintptr_t)object % MEMORY_ALLOCATION_ALIGNMENT == 0);
 original_open=1;stable_open=freed=dup_calls=close_calls=grab_calls=release_calls=0;
 force_grab_error=force_release_error=force_dup_error=lookup_error=constructor_raise=0;
 backend_owned=ref>0;race=at_lookup=resumed=0;race_output=(void*)123;race_status=STATUS_INVALID_PARAMETER;
 assert(!map_depth&&!ref_depth);
}
static void *reference_thread(void *unused) {
 (void)unused;race_status=ObReferenceObjectByHandle((HANDLE)11,0,&object_type,KernelMode,&race_output,NULL);return NULL;
}
static void *close_thread(void *unused) {
 (void)unused;pthread_mutex_lock(&state_lock);
 while(!at_lookup)pthread_cond_wait(&condition,&state_lock);
 original_open=0;int should_free=!stable_open&&!backend_owned;
 pthread_mutex_unlock(&state_lock);
 if(should_free){struct dispatch_context ctx={.params.free.obj=object};dispatch_free(&ctx);}
 pthread_mutex_lock(&state_lock);resumed=1;pthread_cond_broadcast(&condition);pthread_mutex_unlock(&state_lock);return NULL;
}
static void concurrent_close(void) {
 reset(0);race=1;pthread_t worker,closer;
 assert(!pthread_create(&worker,NULL,reference_thread,NULL));assert(!pthread_create(&closer,NULL,close_thread,NULL));
 assert(!pthread_join(worker,NULL));assert(!pthread_join(closer,NULL));
 assert(!race_status&&race_output==object&&!freed&&backend_owned);
 assert(((struct object_header*)object-1)->ref==1&&stable_open==0);
 struct dispatch_context ctx={.params.free.obj=object};assert(dispatch_free(&ctx)==STATUS_INVALID_PARAMETER&&!freed);
 ObDereferenceObject(object);assert(!backend_owned);assert(!dispatch_free(&ctx)&&freed==1);
}
int main(int argc,char **argv) {
 pthread_mutexattr_t attr;assert(!pthread_mutexattr_init(&attr));assert(!pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE));
 assert(!pthread_mutex_init(&handle_map_cs,&attr));assert(!pthread_mutex_init(&obref_cs,&attr));pthread_mutexattr_destroy(&attr);
 if(argc>1&&strcmp(argv[1],"race")==0){concurrent_close();puts("race PASS");return 0;}
 reset(0);void *out=(void*)123;
 assert(ObReferenceObjectByHandle((HANDLE)11,0,&object_type,KernelMode,&out,NULL)==0);
 assert(out==object&&dup_calls==1&&close_calls==1&&stable_open==0&&backend_owned);
 ObDereferenceObject(object);assert(!backend_owned&&release_calls==1);
 reset(1);out=(void*)123;assert(!ObReferenceObjectByHandle((HANDLE)11,0,&object_type,KernelMode,&out,NULL));
 assert(((struct object_header*)object-1)->ref==2&&grab_calls==0);ObDereferenceObject(object);ObDereferenceObject(object);
 reset(0);force_dup_error=1;assert(ObReferenceObjectByHandle((HANDLE)11,0,NULL,KernelMode,&out,NULL)==STATUS_INVALID_HANDLE&&stable_open==0);
 reset(0);force_grab_error=1;out=(void*)123;assert(ObReferenceObjectByHandle((HANDLE)11,0,NULL,KernelMode,&out,NULL)==STATUS_INVALID_HANDLE);
 assert(out==(void*)123&&stable_open==0&&((struct object_header*)object-1)->ref==0);
 reset(MAXLONG);out=(void*)123;assert(ObReferenceObjectByHandle((HANDLE)11,0,NULL,KernelMode,&out,NULL)==STATUS_INTEGER_OVERFLOW);
 assert(out==(void*)123&&stable_open==0&&((struct object_header*)object-1)->ref==MAXLONG);
 reset(0);assert(ObReferenceObjectByHandle((HANDLE)11,0,NULL,UserMode,&out,NULL)==STATUS_NOT_IMPLEMENTED);
 assert(ObReferenceObjectByHandle((HANDLE)11,0,NULL,KernelMode,NULL,NULL)==STATUS_INVALID_PARAMETER);
 assert(ObReferenceObjectByHandle((HANDLE)11,0,NULL,KernelMode,&out,(void*)1)==STATUS_NOT_IMPLEMENTED&&dup_calls==0);
 reset(0);lookup_error=1;out=(void*)123;assert(ObReferenceObjectByHandle((HANDLE)11,0,NULL,KernelMode,&out,NULL)==STATUS_INVALID_HANDLE);
 assert(out==(void*)123&&stable_open==0&&close_calls==1);
 reset(0);constructor_raise=1;jmp_buf frame;raise_target=&frame;
 if(!setjmp(frame)){ObReferenceObjectByHandle((HANDLE)11,0,NULL,KernelMode,&out,NULL);assert(0);}
 assert(raised==STATUS_NO_MEMORY&&stable_open==0&&!map_depth&&!ref_depth);raise_target=NULL;
 reset(1);force_release_error=1;raise_target=&frame;
 if(!setjmp(frame)){ObDereferenceObject(object);assert(0);}
 assert(raised==STATUS_INVALID_HANDLE&&((struct object_header*)object-1)->ref==0&&backend_owned&&!ref_depth);raise_target=NULL;
 assert(((struct object_header*)object-1)->reference_error==STATUS_INVALID_HANDLE);
 int old_grabs=grab_calls;assert(reference_kernel_object_checked(object)==STATUS_INVALID_HANDLE&&grab_calls==old_grabs);
 raise_target=&frame;if(!setjmp(frame)){ObDereferenceObject(object);assert(0);}
 assert(raised==STATUS_INVALID_HANDLE&&release_calls==1);raise_target=NULL;
 reset(1);force_release_error=2;raise_target=&frame;
 if(!setjmp(frame)){ObDereferenceObject(object);assert(0);}
 assert(raised==STATUS_NO_MEMORY&&!backend_owned&&((struct object_header*)object-1)->ref==0);raise_target=NULL;
 assert(reference_kernel_object_checked(object)==STATUS_NO_MEMORY && release_calls==1);
 original_open=0;struct dispatch_context committed={.params.free.obj=object};
 assert(!dispatch_free(&committed)&&freed==1);
 struct dispatch_context null_free={.params.free.obj=NULL};assert(dispatch_free(&null_free)==STATUS_INVALID_PARAMETER);
 reset(0);raise_target=&frame;
 if(!setjmp(frame)){ObDereferenceObject(object);assert(0);}
 assert(raised==STATUS_INVALID_PARAMETER&&!release_calls&&((struct object_header*)object-1)->ref==0);raise_target=NULL;
 for(int i=0;i<100;i++)concurrent_close();
 free(NULL);pthread_mutex_destroy(&handle_map_cs);pthread_mutex_destroy(&obref_cs);
 puts("K1 exact functions: ownership/errors/unwind + 100 deterministic two-thread races PASS");return 0;
}
