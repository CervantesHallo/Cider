#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include "fixture.h"
#include <stdio.h>
#include <string.h>
typedef NTSTATUS(WINAPI *fixture_fn)(void *);
static unsigned checks,failures,snapshots;
#define CHECK(x) check_value(!!(x),#x,__LINE__)
static BOOL check_value(BOOL value,const char *name,unsigned line)
{
    ++checks;if(!value)++failures;
    printf("{\"type\":\"recovery-fifo\",\"event\":\"check\",\"check\":%u,\"ok\":%u,\"line\":%u}\n",checks,(unsigned)value,line);
    if(!value)printf("fifo-fail %s\n",name);
    fflush(stdout);return value;
}
static BOOL call(fixture_fn fn,struct recovery_fifo_fixture *f,unsigned op,unsigned target,HWND root)
{
    f->size=sizeof(*f);f->version=2;f->operation=op;f->target=target;f->root=(uint32_t)(uintptr_t)root;f->status=ERROR_INVALID_DATA;f->reserved[0]=f->reserved[1]=0;
    NTSTATUS status=fn(f);
    printf("{\"type\":\"recovery-fifo\",\"event\":\"call\",\"op\":%u,\"ntstatus\":%u,\"status\":%u,\"acquire_error\":%u}\n",op,(unsigned)(uint32_t)status,f->status,f->acquire_error);fflush(stdout);
    return status==0&&f->size==sizeof(*f)&&f->version==2&&f->operation==op&&f->target==target&&!f->reserved[0]&&!f->reserved[1];
}
static void sample(const char *phase,const struct recovery_fifo_fixture *f)
{
    printf("{\"type\":\"recovery-fifo\",\"event\":\"snapshot\",\"phase\":\"%s\",\"index\":%u,\"older\":%llu,\"newer\":%llu,\"flags\":%u,\"setups\":%u,\"queued\":%u,\"busy\":%u,\"active\":%u,\"older_present\":%u,\"newer_present\":%u,\"older_last\":%u,\"newer_last\":%u,\"older_retries\":%u,\"newer_retries\":%u,\"pe_watches\":%u,\"pe_timers\":%u,\"query\":%u,\"set\":%u,\"older_position\":%u,\"newer_position\":%u}\n",phase,++snapshots,(unsigned long long)f->older,(unsigned long long)f->newer,f->flags,f->setups,f->queued,f->busy,f->active,f->older_present,f->newer_present,f->older_last,f->newer_last,f->older_retries,f->newer_retries,f->pe_watches,f->pe_timers,f->query_status,f->set_status,f->older_position,f->newer_position);fflush(stdout);
}
static BOOL poll(fixture_fn fn,struct recovery_fifo_fixture *f,HWND root,unsigned goal)
{
    ULONGLONG start=GetTickCount64(),end=start+4000;
    for(;;)
    {
        ULONGLONG now=GetTickCount64();BOOL ready;
        if(now<start||now>=end||!call(fn,f,2,0,root)||GetTickCount64()>=end||f->status)return FALSE;
        sample(goal==0?"baseline":goal==1?"fairness":goal==3?"order":"settled",f);
        if(f->pe_watches)return FALSE;
        if(goal==0)ready=f->setups==2&&f->older_present&&f->newer_present&&(f->flags&32)&&!(f->flags&256)&&f->older_last==(uint32_t)STATUS_HANDLE_NOT_CLOSABLE&&f->newer_last==(uint32_t)STATUS_HANDLE_NOT_CLOSABLE&&f->pe_timers>0;
        else if(goal==3)ready=(f->flags&64)&&!(f->flags&256);
        else if(goal==1)
        {
            if(!(f->flags&64)||(f->flags&256))return FALSE;
            if(!(f->flags&128))return FALSE;
            ready=f->setups==1&&f->older_present&&!f->newer_present&&(f->flags&7)==4&&f->older_last==(uint32_t)STATUS_HANDLE_NOT_CLOSABLE&&f->newer_retries>0&&f->newer_last==0;
        }
        else ready=f->setups==0&&f->queued==0&&f->busy==0&&f->active==0&&!f->older_present&&!f->newer_present&&!f->pe_watches&&!f->pe_timers&&(f->flags&7)==(f->older?2u:0u)+(f->newer?4u:0u);
        if(GetTickCount64()>=end)return FALSE;
        if(ready)return TRUE;
        Sleep(10);
    }
}
static BOOL clear_owner(fixture_fn fn,struct recovery_fifo_fixture *f,HWND root,unsigned target,BOOL *applied)
{
    ULONGLONG start=GetTickCount64(),end=start+4000;
    for(;;)
    {
        if(GetTickCount64()<start||GetTickCount64()>=end)return FALSE;
        if(!call(fn,f,3,target,root))return FALSE;
        if(!f->status){*applied=TRUE;return GetTickCount64()<end;}
        if(GetTickCount64()>=end||f->status!=ERROR_IO_PENDING)return FALSE;
        Sleep(10);
    }
}
int main(int argc,char **argv)
{
    WNDCLASSW cls={0};HINSTANCE instance=GetModuleHandleW(NULL);ATOM atom=0;HWND root=NULL;HMODULE driver=NULL;fixture_fn fn=NULL;
    struct recovery_fifo_fixture f={0};BOOL clean=TRUE,root_destroyed=FALSE,clear_old=FALSE,clear_new=FALSE,settled=FALSE,fair=FALSE;uint64_t older=0,newer=0;
    (void)argv;
    if(!CHECK(argc==1))goto cleanup;
    cls.lpfnWndProc=DefWindowProcW;cls.hInstance=instance;cls.lpszClassName=L"CiderRecoveryFifoRoot";atom=RegisterClassW(&cls);if(!CHECK(atom!=0))goto cleanup;
    root=CreateWindowW(cls.lpszClassName,L"Cider FIFO",WS_OVERLAPPEDWINDOW|WS_VISIBLE,20,20,320,240,NULL,NULL,instance,NULL);if(!CHECK(root!=NULL))goto cleanup;
    driver=LoadLibraryW(L"winemac.drv");if(!CHECK(driver!=NULL))goto cleanup;
    fn=(fixture_fn)(uintptr_t)GetProcAddress(driver,"cider_recovery_fifo_fixture");if(!CHECK(fn!=NULL))goto cleanup;
    {
        ULONGLONG begin=GetTickCount64();BOOL returned=call(fn,&f,1,0,root);
        if(returned){older=f.older;newer=f.newer;}
        if(!CHECK(returned&&GetTickCount64()>=begin&&GetTickCount64()<begin+4000&&f.status==0))goto cleanup;
    }
    if(!CHECK(older>=UINT64_C(8589934592)&&newer>older&&f.acquire_error==RtlNtStatusToDosError(STATUS_HANDLE_NOT_CLOSABLE)))goto cleanup;
    if(!CHECK(poll(fn,&f,root,0)))goto cleanup;
    if(!CHECK((f.flags&7)==1&&(f.flags&32)&&!(f.flags&256)&&f.pe_watches==0&&f.pe_timers>0))goto cleanup;
    if(!CHECK(DestroyWindow(root)))goto cleanup;
    root=NULL;root_destroyed=TRUE;
    {
        ULONGLONG begin=GetTickCount64();BOOL returned=call(fn,&f,2,0,root);
        if(!CHECK(returned&&GetTickCount64()>=begin&&GetTickCount64()<begin+4000&&f.status==0&&!(f.flags&1)&&f.older_present&&f.newer_present))goto cleanup;
    }
    if(!CHECK(clear_owner(fn,&f,root,2,&clear_new)))goto cleanup;
    if(!CHECK(clear_new&&!(f.flags&2)&&(f.flags&4)&&f.query_status==0&&f.set_status==0&&poll(fn,&f,root,3)))goto cleanup;
    fair=poll(fn,&f,root,1);
    CHECK(fair); /* CHECK14: exact before failure, then complete real cleanup */
    if(!CHECK(clear_owner(fn,&f,root,1,&clear_old)))goto cleanup;
    if(!CHECK(clear_old&&(f.flags&6)==6))goto cleanup;
    settled=poll(fn,&f,root,2);if(!CHECK(settled))goto cleanup;
    CHECK(f.older==older&&f.newer==newer);
    if(!CHECK(FreeLibrary(driver)))goto cleanup;
    driver=NULL;
    if(!CHECK(UnregisterClassW(cls.lpszClassName,instance)))goto cleanup;
    atom=0;
cleanup:
    if(root){if(DestroyWindow(root)){root=NULL;root_destroyed=TRUE;}else clean=FALSE;}
    if(fn&&driver)
    {
        if(older&&!clear_old)clean=clear_owner(fn,&f,root,1,&clear_old)&&clean;
        if(newer&&!clear_new)clean=clear_owner(fn,&f,root,2,&clear_new)&&clean;
        if(!settled)settled=poll(fn,&f,root,2);
        clean=settled&&clean;
    }
    if(driver)clean=FreeLibrary(driver)&&clean;
    if(atom)clean=UnregisterClassW(cls.lpszClassName,instance)&&clean;
    printf("{\"type\":\"recovery-fifo\",\"event\":\"summary\",\"bits\":%u,\"checks\":%u,\"failures\":%u,\"fair\":%u,\"root_destroyed\":%u,\"clear_old\":%u,\"clear_new\":%u,\"settled\":%u,\"cleanup\":%u,\"older\":%llu,\"newer\":%llu,\"older_retries\":%u,\"newer_retries\":%u}\n",(unsigned)(8*sizeof(void*)),checks,failures,(unsigned)fair,(unsigned)root_destroyed,(unsigned)clear_old,(unsigned)clear_new,(unsigned)settled,(unsigned)clean,(unsigned long long)older,(unsigned long long)newer,f.older_retries,f.newer_retries);fflush(stdout);
    return failures||!clean||!settled?1:0;
}
