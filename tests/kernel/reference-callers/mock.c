/* Exact patched C bodies are generated in extracted.h; only dependencies are mocked.
 * Host setjmp models unwind control flow; it does not execute Wine's SEH runtime. */
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int32_t NTSTATUS;
typedef int BOOL;
typedef int32_t LONG;
typedef uint32_t ULONG, ACCESS_MASK;
typedef uintptr_t ULONG_PTR, SIZE_T;
typedef void *HANDLE, *PVOID;
typedef uint16_t WCHAR;
typedef uint8_t BYTE;
typedef int KPROCESSOR_MODE, WORK_QUEUE_TYPE;
#define WINAPI
#define CALLBACK
#define FASTCALL
#define TRUE 1
#define FALSE 0
#define KernelMode 0
#define UserMode 1
#define STATUS_SUCCESS ((NTSTATUS)0)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xc0000008)
#define STATUS_NO_MEMORY ((NTSTATUS)0xc0000017)
#define STATUS_ACCESS_DENIED ((NTSTATUS)0xc0000022)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xc0000001)
#define STATUS_INTEGER_OVERFLOW ((NTSTATUS)0xc0000095)
#define STATUS_NOT_IMPLEMENTED ((NTSTATUS)0xc0000002)
#define STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xc0000034)
#define STATUS_OBJECT_TYPE_MISMATCH ((NTSTATUS)0xc0000024)
#define STATUS_IMAGE_ALREADY_LOADED ((NTSTATUS)0xc000010e)
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define PROCESS_ALL_ACCESS 0x1fffff
#define PROCESS_QUERY_INFORMATION 0x400
#define THREAD_ALL_ACCESS 0x1fffff
#define THREAD_QUERY_INFORMATION 0x40
#define MAXIMUM_ALLOWED 0x02000000
#define EVENT_ALL_ACCESS 0x1f0003
#define OBJ_CASE_INSENSITIVE 0x40
#define TRACE(...) ((void)0)
#define FIXME(...) ((void)0)
#define WARN(...) ((void)0)
#define ERR(...) ((void)0)
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define MAX_SERVICE_NAME 260
#define SPDRP_SERVICE 4
#define HandleToUlong(x) ((ULONG)(uintptr_t)(x))
#define ULongToHandle(x) ((HANDLE)(uintptr_t)(x))

typedef struct { ULONG Length, MaximumLength; WCHAR *Buffer; } UNICODE_STRING;
typedef struct { int unused; } ACCESS_STATE, OBJECT_HANDLE_INFORMATION, *POBJECT_HANDLE_INFORMATION;
typedef struct _OBJECT_TYPE { int id; } *POBJECT_TYPE;
static struct _OBJECT_TYPE types[5];
static POBJECT_TYPE IoFileObjectType=&types[0], IoDriverObjectType=&types[1],
    PsProcessType=&types[2], PsThreadType=&types[3], ExEventObjectType=&types[4];
typedef struct { int Type; struct { void *Blink, *Flink; } WaitListHead; } DISPATCHER_HEADER;
typedef struct { uintptr_t UniqueProcessId, InheritedFromUniqueProcessId; } PROCESS_BASIC_INFORMATION;
typedef struct { DISPATCHER_HEADER header; PROCESS_BASIC_INFORMATION info; BOOL wow64; } *PEPROCESS;
typedef struct { HANDLE UniqueProcess, UniqueThread; } CLIENT_ID;
struct _KTHREAD { DISPATCHER_HEADER header; PEPROCESS process; CLIENT_ID id; unsigned user_affinity; };
typedef struct _KTHREAD *PRKTHREAD, *PETHREAD;
typedef struct { CLIENT_ID ClientId; } THREAD_BASIC_INFORMATION;
typedef struct { int Type, Size; } FILE_OBJECT;
typedef struct { int tag; } DEVICE_OBJECT, *PDEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT;
typedef struct { NTSTATUS (*AddDevice)(DRIVER_OBJECT *, DEVICE_OBJECT *); } DRIVER_EXTENSION;
struct _DRIVER_OBJECT { UNICODE_STRING DriverName; DRIVER_EXTENSION *DriverExtension; };
typedef void *HDEVINFO;
typedef struct { int unused; } SP_DEVINFO_DATA;
static const WCHAR servicesW[]={42,0};
struct wine_rb_entry { int tag; };
struct wine_driver { DRIVER_OBJECT driver_obj; struct wine_rb_entry entry; };
static struct wine_driver tracked_driver;
static int wine_drivers, drivers_cs, handle_map_cs, tree_present, compare_result, lock_depth;
#define WINE_RB_ENTRY_VALUE(e,t,m) (&tracked_driver)
typedef struct { UNICODE_STRING *ObjectName; HANDLE RootDirectory; ULONG Attributes; } OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;
#define InitializeObjectAttributes(a,n,f,r,s) do { (a)->ObjectName=n; (a)->Attributes=f; (a)->RootDirectory=r; } while(0)
enum { ProcessBasicInformation, ProcessWow64Information, ThreadBasicInformation, EventBasicInformation };
enum { NotificationEvent, SynchronizationEvent };
typedef struct { int EventType, EventState; } EVENT_BASIC_INFORMATION;
typedef struct { DISPATCHER_HEADER Header; int state; } KEVENT, *PKEVENT;
typedef void TP_CALLBACK_INSTANCE;
typedef void (*PIO_WORKITEM_ROUTINE)(DEVICE_OBJECT *, void *);
struct _IO_WORKITEM { DEVICE_OBJECT *device; PIO_WORKITEM_ROUTINE worker; void *context; };
typedef struct _IO_WORKITEM *PIO_WORKITEM;
static struct { void *Instrumentation[2]; } teb;
static ULONG request_thread=10, client_tid=20, current_tid=11;
#define NtCurrentTeb() (&teb)

static NTSTATUS reference_status, map_status, alloc_status, query_process_status[2], query_thread_status,
    query_event_status, map_raises, open_process_status, open_thread_status, open_pointer_status, create_event_status,
    raised, last_status;
static int references, acquired, dereferenced, submissions, submit_ok, callbacks, allocs,
    query_calls, opens, closes, expected_driver_lock;
static HANDLE last_closed;
static jmp_buf *raise_target;
static DEVICE_OBJECT device;
static struct _KTHREAD mapped_thread;
static struct { DISPATCHER_HEADER header; PROCESS_BASIC_INFORMATION info; BOOL wow64; } mapped_process;
static void *allocation;
static void (*pending_callback)(TP_CALLBACK_INSTANCE *, void *);
static void *pending_context;
static int checks;
#define CHECK(x) do { ++checks; assert(x); } while(0)
static _Noreturn void RtlRaiseStatus(NTSTATUS status)
{
    CHECK(status != STATUS_SUCCESS);
    CHECK(lock_depth == 0);
    raised=status;
    assert(raise_target);
    longjmp(*raise_target,1);
}
#define EXPECT_RAISE(s, expression) do { jmp_buf frame; jmp_buf *outer=raise_target; \
    raise_target=&frame; if (!setjmp(frame)) { expression; CHECK(0); } \
    CHECK(raised==(s)); raise_target=outer; } while(0)
#define __TRY do { jmp_buf frame; jmp_buf *outer=raise_target; int caught; \
    raise_target=&frame; caught=setjmp(frame); if (!caught)
#define __FINALLY_CTX(fn,ctx) raise_target=outer; fn(!caught,ctx); \
    if(caught) longjmp(*outer,1); } while(0)

static void EnterCriticalSection(int *cs) { CHECK(cs==&drivers_cs || cs==&handle_map_cs); ++lock_depth; }
static void LeaveCriticalSection(int *cs) { CHECK(cs==&drivers_cs || cs==&handle_map_cs); CHECK(lock_depth==1); --lock_depth; }
static struct wine_rb_entry *wine_rb_get(int *tree, UNICODE_STRING *name)
{ CHECK(tree==&wine_drivers); CHECK(lock_depth==1); return tree_present ? &tracked_driver.entry : NULL; }
static int RtlCompareUnicodeString(UNICODE_STRING *a, UNICODE_STRING *b, BOOL insensitive)
{ CHECK(!insensitive); return compare_result; }
static NTSTATUS reference_kernel_object_checked(void *obj)
{
    if (expected_driver_lock) CHECK(lock_depth==1);
    if (!obj) return STATUS_INVALID_PARAMETER;
    if (reference_status) return reference_status;
    ++references; ++acquired; return STATUS_SUCCESS;
}
static void ObDereferenceObject(void *obj)
{ CHECK(obj!=NULL); CHECK(references>0); --references; ++dereferenced; last_status=STATUS_UNSUCCESSFUL; }
static NTSTATUS RtlGetLastNtStatus(void) { return last_status; }
static BOOL TrySubmitThreadpoolCallback(void (*fn)(TP_CALLBACK_INSTANCE *, void *), void *ctx, void *env)
{ ++submissions; if(submit_ok) { pending_callback=fn; pending_context=ctx; } return submit_ok; }
static NTSTATUS alloc_kernel_object_checked(POBJECT_TYPE type, HANDLE h, SIZE_T size, LONG ref, void **out)
{
    ++allocs;
    if(alloc_status) return alloc_status;
    CHECK(allocation==NULL); allocation=calloc(1,size); CHECK(allocation!=NULL); *out=allocation; return STATUS_SUCCESS;
}
static NTSTATUS kernel_object_from_handle(HANDLE handle, POBJECT_TYPE type, void **out)
{
    CHECK(handle!=NULL);
    if (map_raises) RtlRaiseStatus( STATUS_NO_MEMORY );
    if (map_status) return map_status;
    *out = type==PsProcessType ? (void *)&mapped_process : (void *)&mapped_thread;
    return STATUS_SUCCESS;
}
static NTSTATUS NtQueryInformationProcess(HANDLE h, int cls, void *out, SIZE_T size, void *len)
{
    ++query_calls;
    if(query_process_status[cls]) return query_process_status[cls];
    if(cls==ProcessBasicInformation) ((PROCESS_BASIC_INFORMATION *)out)->UniqueProcessId=42;
    else *(ULONG_PTR *)out=1;
    return STATUS_SUCCESS;
}
static NTSTATUS NtQueryInformationThread(HANDLE h, int cls, THREAD_BASIC_INFORMATION *out, SIZE_T size, void *len)
{ ++query_calls; if(query_thread_status) return query_thread_status; out->ClientId=(CLIENT_ID){(HANDLE)42,(HANDLE)43}; return 0; }
static NTSTATUS NtQueryEvent(HANDLE h, int cls, EVENT_BASIC_INFORMATION *out, SIZE_T size, void *len)
{ ++query_calls; if(query_event_status) return query_event_status; *out=(EVENT_BASIC_INFORMATION){SynchronizationEvent,1}; return 0; }
static void KeInitializeEvent(KEVENT *event, int type, BOOL state) { event->Header.Type=type; event->state=state; }
static NTSTATUS NtOpenProcess(HANDLE *out, ACCESS_MASK access, OBJECT_ATTRIBUTES *attr, CLIENT_ID *cid)
{ if(open_process_status) return open_process_status; ++opens; *out=(HANDLE)77; return 0; }
static HANDLE OpenProcess(ACCESS_MASK access, BOOL inherit, ULONG id)
{ if(open_process_status) return NULL; ++opens; return (HANDLE)77; }
static NTSTATUS NtOpenThread(HANDLE *out, ACCESS_MASK access, OBJECT_ATTRIBUTES *attr, CLIENT_ID *cid)
{ if(open_thread_status) return open_thread_status; ++opens; *out=(HANDLE)78; return 0; }
#ifdef COMBINED_LIFETIME_FIX
#define DUPLICATE_SAME_ACCESS 2
#define kernel_object_from_handle_locked kernel_object_from_handle
#define LOOKUP_HANDLES 2
static HANDLE GetCurrentProcess(void) { return (HANDLE)(intptr_t)-1; }
static NTSTATUS NtDuplicateObject(HANDLE from, HANDLE h, HANDLE to, HANDLE *out, ACCESS_MASK access, ULONG attr, ULONG opts)
{ CHECK(access==0 && attr==0 && opts==DUPLICATE_SAME_ACCESS); ++opens; *out=(HANDLE)81; return 0; }
#else
#define LOOKUP_HANDLES 1
#endif
static NTSTATUS NtClose(HANDLE h) { CHECK(h!=NULL); ++closes; last_closed=h; return 0; }
static NTSTATUS NtCreateEvent(HANDLE *out, ACCESS_MASK access, OBJECT_ATTRIBUTES *attr, int type, BOOL state)
{ if(create_event_status) return create_event_status; ++opens; *out=(HANDLE)79; return 0; }
static NTSTATUS ObOpenObjectByPointer(void *obj, ULONG attributes, ACCESS_STATE *state, ACCESS_MASK access,
    POBJECT_TYPE type, KPROCESSOR_MODE mode, HANDLE *out)
{ if(open_pointer_status) return open_pointer_status; *out=(HANDLE)80; return 0; }
static HANDLE GetCurrentThread(void) { return (HANDLE)(intptr_t)-2; }
static ULONG GetCurrentThreadId(void) { return current_tid; }
static DEVICE_OBJECT *IoGetAttachedDevice(DEVICE_OBJECT *obj) { return obj; }
static void lstrcpyW(WCHAR *out, const WCHAR *in) { while((*out++=*in++)); }
static void lstrcatW(WCHAR *out, const WCHAR *in) { while(*out) ++out; lstrcpyW(out,in); }
static void RtlInitUnicodeString(UNICODE_STRING *str, WCHAR *buffer)
{ size_t n=0; while(buffer[n]) ++n; *str=(UNICODE_STRING){n*2,(n+1)*2,buffer}; }
static BOOL SetupDiGetDeviceRegistryPropertyW(HDEVINFO set, SP_DEVINFO_DATA *dev, ULONG prop,
    void *type, void *buffer, SIZE_T size, void *required)
{ ((WCHAR *)buffer)[0]=42; ((WCHAR *)buffer)[1]=0; return TRUE; }
/* A host mock: this does not link to or execute any driver loader. */
static NTSTATUS ZwLoadDriver(UNICODE_STRING *name) { return STATUS_SUCCESS; }

#include "extracted.h"

static void reset(void)
{
    free(allocation); allocation=NULL;
    reference_status=map_status=alloc_status=query_thread_status=query_event_status=0;
    query_process_status[0]=query_process_status[1]=open_process_status=open_thread_status=0;
    open_pointer_status=create_event_status=raised=last_status=0;
    references=acquired=dereferenced=submissions=callbacks=allocs=query_calls=opens=closes=map_raises=0;
    lock_depth=expected_driver_lock=compare_result=0; tree_present=submit_ok=1;
    pending_callback=NULL; pending_context=NULL; last_closed=NULL;
    memset(&teb,0,sizeof(teb)); current_tid=11;
}
static void worker(DEVICE_OBJECT *obj, void *ctx)
{ CHECK(obj==&device); CHECK(ctx==(void *)123); ++callbacks; }
static void free_worker(DEVICE_OBJECT *obj, void *ctx)
{ CHECK(obj==&device); ++callbacks; free(ctx); }
static void raising_worker(DEVICE_OBJECT *obj, void *ctx)
{ CHECK(obj==&device); ++callbacks; RtlRaiseStatus(STATUS_ACCESS_DENIED); }

static void test_work(void)
{
    struct _IO_WORKITEM item={&device,NULL,NULL};
    reset(); reference_status=STATUS_INTEGER_OVERFLOW;
    EXPECT_RAISE(STATUS_INTEGER_OVERFLOW,IoQueueWorkItem(&item,worker,0,(void *)123));
    CHECK(acquired==0 && dereferenced==0 && submissions==0);
    reset(); submit_ok=0; last_status=STATUS_NO_MEMORY;
    EXPECT_RAISE(STATUS_NO_MEMORY,IoQueueWorkItem(&item,worker,0,(void *)123));
    CHECK(acquired==1 && dereferenced==1 && references==0 && callbacks==0);
    reset(); submit_ok=0;
    EXPECT_RAISE(STATUS_UNSUCCESSFUL,IoQueueWorkItem(&item,worker,0,(void *)123));
    CHECK(dereferenced==1 && references==0);
    reset(); IoQueueWorkItem(&item,worker,0,(void *)123);
    CHECK(acquired==1 && dereferenced==0 && references==1);
    pending_callback(NULL,pending_context);
    CHECK(callbacks==1 && dereferenced==1 && references==0);
    reset(); PIO_WORKITEM dynamic=malloc(sizeof(*dynamic)); CHECK(dynamic!=NULL);
    dynamic->device=&device; IoQueueWorkItem(dynamic,free_worker,0,dynamic);
    pending_callback(NULL,pending_context); CHECK(callbacks==1 && dereferenced==1);
    reset(); IoQueueWorkItem(&item,raising_worker,0,NULL);
    EXPECT_RAISE(STATUS_ACCESS_DENIED,pending_callback(NULL,pending_context));
    CHECK(callbacks==1 && dereferenced==1 && references==0);
    reset(); EXPECT_RAISE(STATUS_INVALID_PARAMETER,IoQueueWorkItem(NULL,worker,0,NULL));
    EXPECT_RAISE(STATUS_INVALID_PARAMETER,IoQueueWorkItem(&item,NULL,0,NULL));
    item.device=NULL;
    EXPECT_RAISE(STATUS_INVALID_PARAMETER,IoQueueWorkItem(&item,worker,0,NULL));
    CHECK(acquired==0 && submissions==0 && dereferenced==0);
}

static void test_name(void)
{
    WCHAR buffer[2]={42,0}; UNICODE_STRING name={2,4,buffer};
    void *out=(void *)123; NTSTATUS status;
    reset(); expected_driver_lock=1; reference_status=STATUS_INTEGER_OVERFLOW;
    status=ObReferenceObjectByName(&name,0,NULL,0,IoDriverObjectType,KernelMode,NULL,&out);
    CHECK(status==STATUS_INTEGER_OVERFLOW && out==(void *)123 && references==0 && lock_depth==0);
    reference_status=0;
    status=ObReferenceObjectByName(&name,0,NULL,0,IoDriverObjectType,KernelMode,NULL,&out);
    CHECK(status==0 && out==&tracked_driver.driver_obj && references==1 && lock_depth==0);
    reset(); out=(void *)123;
    CHECK(ObReferenceObjectByName(&name,0,NULL,0,PsProcessType,KernelMode,NULL,&out)==STATUS_OBJECT_TYPE_MISMATCH);
    CHECK(out==(void *)123 && acquired==0);
    CHECK(ObReferenceObjectByName(&name,0,NULL,0,NULL,UserMode,NULL,&out)==STATUS_NOT_IMPLEMENTED);
    CHECK(ObReferenceObjectByName(&name,0,NULL,1,NULL,KernelMode,NULL,&out)==STATUS_NOT_IMPLEMENTED);
    CHECK(ObReferenceObjectByName(&name,0,(void *)1,0,NULL,KernelMode,NULL,&out)==STATUS_NOT_IMPLEMENTED);
    CHECK(ObReferenceObjectByName(&name,0,NULL,0,NULL,KernelMode,(void *)1,&out)==STATUS_NOT_IMPLEMENTED);
    CHECK(ObReferenceObjectByName(&name,1,NULL,0,NULL,KernelMode,NULL,&out)==STATUS_NOT_IMPLEMENTED);
    compare_result=1;
    CHECK(ObReferenceObjectByName(&name,0,NULL,0,NULL,KernelMode,NULL,&out)==STATUS_OBJECT_NAME_NOT_FOUND);
    CHECK(ObReferenceObjectByName(&name,OBJ_CASE_INSENSITIVE,NULL,0,NULL,KernelMode,NULL,&out)==0);
    reset(); tree_present=0; out=(void *)123;
    CHECK(ObReferenceObjectByName(&name,0,NULL,0,NULL,KernelMode,NULL,&out)==STATUS_NOT_IMPLEMENTED);
    CHECK(out==(void *)123 && lock_depth==0 && acquired==0);
    CHECK(ObReferenceObjectByName(NULL,0,NULL,0,NULL,KernelMode,NULL,&out)==STATUS_INVALID_PARAMETER);
    CHECK(ObReferenceObjectByName(&name,0,NULL,0,NULL,KernelMode,NULL,NULL)==STATUS_INVALID_PARAMETER);
    name.Length=3;
    CHECK(ObReferenceObjectByName(&name,0,NULL,0,NULL,KernelMode,NULL,&out)==STATUS_INVALID_PARAMETER);
    name.Length=6;
    CHECK(ObReferenceObjectByName(&name,0,NULL,0,NULL,KernelMode,NULL,&out)==STATUS_INVALID_PARAMETER);
    reset(); name.Length=2; OBJECT_ATTRIBUTES attr={&name,NULL,0}; HANDLE h=(void *)456;
    open_pointer_status=STATUS_ACCESS_DENIED;
    CHECK(ObOpenObjectByName(&attr,NULL,KernelMode,NULL,0,NULL,&h)==STATUS_ACCESS_DENIED);
    CHECK(acquired==1 && dereferenced==1 && references==0 && h==(void *)456);
    reset(); reference_status=STATUS_INTEGER_OVERFLOW;
    CHECK(ObOpenObjectByName(&attr,NULL,KernelMode,NULL,0,NULL,&h)==STATUS_INTEGER_OVERFLOW);
    CHECK(acquired==0 && dereferenced==0);
    reset();
    CHECK(ObOpenObjectByName(NULL,NULL,KernelMode,NULL,0,NULL,&h)==STATUS_INVALID_PARAMETER);
    CHECK(ObOpenObjectByName(&attr,NULL,KernelMode,NULL,0,NULL,NULL)==STATUS_INVALID_PARAMETER);
    attr.RootDirectory=(HANDLE)1;
    CHECK(ObOpenObjectByName(&attr,NULL,KernelMode,NULL,0,NULL,&h)==STATUS_NOT_IMPLEMENTED);
    CHECK(acquired==0 && dereferenced==0 && h==(void *)456);
}

static void test_constructors(void)
{
    void *out=(void *)123;
    reset(); query_process_status[0]=STATUS_ACCESS_DENIED;
    CHECK(create_process_object((HANDLE)1,&out)==STATUS_ACCESS_DENIED);
    CHECK(out==(void *)123 && allocs==0);
    reset(); query_process_status[1]=STATUS_INVALID_HANDLE;
    CHECK(create_process_object((HANDLE)1,&out)==STATUS_INVALID_HANDLE);
    CHECK(out==(void *)123 && allocs==0 && query_calls==2);
    reset(); alloc_status=STATUS_ACCESS_DENIED;
    CHECK(create_process_object((HANDLE)1,&out)==STATUS_ACCESS_DENIED);
    CHECK(out==(void *)123 && allocation==NULL);
    reset(); CHECK(create_process_object((HANDLE)1,&out)==0);
    CHECK(((PEPROCESS)out)->info.UniqueProcessId==42 && ((PEPROCESS)out)->wow64);
    CHECK(((PEPROCESS)out)->header.Type==3);
    reset(); out=(void *)123; query_thread_status=STATUS_ACCESS_DENIED;
    CHECK(create_thread_object((HANDLE)1,&out)==STATUS_ACCESS_DENIED);
    CHECK(allocs==0 && opens==0 && closes==0 && out==(void *)123);
    reset(); open_process_status=STATUS_ACCESS_DENIED;
    CHECK(create_thread_object((HANDLE)1,&out)==STATUS_ACCESS_DENIED);
    CHECK(allocs==0 && opens==0 && closes==0);
    reset(); map_status=STATUS_INVALID_HANDLE;
    CHECK(create_thread_object((HANDLE)1,&out)==STATUS_INVALID_HANDLE);
    CHECK(allocs==0 && opens==1 && closes==1 && last_closed==(HANDLE)77);
    reset(); map_raises=1;
    EXPECT_RAISE(STATUS_NO_MEMORY,create_thread_object((HANDLE)1,&out));
    CHECK(opens==1 && closes==1 && allocs==0);
    reset(); alloc_status=STATUS_NO_MEMORY;
    CHECK(create_thread_object((HANDLE)1,&out)==STATUS_NO_MEMORY);
    CHECK(allocation==NULL && opens==1 && closes==1 && out==(void *)123);
    reset(); CHECK(create_thread_object((HANDLE)1,&out)==0);
    CHECK(opens==1 && closes==1 && ((PRKTHREAD)out)->process==(PEPROCESS)&mapped_process);
    CHECK(((PRKTHREAD)out)->id.UniqueThread==(HANDLE)43);
    reset(); out=(void *)123; query_event_status=STATUS_ACCESS_DENIED;
    CHECK(create_event_object((HANDLE)1,&out)==STATUS_ACCESS_DENIED);
    CHECK(allocs==0 && out==(void *)123);
    reset(); alloc_status=STATUS_INVALID_HANDLE;
    CHECK(create_event_object((HANDLE)1,&out)==STATUS_INVALID_HANDLE);
    CHECK(out==(void *)123 && allocation==NULL);
    reset(); CHECK(create_event_object((HANDLE)1,&out)==0);
    CHECK(((KEVENT *)out)->state==1 && ((KEVENT *)out)->Header.WaitListHead.Blink==INVALID_HANDLE_VALUE);
    reset(); out=(void *)123; alloc_status=STATUS_NO_MEMORY;
    CHECK(create_file_object((HANDLE)1,&out)==STATUS_NO_MEMORY);
    CHECK(out==(void *)123);
    reset(); alloc_status=STATUS_ACCESS_DENIED;
    CHECK(create_file_object((HANDLE)1,&out)==STATUS_ACCESS_DENIED);
    CHECK(out==(void *)123);
    reset(); CHECK(create_file_object((HANDLE)1,&out)==0);
    CHECK(((FILE_OBJECT *)out)->Type==5 && ((FILE_OBJECT *)out)->Size==sizeof(FILE_OBJECT));
}

static void test_handles(void)
{
    PEPROCESS p=(void *)123; PETHREAD t=(void *)123; HANDLE h=(void *)123;
    for (int failure=0;failure<3;failure++)
    {
        reset(); if(failure==0) map_status=STATUS_INVALID_HANDLE;
        if(failure==1) reference_status=STATUS_INTEGER_OVERFLOW;
        NTSTATUS want=failure==0 ? map_status : reference_status;
        CHECK(PsLookupProcessByProcessId((HANDLE)42,&p)==want);
        CHECK(opens==LOOKUP_HANDLES && closes==LOOKUP_HANDLES && last_closed==(HANDLE)77);
        CHECK(failure==2 ? p==(PEPROCESS)&mapped_process : p==(void *)123);
        reset(); if(failure==0) map_status=STATUS_INVALID_HANDLE;
        if(failure==1) reference_status=STATUS_INTEGER_OVERFLOW;
        CHECK(PsLookupThreadByThreadId((HANDLE)43,&t)==want);
        CHECK(opens==LOOKUP_HANDLES && closes==LOOKUP_HANDLES && last_closed==(HANDLE)78);
        CHECK(failure==2 ? t==&mapped_thread : t==(void *)123);
    }
    reset(); p=(void *)123; open_process_status=STATUS_ACCESS_DENIED;
    CHECK(PsLookupProcessByProcessId((HANDLE)42,&p)==STATUS_INVALID_PARAMETER);
    CHECK(opens==0 && closes==0 && p==(void *)123);
    reset(); t=(void *)123; open_thread_status=STATUS_ACCESS_DENIED;
    CHECK(PsLookupThreadByThreadId((HANDLE)43,&t)==STATUS_ACCESS_DENIED);
    CHECK(opens==0 && closes==0 && t==(void *)123);
    reset(); CHECK(ObReferenceObjectByHandle((HANDLE)1,0,NULL,UserMode,(void **)&p,NULL)==STATUS_NOT_IMPLEMENTED);
    for (int notification=0;notification<2;notification++)
    {
        reset(); map_status=STATUS_ACCESS_DENIED;
        CHECK((notification ? IoCreateNotificationEvent(NULL,&h) : IoCreateSynchronizationEvent(NULL,&h))==NULL);
        CHECK(opens==1 && closes==1 && last_closed==(HANDLE)79 && h==(void *)123);
        reset(); create_event_status=STATUS_NO_MEMORY;
        CHECK((notification ? IoCreateNotificationEvent(NULL,&h) : IoCreateSynchronizationEvent(NULL,&h))==NULL);
        CHECK(opens==0 && closes==0 && h==(void *)123);
    }
}

static void test_current_and_void(void)
{
    reset(); current_tid=request_thread; open_thread_status=STATUS_ACCESS_DENIED;
    EXPECT_RAISE(STATUS_ACCESS_DENIED,KeGetCurrentThread());
    CHECK(opens==0 && closes==0 && teb.Instrumentation[1]==NULL);
    reset(); current_tid=request_thread; map_status=STATUS_INVALID_HANDLE;
    EXPECT_RAISE(STATUS_INVALID_HANDLE,KeGetCurrentThread());
    CHECK(opens==1 && closes==1 && last_closed==(HANDLE)78 && teb.Instrumentation[1]==NULL);
    reset(); map_status=STATUS_NO_MEMORY;
    EXPECT_RAISE(STATUS_NO_MEMORY,KeGetCurrentThread());
    CHECK(opens==0 && closes==0 && teb.Instrumentation[1]==NULL);
    reset(); current_tid=request_thread;
    CHECK(KeGetCurrentThread()==&mapped_thread && opens==1 && closes==1);
    CHECK(KeGetCurrentThread()==&mapped_thread && opens==1 && closes==1);
    reset(); EXPECT_RAISE(STATUS_INVALID_PARAMETER,ObReferenceObject(NULL));
    reference_status=STATUS_INTEGER_OVERFLOW;
    EXPECT_RAISE(STATUS_INTEGER_OVERFLOW,ObfReferenceObject(&device));
    EXPECT_RAISE(STATUS_INTEGER_OVERFLOW,IoGetAttachedDeviceReference(&device));
    CHECK(acquired==0 && dereferenced==0);
    reset(); CHECK(IoGetAttachedDeviceReference(&device)==&device && references==1);
    reset(); ObfReferenceObject(&device); CHECK(references==1);
}

static NTSTATUS add_device(DRIVER_OBJECT *driver, DEVICE_OBJECT *obj)
{ CHECK(driver==&tracked_driver.driver_obj && obj==&device); ++callbacks; return STATUS_ACCESS_DENIED; }
static NTSTATUS raising_add_device(DRIVER_OBJECT *driver, DEVICE_OBJECT *obj)
{ ++callbacks; RtlRaiseStatus(STATUS_ACCESS_DENIED); }
static void test_pnp_cleanup(void)
{
    DRIVER_EXTENSION extension={add_device};
    tracked_driver.driver_obj.DriverExtension=&extension;
    reset(); load_function_driver(&device,NULL,NULL);
    CHECK(callbacks==1 && acquired==1 && dereferenced==1 && references==0);
    reset(); reference_status=STATUS_INTEGER_OVERFLOW;
    load_function_driver(&device,NULL,NULL);
    CHECK(callbacks==0 && acquired==0 && dereferenced==0);
    reset(); extension.AddDevice=NULL;
    load_function_driver(&device,NULL,NULL);
    CHECK(callbacks==0 && acquired==1 && dereferenced==1);
    reset(); extension.AddDevice=raising_add_device;
    EXPECT_RAISE(STATUS_ACCESS_DENIED,load_function_driver(&device,NULL,NULL));
    CHECK(callbacks==1 && acquired==1 && dereferenced==1 && references==0);
}

int main(void)
{
    test_work(); test_name(); test_constructors(); test_handles(); test_current_and_void(); test_pnp_cleanup();
    reset(); printf("PASS: %d assertions across work submission/unwind, named lookup, constructors, handles, void APIs, and PnP cleanup\n",checks);
    return 0;
}
