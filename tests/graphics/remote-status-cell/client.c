#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include "fixture.h"
#include "wine/macdrv_remote.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct control
{
    uint32_t magic,version,pointer_bits,child_pid,birth_lo,birth_hi,checks,failures;
    uint32_t open_status,map_write_status,partial_close_status,protected_close_status;
    uint32_t close_handle_status,close_all_status,initial_read,update_read,final_read,final_protection;
    uint32_t expected_state,expected_pending_models,applied_lo,applied_hi,quiet_lo,quiet_hi;
    struct macdrv_status_wire expected;
};
_Static_assert(sizeof(struct control)==256,"cross-bit control256");
static unsigned checks,failures;
#define CHECK(x) check_value(!!(x),#x,__LINE__)
static BOOL check_value(BOOL value,const char *expression,unsigned line)
{
    ++checks;if(!value){++failures;printf("status-cell-fail %u line %u: %s\n",checks,line,expression);fflush(stdout);}return value;
}
static uint64_t file_time(FILETIME t){return(uint64_t)t.dwLowDateTime|((uint64_t)t.dwHighDateTime<<32);}
static BOOL birth_of(HANDLE process,uint64_t *out)
{
    FILETIME created,exited,kernel,user;if(!GetProcessTimes(process,&created,&exited,&kernel,&user))return FALSE;
    *out=file_time(created);return TRUE;
}
static BOOL pump_until(HANDLE handle,DWORD milliseconds)
{
    ULONGLONG end=GetTickCount64()+milliseconds;
    for(;;)
    {
        ULONGLONG now=GetTickCount64();DWORD result;MSG msg;
        if(now>=end)return FALSE;
        result=MsgWaitForMultipleObjects(1,&handle,FALSE,(DWORD)(end-now),QS_ALLINPUT);
        if(GetTickCount64()>=end)return FALSE;
        if(result==WAIT_OBJECT_0)return TRUE;
        if(result!=WAIT_OBJECT_0+1)return FALSE;
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE))
        {
            if(GetTickCount64()>=end)return FALSE;
            TranslateMessage(&msg);DispatchMessageW(&msg);
            if(GetTickCount64()>=end)return FALSE;
        }
    }
}
static BOOL values_match(const struct macdrv_status_snapshot *s,const struct control *c)
{
    uint64_t applied=(uint64_t)c->applied_lo|((uint64_t)c->applied_hi<<32);
    uint64_t quiet=(uint64_t)c->quiet_lo|((uint64_t)c->quiet_hi<<32);
    return s->values_valid && s->values.state==c->expected_state && s->values.pending_models==c->expected_pending_models &&
           s->values.applied_sequence==applied && s->values.quiescent_sequence==quiet;
}
static BOOL close_controls(HANDLE *events,HANDLE mapping,struct control *control)
{
    BOOL ok=TRUE;unsigned i;
    if(control)ok=UnmapViewOfFile(control)&&ok;
    for(i=0;i<7;++i)if(events[i])ok=CloseHandle(events[i])&&ok;
    if(mapping)ok=CloseHandle(mapping)&&ok;
    return ok;
}
static int reader_main(int argc,char **argv)
{
    HANDLE mapping=NULL,events[7]={0},borrowed;struct control *c=NULL;unsigned i;
    struct macdrv_status_identity identity;struct macdrv_status_snapshot snapshot;
    struct macdrv_status_cell *reader=NULL,*scratch=NULL;NTSTATUS status;uint64_t birth=0;
    void *bad_view=NULL;SIZE_T bad_size=0;MEMORY_BASIC_INFORMATION info;BOOL cleaned=TRUE;
    uint32_t ro_status=0,partial=0,protected_status=0,final_kind=0;
    if(!CHECK(argc==10))goto done;
    mapping=OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,argv[2]);if(!CHECK(mapping!=NULL))goto done;
    for(i=0;i<7;++i)events[i]=OpenEventA(EVENT_MODIFY_STATE|SYNCHRONIZE,FALSE,argv[3+i]);
    if(!CHECK(events[0]&&events[1]&&events[2]&&events[3]&&events[4]&&events[5]&&events[6]))goto done;
    c=MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*c));if(!CHECK(c!=NULL))goto done;
    if(!CHECK(birth_of(GetCurrentProcess(),&birth)))goto done;
    c->pointer_bits=8*sizeof(void*);c->child_pid=GetCurrentProcessId();c->birth_lo=(uint32_t)birth;c->birth_hi=(uint32_t)(birth>>32);
    if(!CHECK(SetEvent(events[0])))goto done;
    if(!CHECK(WaitForSingleObject(events[1],10000)==WAIT_OBJECT_0))goto done;
    if(!CHECK(c->magic==0x5354434c&&c->version==1))goto done;
    macdrv_status_read_identity(&c->expected,&identity);if(!CHECK(macdrv_status_identity_valid(&identity)))goto done;
    status=macdrv_status_cell_open(&identity,&reader);c->open_status=(uint32_t)status;if(!CHECK(status==0))goto done;
    c->initial_read=macdrv_status_cell_read(reader,&snapshot);if(!CHECK(c->initial_read==MACDRV_STATUS_READ_SNAPSHOT&&values_match(&snapshot,c)))goto done;
    if(!CHECK(VirtualQuery(macdrv_status_cell_view(reader,NULL),&info,sizeof(info))==sizeof(info)&&info.Protect==PAGE_READONLY&&info.State==MEM_COMMIT))goto done;
    status=NtMapViewOfSection(macdrv_status_cell_handle(reader),GetCurrentProcess(),&bad_view,0,0,NULL,&bad_size,2,0,PAGE_READWRITE);
    ro_status=c->map_write_status=(uint32_t)status;CHECK(status==STATUS_ACCESS_DENIED);
    if(bad_view){status=NtUnmapViewOfSection(GetCurrentProcess(),bad_view);if(!CHECK(status==0))goto done;bad_view=NULL;}
    else CHECK(TRUE);
    if(!CHECK(macdrv_status_cell_open(&identity,&scratch)==0))goto done;
    if(!CHECK(SetHandleInformation(macdrv_status_cell_handle(scratch),HANDLE_FLAG_PROTECT_FROM_CLOSE,HANDLE_FLAG_PROTECT_FROM_CLOSE)))goto done;
    partial=c->partial_close_status=(uint32_t)macdrv_status_cell_close(&scratch);
    if(!CHECK(partial==(uint32_t)STATUS_HANDLE_NOT_CLOSABLE&&scratch!=NULL))goto done;
    CHECK(macdrv_status_cell_view(scratch,NULL)==NULL&&macdrv_status_cell_read(scratch,&snapshot)==MACDRV_STATUS_READ_INVALID);
    if(!CHECK(SetHandleInformation(macdrv_status_cell_handle(scratch),HANDLE_FLAG_PROTECT_FROM_CLOSE,0)))goto done;
    if(!CHECK(macdrv_status_cell_close(&scratch)==0&&scratch==NULL))goto done;
    borrowed=macdrv_status_cell_handle(reader);
    if(!CHECK(SetHandleInformation(borrowed,HANDLE_FLAG_PROTECT_FROM_CLOSE,HANDLE_FLAG_PROTECT_FROM_CLOSE)))goto done;
    protected_status=c->protected_close_status=(uint32_t)macdrv_status_cell_close_handle(reader);
    CHECK(protected_status==(uint32_t)STATUS_HANDLE_NOT_CLOSABLE&&macdrv_status_cell_handle(reader)==borrowed);
    CHECK(macdrv_status_cell_read(reader,&snapshot)==MACDRV_STATUS_READ_SNAPSHOT&&values_match(&snapshot,c));
    if(!CHECK(SetHandleInformation(borrowed,HANDLE_FLAG_PROTECT_FROM_CLOSE,0)))goto done;
    c->close_handle_status=(uint32_t)macdrv_status_cell_close_handle(reader);CHECK(c->close_handle_status==0&&macdrv_status_cell_handle(reader)==NULL);
    CHECK(macdrv_status_cell_read(reader,&snapshot)==MACDRV_STATUS_READ_SNAPSHOT&&values_match(&snapshot,c));
    if(!CHECK(SetEvent(events[2])))goto done;
    if(!CHECK(WaitForSingleObject(events[3],10000)==WAIT_OBJECT_0))goto done;
    c->update_read=macdrv_status_cell_read(reader,&snapshot);CHECK(c->update_read==MACDRV_STATUS_READ_SNAPSHOT&&values_match(&snapshot,c));
    if(!CHECK(SetEvent(events[4])))goto done;
    if(!CHECK(WaitForSingleObject(events[5],10000)==WAIT_OBJECT_0))goto done;
    final_kind=c->final_read=macdrv_status_cell_read(reader,&snapshot);CHECK(final_kind==MACDRV_STATUS_READ_SAFE_ONLY&&snapshot.safe==1&&!snapshot.values_valid&&macdrv_status_identity_equal(&snapshot.identity,&identity));
    CHECK(VirtualQuery(macdrv_status_cell_view(reader,NULL),&info,sizeof(info))==sizeof(info)&&info.Protect==PAGE_READONLY&&info.State==MEM_COMMIT);c->final_protection=info.Protect;
    c->close_all_status=(uint32_t)macdrv_status_cell_close(&reader);CHECK(c->close_all_status==0&&reader==NULL);
    CHECK(SetEvent(events[6]));
done:
    if(bad_view)cleaned=NtUnmapViewOfSection(GetCurrentProcess(),bad_view)==0&&cleaned;
    if(scratch){HANDLE h=macdrv_status_cell_handle(scratch);if(h)cleaned=SetHandleInformation(h,HANDLE_FLAG_PROTECT_FROM_CLOSE,0)&&cleaned;cleaned=macdrv_status_cell_close(&scratch)==0&&cleaned;}
    if(reader){HANDLE h=macdrv_status_cell_handle(reader);if(h)cleaned=SetHandleInformation(h,HANDLE_FLAG_PROTECT_FROM_CLOSE,0)&&cleaned;cleaned=macdrv_status_cell_close(&reader)==0&&cleaned;}
    if(c){c->checks=checks;c->failures=failures;}
    cleaned=close_controls(events,mapping,c)&&cleaned;
    printf("{\"type\":\"status-cell-reader\",\"bits\":%u,\"pid\":%lu,\"checks\":%u,\"failures\":%u,\"map_write_status\":%u,\"partial_close_status\":%u,\"protected_close_status\":%u,\"final_read\":%u,\"cleanup\":%u}\n",(unsigned)(8*sizeof(void*)),GetCurrentProcessId(),checks,failures,ro_status,partial,protected_status,final_kind,(unsigned)cleaned);fflush(stdout);
    return failures||!cleaned?1:0;
}
typedef NTSTATUS(WINAPI *retire_fn)(struct retirement_fixture *);
static BOOL retire_call(retire_fn fn,struct retirement_fixture *frame,unsigned op,HWND root,HWND child)
{
    frame->size=sizeof(*frame);frame->version=1;frame->operation=op;frame->status=ERROR_INVALID_DATA;
    frame->root=(uint32_t)(uintptr_t)root;frame->child=(uint32_t)(uintptr_t)child;frame->reserved=0;
    return fn(frame)==0&&(frame->status==0||(op==5&&frame->status==ERROR_IO_PENDING));
}
static BOOL retire_poll(retire_fn fn,struct retirement_fixture *frame,unsigned op,HWND root,HWND child,BOOL completion)
{
    ULONGLONG end=GetTickCount64()+3000;
    for(;;)
    {
        MSG msg;BOOL ready;
        if(GetTickCount64()>=end||!retire_call(fn,frame,op,root,child)||GetTickCount64()>=end)return FALSE;
        ready=completion?frame->completion_seen==1:
            (!frame->model_found&&!frame->retirement_total&&!frame->pe_watches&&!frame->pe_timers&&(op!=5||frame->status==0));
        if(GetTickCount64()>=end)return FALSE;
        if(ready)return TRUE;
        MsgWaitForMultipleObjectsEx(0,NULL,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(GetTickCount64()>=end)return FALSE;
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){if(GetTickCount64()>=end)return FALSE;
            TranslateMessage(&msg);DispatchMessageW(&msg);if(GetTickCount64()>=end)return FALSE;}
    }
}
static void expected_values(struct control *c,const struct macdrv_status_values *v)
{
    c->expected_state=v->state;c->expected_pending_models=v->pending_models;
    c->applied_lo=(uint32_t)v->applied_sequence;c->applied_hi=(uint32_t)(v->applied_sequence>>32);
    c->quiet_lo=(uint32_t)v->quiescent_sequence;c->quiet_hi=(uint32_t)(v->quiescent_sequence>>32);
}
static void codec_checks(const struct macdrv_status_identity *id)
{
    struct macdrv_status_wire wire={0};struct macdrv_status_snapshot snapshot;struct macdrv_status_identity wrong=*id;
    struct macdrv_status_values v={MACDRV_STATUS_ACTIVE,0,0,0,0,UINT64_C(8589934620),UINT64_C(8589934619)};
    CHECK(macdrv_status_read_wire(&wire,id,&snapshot)==MACDRV_STATUS_READ_RETRY);
    CHECK(macdrv_status_initialize(&wire,id,&v));CHECK(!macdrv_status_initialize(&wire,id,&v));
    CHECK(macdrv_status_read_wire(&wire,id,&snapshot)==MACDRV_STATUS_READ_SNAPSHOT&&snapshot.values_valid&&snapshot.values.applied_sequence==v.applied_sequence);
    wrong.generation+=UINT64_C(4294967296);CHECK(macdrv_status_read_wire(&wire,&wrong,&snapshot)==MACDRV_STATUS_READ_MISMATCH);
    snapshot.identity=wrong;
    CHECK(macdrv_status_read_wire(&wire,&snapshot.identity,&snapshot)==MACDRV_STATUS_READ_MISMATCH);
    macdrv_status_store(&wire,6,2);CHECK(macdrv_status_read_wire(&wire,id,&snapshot)==MACDRV_STATUS_READ_INVALID);macdrv_status_store(&wire,6,0);
    macdrv_status_store(&wire,3,3);CHECK(macdrv_status_read_wire(&wire,id,&snapshot)==MACDRV_STATUS_READ_RETRY);
    macdrv_status_store(&wire,3,UINT32_MAX-1);v.applied_sequence++;
    CHECK(macdrv_status_publish_wire(&wire,&v,0)==MACDRV_STATUS_PUBLISH_EXHAUSTED&&macdrv_status_load(&wire,3)==UINT32_MAX-1);
    CHECK(macdrv_status_read_wire(&wire,id,&snapshot)==MACDRV_STATUS_READ_EXHAUSTED&&!snapshot.values_valid&&!snapshot.safe);
    v.state=MACDRV_STATUS_RETIRED;v.quiescent_sequence=v.applied_sequence;
    CHECK(macdrv_status_publish_wire(&wire,&v,1)==MACDRV_STATUS_PUBLISH_EXHAUSTED);
    CHECK(macdrv_status_read_wire(&wire,id,&snapshot)==MACDRV_STATUS_READ_SAFE_ONLY&&snapshot.safe&&!snapshot.values_valid);
    snapshot.identity=wrong;
    CHECK(macdrv_status_read_wire(&wire,&snapshot.identity,&snapshot)==MACDRV_STATUS_READ_MISMATCH);
    macdrv_status_store(&wire,33,1);CHECK(macdrv_status_read_wire(&wire,id,&snapshot)==MACDRV_STATUS_READ_INVALID);
}
int main(int argc,char **argv)
{
    char prefix[96],mapname[120],names[7][128],command[1800];HANDLE events[7]={0},mapping=NULL;
    struct control *control=NULL;PROCESS_INFORMATION peer={0};STARTUPINFOA startup={0};unsigned i;
    HINSTANCE instance=GetModuleHandleW(NULL);WNDCLASSW cls={0};HWND root=NULL,child=NULL;ATOM atom=0;
    HMODULE driver=NULL;retire_fn retire=NULL;struct retirement_fixture frame={0};struct macdrv_remote_packet query={0};DWORD_PTR handled=0;
    struct macdrv_status_identity identity={0};struct macdrv_status_values values={MACDRV_STATUS_ACTIVE,0,0,0,0,UINT64_C(8589934620),UINT64_C(8589934619)};
    struct macdrv_status_cell *writer=NULL,*duplicate=NULL;DWORD code=STILL_ACTIVE;uint64_t peer_birth=0;NTSTATUS status;BOOL clean=TRUE,root_destroyed=FALSE;
    if(argc>1&&!strcmp(argv[1],"--reader"))return reader_main(argc,argv);
    if(!CHECK(argc==2))goto done;
    cls.lpfnWndProc=DefWindowProcW;cls.hInstance=instance;cls.lpszClassName=L"CiderStatusCellActualRoot";
    atom=RegisterClassW(&cls);if(!CHECK(atom!=0))goto done;
    root=CreateWindowW(cls.lpszClassName,L"Cider status cell",WS_OVERLAPPEDWINDOW|WS_VISIBLE,30,30,320,240,NULL,NULL,instance,NULL);if(!CHECK(root!=NULL))goto done;
    child=CreateWindowW(cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_BORDER,10,10,120,80,root,NULL,instance,NULL);if(!CHECK(child!=NULL))goto done;
    driver=LoadLibraryW(L"winemac.drv");if(!CHECK(driver!=NULL))goto done;retire=(retire_fn)(uintptr_t)GetProcAddress(driver,"cider_retirement_fixture");if(!CHECK(retire!=NULL))goto done;
    query.size=sizeof(query);query.version=MACDRV_REMOTE_PACKET_VERSION;query.operation=MACDRV_REMOTE_QUERY_ROOT;query.root=(uint32_t)(uintptr_t)root;
    if(!CHECK(SendMessageTimeoutW(root,WM_MACDRV_REMOTE_HOST_PACKET,0,(LPARAM)&query,SMTO_ABORTIFHUNG,3000,&handled)&&handled==1&&query.status==0&&query.flags==1))goto done;
    if(!CHECK(retire_call(retire,&frame,1,root,child)&&frame.context_id&&frame.lease>=UINT64_C(8589934592)&&frame.ticket>=UINT64_C(8589934592)&&frame.model_found==1))goto done;
    identity.kind=MACDRV_STATUS_HOST;identity.root_pid=query.root_pid;identity.root_birth=query.root_create_time;identity.root_epoch=query.root_epoch;
    identity.producer_pid=GetCurrentProcessId();identity.producer_birth=query.root_create_time;identity.generation=UINT64_C(8589934592);identity.lease=frame.lease;identity.ticket=frame.ticket;identity.context_id=frame.context_id;
    if(!CHECK(NtQueryInformationProcess(GetCurrentProcess(),ProcessSessionInformation,&identity.session_id,sizeof(identity.session_id),NULL)==0))goto done;
    codec_checks(&identity);
    snprintf(prefix,sizeof(prefix),"Local\\CiderStatus-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());snprintf(mapname,sizeof(mapname),"%s-map",prefix);
    mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(*control),mapname);if(!CHECK(mapping!=NULL))goto done;
    control=MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*control));if(!CHECK(control!=NULL))goto done;memset(control,0,sizeof(*control));control->magic=0x5354434c;control->version=1;
    for(i=0;i<7;++i){snprintf(names[i],sizeof(names[i]),"%s-event%u",prefix,i);events[i]=CreateEventA(NULL,TRUE,FALSE,names[i]);}if(!CHECK(events[0]&&events[1]&&events[2]&&events[3]&&events[4]&&events[5]&&events[6]))goto done;
    startup.cb=sizeof(startup);snprintf(command,sizeof(command),"\"%s\" --reader %s %s %s %s %s %s %s %s",argv[1],mapname,names[0],names[1],names[2],names[3],names[4],names[5],names[6]);
    if(!CHECK(CreateProcessA(NULL,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&peer)))goto done;
    if(!CHECK(pump_until(events[0],6000)))goto done;
    CHECK(control->pointer_bits==(sizeof(void*)==8?32u:64u)&&control->child_pid==peer.dwProcessId);
    CHECK(birth_of(peer.hProcess,&peer_birth)&&peer_birth==((uint64_t)control->birth_lo|((uint64_t)control->birth_hi<<32)));
    status=macdrv_status_cell_create(&identity,&values,&writer);if(!CHECK(status==0&&writer!=NULL))goto done;
    status=macdrv_status_cell_create(&identity,&values,&duplicate);CHECK(status==STATUS_OBJECT_NAME_COLLISION&&duplicate!=NULL);CHECK(macdrv_status_cell_close(&duplicate)==0&&duplicate==NULL);
    CHECK(macdrv_status_initialize(&control->expected,&identity,&values));expected_values(control,&values);if(!CHECK(SetEvent(events[1])))goto done;
    if(!CHECK(pump_until(events[2],6000)))goto done;
    if(!CHECK(retire_call(retire,&frame,2,root,child)&&retire_poll(retire,&frame,3,root,child,TRUE)))goto done;
    CHECK(frame.pending==1&&frame.model_found==1&&frame.watched==1&&!frame.finalized);
    values.state=MACDRV_STATUS_REVOKING;values.pending_models=frame.pending;values.applied_sequence+=7;values.quiescent_sequence+=6;
    CHECK(macdrv_status_cell_publish(writer,&values,0)==MACDRV_STATUS_PUBLISHED);expected_values(control,&values);if(!CHECK(SetEvent(events[3])))goto done;
    if(!CHECK(pump_until(events[4],6000)))goto done;
    if(!CHECK(retire_call(retire,&frame,4,root,child)&&retire_poll(retire,&frame,3,root,child,FALSE)))goto done;
    values.state=MACDRV_STATUS_RETIRED;values.pending_models=0;values.quiescent_sequence=values.applied_sequence;
    CHECK(macdrv_status_cell_publish(writer,&values,1)==MACDRV_STATUS_PUBLISHED);
    if(!CHECK(macdrv_status_cell_close(&writer)==0&&writer==NULL))goto done;
    if(!CHECK(retire_poll(retire,&frame,5,root,child,FALSE)))goto done;
    if(!CHECK(DestroyWindow(child)))goto done;
    child=NULL;
    if(!CHECK(DestroyWindow(root)))goto done;
    root=NULL;root_destroyed=TRUE;
    if(!CHECK(SetEvent(events[5])))goto done;
    if(!CHECK(pump_until(events[6],6000)))goto done;
    CHECK(pump_until(peer.hProcess,4000));CHECK(GetExitCodeProcess(peer.hProcess,&code)&&code==0);
    CHECK(control->checks==35&&!control->failures&&control->map_write_status==(uint32_t)STATUS_ACCESS_DENIED&&control->partial_close_status==(uint32_t)STATUS_HANDLE_NOT_CLOSABLE&&control->protected_close_status==(uint32_t)STATUS_HANDLE_NOT_CLOSABLE&&control->final_read==MACDRV_STATUS_READ_SAFE_ONLY);
done:
    if(retire&&!root_destroyed)clean=retire_poll(retire,&frame,5,root,child,FALSE)&&clean;
    if(peer.hProcess)
    {
        DWORD state=WaitForSingleObject(peer.hProcess,0);BOOL killed=FALSE;
        if(state!=WAIT_OBJECT_0){killed=TerminateProcess(peer.hProcess,99);(void)pump_until(peer.hProcess,3000);state=WaitForSingleObject(peer.hProcess,0);}
        clean=(state==WAIT_OBJECT_0)&&clean;
        printf("{\"type\":\"status-cell-peer-cleanup\",\"pid\":%lu,\"wait\":%lu,\"terminate\":%u,\"confirmed\":%u}\n",peer.dwProcessId,state,(unsigned)killed,(unsigned)(state==WAIT_OBJECT_0));
        clean=CloseHandle(peer.hThread)&&clean;clean=CloseHandle(peer.hProcess)&&clean;
    }
    clean=macdrv_status_cell_close(&duplicate)==0&&clean;clean=macdrv_status_cell_close(&writer)==0&&clean;
    if(child)clean=DestroyWindow(child)&&clean;
    if(root)clean=DestroyWindow(root)&&clean;
    if(driver)clean=FreeLibrary(driver)&&clean;
    if(atom)clean=UnregisterClassW(cls.lpszClassName,instance)&&clean;
    clean=close_controls(events,mapping,control)&&clean;
    printf("{\"type\":\"status-cell-owner\",\"bits\":%u,\"checks\":%u,\"failures\":%u,\"root_destroyed_before_final_read\":%u,\"cleanup\":%u}\n",(unsigned)(8*sizeof(void*)),checks,failures,(unsigned)root_destroyed,(unsigned)clean);fflush(stdout);
    return failures||!clean||!root_destroyed?1:0;
}
