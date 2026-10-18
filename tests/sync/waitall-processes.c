/* Actual Win32 named-object, cross-process fixture; native coordinator tests
 * cannot substitute for this executable. Build with either MinGW C target.
 * Usage: waitall-processes.exe [iterations=100000, maximum 1000000].
 * Four owned self children, four roles each. No per-operation ledger lock.
 * The section is pagefile-backed: unmap/last close deletes the named objects;
 * no filesystem file is created. Only the parent writes JSONL. Acceptance needs
 * a complete ok:true record AND exit status zero; partial/broken stdout fails.
 * Safety deadlines are containment, not performance acceptance thresholds.
 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>

#define CHILDREN 4u
#define ROLES 4u
#define START_MS 30000u
#define RUN_MS 300000u
#define REAP_MS 5000u
#define MAGIC 0x43575031u
#define PREFIX "Local\\CiderWaitAllProc_"
typedef LONG (NTAPI *query_fn)(HANDLE,ULONG,PVOID,ULONG,PULONG);
typedef struct basic { LONG count,maximum; } basic;
typedef struct row {
    volatile LONG completed,acquired,released,full,attempts,all_success,timeouts;
    volatile LONG events,queries,stage,error,last_count,last_maximum;
} row;
typedef struct shared {
    DWORD magic,iterations;
    volatile LONG ready,stop,claimed[CHILDREN];
    row rows[CHILDREN][ROLES];
} shared;
typedef struct resources {
    HANDLE mapping,sem,never,automatic,start,ready,job;
    HANDLE process[CHILDREN],thread[CHILDREN];
    shared *view;
} resources;
typedef struct context { unsigned child,role; row *stats; } context;
static resources r;
static query_fn query;
static int child_mode,errors,output_error,shared_ok;
static DWORD first_error;
static DWORD iterations_per_role;
static WCHAR executable[32768],command[33024];

static void flush_output(void)
{
    if (fflush(stdout) || ferror(stdout)) output_error=1;
}
static int fail(const char *where,DWORD error)
{
    ++errors;
    if (!first_error) first_error=error ? error : ERROR_INVALID_DATA;
    if (r.view && shared_ok) InterlockedExchange(&r.view->stop,1);
    if (!child_mode) {
        if (printf("{\"type\":\"error\",\"where\":\"%s\",\"code\":%lu}\n",where,(unsigned long)error)<0) output_error=1;
        flush_output();
    }
    return 0;
}
static int need(const char *where,BOOL ok)
{
    return ok ? 1 : fail(where,GetLastError());
}
static int check(const char *name,int ok,int64_t actual,int64_t expected)
{
    if (!ok) fail(name,ERROR_INVALID_DATA);
    if (!child_mode) {
        if (printf("{\"type\":\"check\",\"name\":\"%s\",\"ok\":%s,\"actual\":%" PRId64 ",\"expected\":%" PRId64 "}\n",name,ok?"true":"false",actual,expected)<0) output_error=1;
        flush_output();
    }
    return ok;
}
static void close_handle(HANDLE *handle)
{
    if (*handle) need("CloseHandle",CloseHandle(*handle));
    *handle=NULL;
}
static DWORD poll_handle(HANDLE handle,const char *where)
{
    DWORD result=WaitForSingleObject(handle,0);
    if (result==WAIT_FAILED) fail(where,GetLastError());
    else if (result!=WAIT_OBJECT_0 && result!=WAIT_TIMEOUT) fail(where,ERROR_INVALID_DATA);
    return result;
}
static int name_for(char *name,const char *nonce,const char *suffix)
{
    int n=snprintf(name,128,"%s.%s",nonce,suffix);
    return n>0 && n<128 ? 1 : fail("object_name_length",ERROR_BUFFER_OVERFLOW);
}
static int fresh(HANDLE *destination,HANDLE value,const char *where)
{
    DWORD error=GetLastError();
    *destination=value;
    return value && error!=ERROR_ALREADY_EXISTS ? 1 : fail(where,error);
}
static int sem_read(basic *info,LONG *status)
{
    ULONG length=0;
    memset(info,0,sizeof(*info));
    *status=query(r.sem,0,info,(ULONG)sizeof(*info),&length);
    return *status>=0 && length==sizeof(*info);
}
static DWORD worker_error(row *stats,LONG stage,DWORD error)
{
    if (!InterlockedCompareExchange(&stats->stage,stage,0)) InterlockedExchange(&stats->error,(LONG)error);
    InterlockedExchange(&r.view->stop,1);
    return 0;
}
static DWORD WINAPI worker(void *argument)
{
    context *ctx=argument;
    row *s=ctx->stats;
    HANDLE pair[2]={r.sem,ctx->role==0 ? r.never : r.automatic};
    DWORD result,i;
    ULONGLONG deadline;
    if (InterlockedIncrement(&r.view->ready)==(LONG)(CHILDREN*ROLES) && !SetEvent(r.ready))
        return worker_error(s,1,GetLastError());
    result=WaitForSingleObject(r.start,START_MS);
    if (result!=WAIT_OBJECT_0) return worker_error(s,2,result==WAIT_FAILED ? GetLastError() : result);
    deadline=GetTickCount64()+RUN_MS;
    for (i=0;i<r.view->iterations && !InterlockedCompareExchange(&r.view->stop,0,0);++i) {
        if (!(i&127) && GetTickCount64()>=deadline) return worker_error(s,3,ERROR_TIMEOUT);
        if (ctx->role==0) {
            result=WaitForMultipleObjects(2,pair,TRUE,0);
            if (result==WAIT_OBJECT_0) { InterlockedIncrement(&s->all_success); InterlockedIncrement(&s->acquired); }
            if (result!=WAIT_TIMEOUT) return worker_error(s,4,result==WAIT_FAILED ? GetLastError() : result);
            InterlockedIncrement(&s->timeouts);
        } else if (ctx->role==1) {
            LONG previous=-1;
            InterlockedIncrement(&s->attempts);
            if (ReleaseSemaphore(r.sem,1,&previous)) {
                InterlockedIncrement(&s->released);
                if (previous!=0) return worker_error(s,5,(DWORD)previous);
            } else {
                DWORD error=GetLastError();
                if (error!=ERROR_TOO_MANY_POSTS) return worker_error(s,6,error);
                InterlockedIncrement(&s->full);
            }
        } else if (ctx->role==2) {
            result=WaitForMultipleObjects(2,pair,FALSE,0);
            if (result==WAIT_OBJECT_0) InterlockedIncrement(&s->acquired);
            else if (result==WAIT_OBJECT_0+1) InterlockedIncrement(&s->events);
            else if (result==WAIT_TIMEOUT) InterlockedIncrement(&s->timeouts);
            else return worker_error(s,7,result==WAIT_FAILED ? GetLastError() : result);
            if (!SetEvent(r.automatic)) return worker_error(s,8,GetLastError());
        } else {
            basic info; LONG status;
            int ok=sem_read(&info,&status);
            InterlockedIncrement(&s->queries);
            InterlockedExchange(&s->last_count,info.count); InterlockedExchange(&s->last_maximum,info.maximum);
            if (!ok) return worker_error(s,9,(DWORD)status);
            if (info.count<0 || info.count>1 || info.maximum!=1) return worker_error(s,10,ERROR_INVALID_DATA);
        }
        InterlockedIncrement(&s->completed);
        if (!(i&63)) SwitchToThread();
    }
    return 0;
}
static int named_objects(const char *nonce,int create,DWORD iterations)
{
    char name[128];
    if (!name_for(name,nonce,"map")) return 0;
    if (create) {
        SetLastError(0);
        if (!fresh(&r.mapping,CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,(DWORD)sizeof(shared),name),"CreateFileMapping.fresh")) return 0;
    } else {
        r.mapping=OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,name);
        if (!r.mapping) return fail("OpenFileMapping",GetLastError());
    }
    r.view=MapViewOfFile(r.mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(shared));
    if (!r.view) return fail("MapViewOfFile",GetLastError());
    if (create) { memset(r.view,0,sizeof(*r.view)); r.view->magic=MAGIC; r.view->iterations=iterations; }
    else if (!check("shared_identity",r.view->magic==MAGIC && r.view->iterations==iterations,r.view->iterations,iterations)) return 0;
    shared_ok=1;
    if (!name_for(name,nonce,"sem")) return 0;
    if (create) {
        SetLastError(0);
        if (!fresh(&r.sem,CreateSemaphoreA(NULL,1,1,name),"CreateSemaphore.fresh")) return 0;
    } else { r.sem=OpenSemaphoreA(SEMAPHORE_ALL_ACCESS,FALSE,name); if (!r.sem) return fail("OpenSemaphore",GetLastError()); }
    {
        HANDLE *handles[4]={&r.never,&r.automatic,&r.start,&r.ready};
        const char *suffix[4]={"never","auto","start","ready"};
        unsigned i;
        for (i=0;i<4;++i) {
            if (!name_for(name,nonce,suffix[i])) return 0;
            if (create) {
                SetLastError(0);
                if (!fresh(handles[i],CreateEventA(NULL,i!=1,FALSE,name),"CreateEvent.fresh")) return 0;
            } else { *handles[i]=OpenEventA(EVENT_ALL_ACCESS,FALSE,name); if (!*handles[i]) return fail("OpenEvent",GetLastError()); }
        }
    }
    return 1;
}
static void close_objects(void)
{
    close_handle(&r.sem); close_handle(&r.never); close_handle(&r.automatic);
    close_handle(&r.start); close_handle(&r.ready);
    if (r.view) { need("UnmapViewOfFile",UnmapViewOfFile(r.view)); r.view=NULL; shared_ok=0; }
    close_handle(&r.mapping);
}
static DWORD child_main(const char *nonce,unsigned index,DWORD iterations)
{
    HANDLE threads[ROLES]={NULL}; context ctx[ROLES];
    unsigned i,created=0; int reaped=1;
    if (!named_objects(nonce,0,iterations)) goto done;
    if (!check("unique_child_slot",!InterlockedCompareExchange(&r.view->claimed[index],1,0),index,index)) goto done;
    for (i=0;i<ROLES;++i) {
        ctx[i].child=index; ctx[i].role=i; ctx[i].stats=&r.view->rows[index][i];
        threads[i]=CreateThread(NULL,0,worker,&ctx[i],0,NULL);
        if (!threads[i]) { fail("CreateThread",GetLastError()); break; }
        ++created;
    }
    if (created==ROLES) {
        DWORD result=WaitForMultipleObjects(created,threads,TRUE,START_MS+RUN_MS);
        if (result!=WAIT_OBJECT_0) fail("child.thread_join",result==WAIT_FAILED ? GetLastError() : result);
    }
    if (errors) { InterlockedExchange(&r.view->stop,1); need("child.cleanup.open_start",SetEvent(r.start)); }
    for (i=0;i<created;++i) if (poll_handle(threads[i],"child.cleanup.poll")!=WAIT_OBJECT_0)
        need("TerminateThread.owned",TerminateThread(threads[i],2));
    for (i=0;i<created;++i) {
        DWORD code=0,result=WaitForSingleObject(threads[i],REAP_MS);
        if (result!=WAIT_OBJECT_0) { fail("child.thread_reap",result==WAIT_FAILED ? GetLastError() : result); reaped=0; }
        else if (need("GetExitCodeThread",GetExitCodeThread(threads[i],&code))) check("thread.exit",code==0,code,0);
        close_handle(&threads[i]);
    }
    if (!reaped) ExitProcess(3); /* never return through live workers' stack contexts */
    for (i=0;i<ROLES;++i) {
        row *s=&r.view->rows[index][i];
        check("worker.finished",s->completed==(LONG)iterations && !s->stage,s->completed,iterations);
    }
done:
    close_objects();
    return errors ? first_error : 0;
}
static int create_child(const char *nonce,unsigned index,DWORD iterations,unsigned *created)
{
    WCHAR wide_nonce[96]; STARTUPINFOW startup; PROCESS_INFORMATION info;
    DWORD length=GetModuleFileNameW(NULL,executable,32768),previous;
    int n;
    if (!length || length>=32768) return fail("GetModuleFileName",length ? ERROR_BUFFER_OVERFLOW : GetLastError());
    if (!MultiByteToWideChar(CP_UTF8,0,nonce,-1,wide_nonce,96)) return fail("child.nonce_convert",GetLastError());
    n=swprintf(command,sizeof(command)/sizeof(command[0]),L"\"%ls\" --child %ls %u %lu",executable,wide_nonce,index,(unsigned long)iterations);
    if (n<0 || n>=32767) return fail("child.command_length",ERROR_BUFFER_OVERFLOW);
    memset(&startup,0,sizeof(startup)); startup.cb=sizeof(startup); memset(&info,0,sizeof(info));
    if (!need("CreateProcess.self",CreateProcessW(executable,command,NULL,NULL,FALSE,CREATE_SUSPENDED|CREATE_NO_WINDOW,NULL,NULL,&startup,&info))) return 0;
    r.process[index]=info.hProcess; r.thread[index]=info.hThread; ++*created;
    if (!need("AssignProcessToJobObject",AssignProcessToJobObject(r.job,info.hProcess))) return 0;
    previous=ResumeThread(info.hThread);
    if (previous==(DWORD)-1) return fail("ResumeThread",GetLastError());
    if (!check("child.suspended_once",previous==1,previous,1)) return 0;
    close_handle(&r.thread[index]); return !errors;
}
static int parent_ready(void)
{
    ULONGLONG end=GetTickCount64()+START_MS;
    while (GetTickCount64()<end) {
        unsigned i;
        DWORD result=WaitForSingleObject(r.ready,50);
        if (result==WAIT_OBJECT_0) return check("all_workers_ready",r.view->ready==(LONG)(CHILDREN*ROLES),r.view->ready,CHILDREN*ROLES);
        if (result!=WAIT_TIMEOUT) return fail("parent.ready_wait",result==WAIT_FAILED ? GetLastError() : result);
        if (InterlockedCompareExchange(&r.view->stop,0,0)) return fail("child_failed_before_start",ERROR_OPERATION_ABORTED);
        for (i=0;i<CHILDREN;++i) {
            result=poll_handle(r.process[i],"parent.startup.poll");
            if (result==WAIT_FAILED) return 0;
            if (result==WAIT_OBJECT_0) {
                DWORD code=0;
                if (!need("startup.GetExitCodeProcess",GetExitCodeProcess(r.process[i],&code))) return 0;
                return fail("child_exited_before_start",code ? code : ERROR_INVALID_DATA);
            }
        }
    }
    return fail("parent.ready_deadline",ERROR_TIMEOUT);
}
static int cleanup_children(unsigned created)
{
    unsigned i; int reaped=1;
    if (r.view && shared_ok) InterlockedExchange(&r.view->stop,1);
    if (r.start) need("cleanup.open_start",SetEvent(r.start));
    for (i=0;i<created;++i) {
        DWORD result=poll_handle(r.process[i],"parent.cleanup.poll");
        if (result!=WAIT_OBJECT_0) need("TerminateProcess.owned",TerminateProcess(r.process[i],2));
    }
    for (i=0;i<created;++i) {
        DWORD result=WaitForSingleObject(r.process[i],REAP_MS);
        if (result!=WAIT_OBJECT_0) { fail("cleanup.child_reap",result==WAIT_FAILED ? GetLastError() : result); reaped=0; }
    }
    if (!reaped && r.job) {
        need("TerminateJobObject.owned",TerminateJobObject(r.job,2)); reaped=1;
        for (i=0;i<created;++i) {
            DWORD result=WaitForSingleObject(r.process[i],REAP_MS);
            if (result!=WAIT_OBJECT_0) { fail("cleanup.job_child_reap",result==WAIT_FAILED ? GetLastError() : result); reaped=0; }
        }
    }
    for (i=0;i<created;++i) { close_handle(&r.thread[i]); close_handle(&r.process[i]); }
    close_handle(&r.job); return reaped;
}
static int complete_parent(int reaped)
{
    int ok;
    flush_output();
    if (!reaped) return 2; /* no receipt for unconfirmed child teardown */
    ok=!errors && !output_error;
    if (printf("{\"type\":\"complete\",\"ok\":%s,\"errors\":%d,\"output_error\":%s,\"pointer_bits\":%u,\"iterations_per_role\":%lu,\"cleanup_checked\":true}\n",ok?"true":"false",errors,output_error?"true":"false",(unsigned)(sizeof(void*)*8),(unsigned long)iterations_per_role)<0) output_error=1;
    flush_output(); return ok && !output_error ? 0 : 1;
}
static int parent_main(DWORD iterations)
{
    char nonce[96]; LARGE_INTEGER tick; JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    basic info; LONG status; unsigned i,j,created=0; int reaped,n;
    int64_t acquired=0,released=0,full=0,attempts=0,all=0,queries=0,completed=0;
    if (!need("QueryPerformanceCounter",QueryPerformanceCounter(&tick))) goto done;
    n=snprintf(nonce,sizeof(nonce),PREFIX "%lu_%" PRIu64,(unsigned long)GetCurrentProcessId(),(uint64_t)tick.QuadPart);
    if (n<=0 || n>=(int)sizeof(nonce)) { fail("nonce_length",ERROR_BUFFER_OVERFLOW); goto done; }
    if (!named_objects(nonce,1,iterations)) goto done;
    r.job=CreateJobObjectW(NULL,NULL); if (!r.job) { fail("CreateJobObject",GetLastError()); goto done; }
    memset(&limits,0,sizeof(limits)); limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!need("job.kill_on_close",SetInformationJobObject(r.job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))) goto done;
    if (printf("{\"type\":\"begin\",\"children\":4,\"roles_per_child\":4,\"pointer_bits\":%u,\"iterations_per_role\":%lu,\"run_timeout_ms\":%u,\"roles\":[\"waitall_never\",\"release\",\"waitany_set_auto\",\"query\"],\"scope\":\"Win32 named objects across four self child processes\"}\n",(unsigned)(sizeof(void*)*8),(unsigned long)iterations,RUN_MS)<0) output_error=1;
    flush_output(); if (output_error) { fail("stdout",ERROR_WRITE_FAULT); goto done; }
    for (i=0;i<CHILDREN;++i) if (!create_child(nonce,i,iterations,&created)) goto done;
    if (!parent_ready() || !need("parent.start",SetEvent(r.start))) goto done;
    {
        DWORD result=WaitForMultipleObjects(CHILDREN,r.process,TRUE,RUN_MS);
        if (result!=WAIT_OBJECT_0) { fail("parent.child_join",result==WAIT_FAILED ? GetLastError() : result); goto done; }
    }
    for (i=0;i<CHILDREN;++i) {
        DWORD code=0;
        if (need("GetExitCodeProcess",GetExitCodeProcess(r.process[i],&code))) check("child.exit",code==0,code,0);
        check("child.claimed",r.view->claimed[i]==1,r.view->claimed[i],1);
        for (j=0;j<ROLES;++j) {
            row *s=&r.view->rows[i][j];
            check("worker.stage",s->stage==0,s->stage,0); check("worker.iterations",s->completed==(LONG)iterations,s->completed,iterations);
            acquired+=s->acquired; released+=s->released; full+=s->full; attempts+=s->attempts;
            all+=s->all_success; queries+=s->queries; completed+=s->completed;
            if (printf("{\"type\":\"worker\",\"child\":%u,\"role\":%u,\"stage\":%ld,\"error\":%lu,\"completed\":%ld,\"sem_acquisitions\":%ld,\"successful_releases\":%ld,\"too_many_posts\":%ld,\"queries\":%ld,\"last_count\":%ld,\"last_maximum\":%ld}\n",i,j,s->stage,(unsigned long)(DWORD)s->error,s->completed,s->acquired,s->released,s->full,s->queries,s->last_count,s->last_maximum)<0) output_error=1;
        }
    }
    if (!sem_read(&info,&status)) { fail("NtQuerySemaphore.final",(DWORD)status); goto done; }
    check("semaphore.maximum",info.maximum==1,info.maximum,1); check("semaphore.bounds",info.count>=0 && info.count<=1,info.count,1);
    check("semaphore.exact_ledger",released+1-acquired==info.count,released+1-acquired,info.count);
    check("release.accounting",released+full==attempts,released+full,attempts);
    check("handoff.acquisitions_observed",acquired>0,acquired,1);
    check("handoff.successful_releases_observed",released>0,released,1);
    check("WaitAll.never_succeeds",all==0,all,0); check("queries.complete",queries==(int64_t)CHILDREN*iterations,queries,(int64_t)CHILDREN*iterations);
    check("iterations.complete",completed==(int64_t)CHILDREN*ROLES*iterations,completed,(int64_t)CHILDREN*ROLES*iterations);
    if (printf("{\"type\":\"result\",\"initial_count\":1,\"final_count\":%ld,\"maximum\":%ld,\"successful_releases\":%" PRId64 ",\"sem_acquisitions\":%" PRId64 ",\"release_too_many_posts\":%" PRId64 ",\"waitall_successes\":%" PRId64 ",\"queries\":%" PRId64 "}\n",info.count,info.maximum,released,acquired,full,all,queries)<0) output_error=1;
done:
    reaped=cleanup_children(created); close_objects(); return complete_parent(reaped);
}
static int number(const char *text,unsigned maximum,unsigned *value)
{
    char *end; unsigned long parsed;
    if (text[0]<'0' || text[0]>'9') return 0;
    errno=0; parsed=strtoul(text,&end,10);
    if (errno || *end || parsed>maximum) return 0;
    *value=(unsigned)parsed; return 1;
}
static int valid_nonce(const char *nonce)
{
    const char *p; unsigned digits=0;
    if (strlen(nonce)>=96 || strncmp(nonce,PREFIX,sizeof(PREFIX)-1)) return 0;
    p=nonce+sizeof(PREFIX)-1;
    while (*p>='0' && *p<='9') { ++p; ++digits; }
    if (!digits || *p++!='_') return 0;
    digits=0; while (*p>='0' && *p<='9') { ++p; ++digits; }
    return digits && !*p;
}
int main(int argc,char **argv)
{
    unsigned iterations=100000,index=0; HMODULE module; FARPROC address;
    child_mode=argc>1 && !strcmp(argv[1],"--child");
    if ((child_mode && (argc!=5 || !valid_nonce(argv[2]) || !number(argv[3],CHILDREN-1,&index) || !number(argv[4],1000000,&iterations) || !iterations)) ||
        (!child_mode && (argc>2 || (argc==2 && (!number(argv[1],1000000,&iterations) || !iterations))))) {
        fail("arguments",ERROR_INVALID_PARAMETER); return child_mode ? (int)first_error : complete_parent(1);
    }
    iterations_per_role=(DWORD)iterations;
    module=GetModuleHandleW(L"ntdll.dll");
    if (!module) { fail("GetModuleHandle.ntdll",GetLastError()); return child_mode ? (int)first_error : complete_parent(1); }
    address=GetProcAddress(module,"NtQuerySemaphore");
    if (!address) { fail("NtQuerySemaphore.available",GetLastError()); return child_mode ? (int)first_error : complete_parent(1); }
    if (sizeof(address)!=sizeof(query)) { fail("NtQuerySemaphore.pointer_size",ERROR_INVALID_DATA); return child_mode ? (int)first_error : complete_parent(1); }
    memcpy(&query,&address,sizeof(query));
    return child_mode ? (int)child_main(argv[2],index,(DWORD)iterations) : parent_main((DWORD)iterations);
}
