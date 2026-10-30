#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ITERATIONS 1000000u
#define BASE_HIGH UINT32_C(0x20000000)
struct stress_control
{
    uint32_t magic,version,pid,birth_lo,birth_hi,bits,mapped,attempts;
    uint32_t snapshots,retries,invalid,mid_snapshots,last_n,writer_running,writer_done,abort;
    struct macdrv_status_wire identity;
};
_Static_assert(sizeof(struct stress_control)==224,"stress control224");
static unsigned checks,failures;
#define CHECK(x) check_value(!!(x),#x,__LINE__)
static BOOL check_value(BOOL value,const char *expression,unsigned line)
{
    ++checks;
    if(!value){++failures;printf("status-stress-fail %u line %u: %s\n",checks,line,expression);fflush(stdout);}
    return value;
}
static uint32_t load32(const uint32_t *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void store32(uint32_t *p,uint32_t value){__atomic_store_n(p,value,__ATOMIC_RELEASE);}
static BOOL birth_of(HANDLE process,uint64_t *out)
{
    FILETIME c,e,k,u;
    if(!GetProcessTimes(process,&c,&e,&k,&u))return FALSE;
    *out=(uint64_t)c.dwLowDateTime|((uint64_t)c.dwHighDateTime<<32);
    return TRUE;
}
static void values_for(uint32_t n,struct macdrv_status_values *v)
{
    memset(v,0,sizeof(*v));v->state=MACDRV_STATUS_ACTIVE;
    v->error=n*UINT32_C(2654435761);v->pending_requests=n^UINT32_C(0x13579bdf);
    v->pending_models=n^UINT32_C(0x2468ace0);v->cleanup_pending=n^UINT32_C(0xdeadbeef);
    v->applied_sequence=((uint64_t)(BASE_HIGH+n)<<32)|(n^UINT32_C(0xa5a5a5a5));
    v->quiescent_sequence=v->applied_sequence-1;
}
static BOOL snapshot_valid(const struct macdrv_status_snapshot *s,uint32_t *n)
{
    struct macdrv_status_values expected;
    uint32_t high=(uint32_t)(s->values.applied_sequence>>32);
    if(high<BASE_HIGH||high>BASE_HIGH+ITERATIONS)return FALSE;
    *n=high-BASE_HIGH;values_for(*n,&expected);
    return s->values_valid&&!s->safe&&!s->exhausted&&s->sequence==2+2*(*n)&&
        s->values.state==expected.state&&s->values.error==expected.error&&
        s->values.pending_requests==expected.pending_requests&&s->values.pending_models==expected.pending_models&&
        s->values.cleanup_pending==expected.cleanup_pending&&s->values.applied_sequence==expected.applied_sequence&&
        s->values.quiescent_sequence==expected.quiescent_sequence;
}
static BOOL wait_flag(const uint32_t *flag,uint32_t minimum,HANDLE peer,DWORD ms)
{
    ULONGLONG end=GetTickCount64()+ms;
    while(GetTickCount64()<end)
    {
        if(load32(flag)>=minimum)return GetTickCount64()<end;
        if(WaitForSingleObject(peer,0)!=WAIT_TIMEOUT)return FALSE;
        SwitchToThread();
    }
    return FALSE;
}
static BOOL close_control(HANDLE mapping,struct stress_control *control)
{
    BOOL clean=TRUE;
    if(control)clean=UnmapViewOfFile(control)&&clean;
    if(mapping)clean=CloseHandle(mapping)&&clean;
    return clean;
}
static int reader_main(int argc,char **argv)
{
    HANDLE mapping=NULL;struct stress_control *c=NULL;struct macdrv_status_cell *reader=NULL;
    struct macdrv_status_identity id;struct macdrv_status_snapshot snapshot;uint64_t birth=0;
    uint32_t attempts=0,snapshots=0,retries=0,invalid=0,mid=0,n=0,last=0;
    enum macdrv_status_read_result result;BOOL clean=TRUE,finished=FALSE;ULONGLONG end;MEMORY_BASIC_INFORMATION info;
    if(!CHECK(argc==3))goto done;
    mapping=OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,argv[2]);if(!CHECK(mapping!=NULL))goto done;
    c=MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*c));if(!CHECK(c!=NULL))goto done;
    if(!CHECK(c->magic==0x53545253&&c->version==1&&birth_of(GetCurrentProcess(),&birth)))goto done;
    c->pid=GetCurrentProcessId();c->birth_lo=(uint32_t)birth;c->birth_hi=(uint32_t)(birth>>32);c->bits=8*sizeof(void*);
    store32(&c->mapped,1);
    end=GetTickCount64()+10000;
    while(!load32(&c->writer_running)&&GetTickCount64()<end&&!load32(&c->abort))SwitchToThread();
    if(!CHECK(GetTickCount64()<end&&!load32(&c->abort)&&load32(&c->writer_running)==1))goto done;
    macdrv_status_read_identity(&c->identity,&id);
    if(!CHECK(macdrv_status_cell_open(&id,&reader)==0))goto done;
    if(!CHECK(VirtualQuery(macdrv_status_cell_view(reader,NULL),&info,sizeof(info))==sizeof(info)&&info.Protect==PAGE_READONLY&&info.State==MEM_COMMIT))goto done;
    if(!CHECK(macdrv_status_cell_close_handle(reader)==0&&macdrv_status_cell_handle(reader)==NULL))goto done;
    store32(&c->mapped,2);
    end=GetTickCount64()+10000;
    for(;;)
    {
        uint32_t running=load32(&c->writer_running);
        if(GetTickCount64()>=end||load32(&c->abort)||attempts==UINT32_MAX)break;
        result=macdrv_status_cell_read(reader,&snapshot);++attempts;
        if(result==MACDRV_STATUS_READ_SNAPSHOT)
        {
            if(!snapshot_valid(&snapshot,&n)||n<last){++invalid;break;}
            last=n;++snapshots;
            if(running==1&&load32(&c->writer_running)==1&&n>0&&n<ITERATIONS)++mid;
        }
        else if(result==MACDRV_STATUS_READ_RETRY)++retries;
        else {++invalid;break;}
        store32(&c->attempts,attempts);store32(&c->mid_snapshots,mid);store32(&c->last_n,last);
        if(load32(&c->writer_done))
        {
            result=macdrv_status_cell_read(reader,&snapshot);
            finished=result==MACDRV_STATUS_READ_SNAPSHOT&&snapshot_valid(&snapshot,&n)&&n==ITERATIONS;
            break;
        }
    }
    CHECK(finished&&GetTickCount64()<end&&invalid==0&&mid>0&&snapshots>0&&retries>0);
    CHECK(macdrv_status_cell_close(&reader)==0&&reader==NULL);
done:
    clean=macdrv_status_cell_close(&reader)==0&&clean;
    if(c){store32(&c->snapshots,snapshots);store32(&c->retries,retries);store32(&c->invalid,invalid);}
    clean=close_control(mapping,c)&&clean;
    printf("{\"type\":\"status-stress-reader\",\"bits\":%u,\"pid\":%lu,\"checks\":%u,\"failures\":%u,\"iterations\":%u,\"attempts\":%u,\"snapshots\":%u,\"retries\":%u,\"invalid\":%u,\"mid_snapshots\":%u,\"finished\":%u,\"cleanup\":%u}\n",(unsigned)(8*sizeof(void*)),GetCurrentProcessId(),checks,failures,ITERATIONS,attempts,snapshots,retries,invalid,mid,(unsigned)finished,(unsigned)clean);fflush(stdout);
    return failures||!clean||!finished?1:0;
}
int main(int argc,char **argv)
{
    HANDLE mapping=NULL;struct stress_control *c=NULL;struct macdrv_status_cell *writer=NULL;
    struct macdrv_status_identity id={0};struct macdrv_status_values v;PROCESS_INFORMATION peer={0};STARTUPINFOA startup={0};
    char name[120],command[1400];uint64_t birth=0;uint32_t i=0;DWORD code=STILL_ACTIVE;BOOL clean=TRUE;ULONGLONG end=0;
    if(argc>1&&!strcmp(argv[1],"--reader"))return reader_main(argc,argv);
    if(!CHECK(argc==2))goto done;
    if(!CHECK(birth_of(GetCurrentProcess(),&birth)))goto done;
    snprintf(name,sizeof(name),"Local\\CiderStatusStress-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
    mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(*c),name);if(!CHECK(mapping!=NULL))goto done;
    c=MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*c));if(!CHECK(c!=NULL))goto done;
    memset(c,0,sizeof(*c));c->magic=0x53545253;c->version=1;
    startup.cb=sizeof(startup);snprintf(command,sizeof(command),"\"%s\" --reader %s",argv[1],name);
    if(!CHECK(CreateProcessA(NULL,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&peer)))goto done;
    if(!CHECK(wait_flag(&c->mapped,1,peer.hProcess,6000)))goto done;
    CHECK(c->pid==peer.dwProcessId&&c->bits==(sizeof(void*)==8?32u:64u));
    id.root_pid=GetCurrentProcessId();id.root_birth=birth;
    if(!CHECK(birth_of(peer.hProcess,&birth)&&birth==((uint64_t)c->birth_lo|((uint64_t)c->birth_hi<<32))))goto done;
    id.producer_pid=peer.dwProcessId;id.producer_birth=birth;id.kind=MACDRV_STATUS_PRODUCER;
    id.root_epoch=UINT64_C(8589934601);id.generation=UINT64_C(8589934602);id.lease=UINT64_C(8589934603);
    if(!CHECK(NtQueryInformationProcess(GetCurrentProcess(),ProcessSessionInformation,&id.session_id,sizeof(id.session_id),NULL)==0))goto done;
    values_for(0,&v);if(!CHECK(macdrv_status_cell_create(&id,&v,&writer)==0))goto done;
    if(!CHECK(macdrv_status_initialize(&c->identity,&id,&v)))goto done;
    store32(&c->writer_running,1);
    if(!CHECK(wait_flag(&c->mapped,2,peer.hProcess,6000)))goto done;
    if(!CHECK(wait_flag(&c->attempts,100,peer.hProcess,3000)))goto done;
    end=GetTickCount64()+8000;
    for(i=1;i<=ITERATIONS;++i)
    {
        values_for(i,&v);
        if(macdrv_status_cell_publish(writer,&v,0)!=MACDRV_STATUS_PUBLISHED)break;
        if(!(i&255)&&GetTickCount64()>=end)break;
        if(!(i&1023))SwitchToThread();
        if(i==ITERATIONS/2&&!wait_flag(&c->mid_snapshots,1,peer.hProcess,2000))break;
    }
    CHECK(i==ITERATIONS+1&&GetTickCount64()<end);
    store32(&c->writer_running,0);store32(&c->writer_done,1);
    {
        ULONGLONG deadline=GetTickCount64()+4000;
        CHECK(WaitForSingleObject(peer.hProcess,4000)==WAIT_OBJECT_0&&GetTickCount64()<deadline);
    }
    CHECK(GetExitCodeProcess(peer.hProcess,&code)&&code==0);
    CHECK(load32(&c->snapshots)>0&&load32(&c->retries)>0&&load32(&c->mid_snapshots)>0&&load32(&c->invalid)==0&&load32(&c->last_n)<=ITERATIONS);
    CHECK(macdrv_status_cell_close(&writer)==0&&writer==NULL);
done:
    if(c)store32(&c->abort,1);
    if(peer.hProcess)
    {
        DWORD state=WaitForSingleObject(peer.hProcess,0);BOOL terminated=FALSE;
        if(state!=WAIT_OBJECT_0){terminated=TerminateProcess(peer.hProcess,99);state=WaitForSingleObject(peer.hProcess,3000);}
        clean=state==WAIT_OBJECT_0&&clean;
        printf("{\"type\":\"status-stress-peer-cleanup\",\"pid\":%lu,\"wait\":%lu,\"terminate\":%u,\"confirmed\":%u}\n",peer.dwProcessId,state,(unsigned)terminated,(unsigned)(state==WAIT_OBJECT_0));
        clean=CloseHandle(peer.hThread)&&clean;clean=CloseHandle(peer.hProcess)&&clean;
    }
    clean=macdrv_status_cell_close(&writer)==0&&clean;clean=close_control(mapping,c)&&clean;
    printf("{\"type\":\"status-stress-owner\",\"bits\":%u,\"checks\":%u,\"failures\":%u,\"iterations\":%u,\"completed\":%u,\"cleanup\":%u}\n",(unsigned)(8*sizeof(void*)),checks,failures,ITERATIONS,i>ITERATIONS?ITERATIONS:i?i-1:0,(unsigned)clean);fflush(stdout);
    return failures||!clean?1:0;
}
