/* Exact extracted WinMM metadata bodies with deterministic COM/allocator doubles.
 * Native algorithm/ownership fixture, NOT Windows, CoreAudio or ABI evidence.
 * run-winmm-metadata-native.py generates metadata-under-test.inc from the full
 * prepared source. No devices, capture streams or real COM objects are used. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <limits.h>

typedef int32_t HRESULT;
typedef unsigned UINT;
typedef unsigned DWORD;
typedef uintptr_t DWORD_PTR;
typedef uint64_t ULONGLONG;
typedef wchar_t WCHAR;
typedef int BOOL;
typedef int EDataFlow;
typedef int ERole;
typedef int INIT_ONCE;
typedef struct { int value; } GUID;
typedef struct { uintptr_t Spare[1]; } DEBUG_INFO;
typedef struct { DEBUG_INFO *DebugInfo;int initialized; } CRITICAL_SECTION;
typedef struct { unsigned wMid,wPid,vDriverVersion,dwFormats,wReserved1,dwSupport,wChannels; WCHAR szPname[32]; } CAPS;
typedef struct { int flow,index; } IMMDevice;
typedef struct { int flow; } IMMDeviceCollection;
typedef struct { int marker; } IMMDeviceEnumerator;
typedef struct { int marker; } IMMNotificationClient;
typedef struct { int marker; } WINMM_Device;
typedef struct {
    CAPS out_caps,in_caps;WCHAR *dev_id;EDataFlow dataflow;
    void *volume;GUID session;UINT index,mixer_count;CRITICAL_SECTION lock;
    WINMM_Device *devices[4];
} WINMM_MMDevice;
#define WINAPI
#define TRUE 1
#define FALSE 0
#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL ((HRESULT)0x80004005u)
#define E_OUTOFMEMORY ((HRESULT)0x8007000eu)
#define E_NOTFOUND ((HRESULT)0x80070490u)
#define RPC_E_CHANGED_MODE ((HRESULT)0x80010106u)
#define FAILED(x) ((HRESULT)(x)<0)
#define SUCCEEDED(x) ((HRESULT)(x)>=0)
#define eRender 0
#define eCapture 1
#define eConsole 0
#define eMultimedia 1
#define eCommunications 2
#define DEVICE_STATE_ACTIVE 1
#define CLSCTX_INPROC_SERVER 1
#define RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO 1
#define WAVECAPS_LRVOLUME 1
#define WAVECAPS_VOLUME 2
#define WAVECAPS_SAMPLEACCURATE 4
#define MAX_DEVICES 4
#define MMSYSERR_NOERROR 0
#define MMSYSERR_ERROR 1
#define MMSYSERR_INVALHANDLE 5
#define MMSYSERR_NODRIVER 6
#define MMSYSERR_INVALPARAM 11
#define lstrlenW(s) ((UINT)wcslen(s))
#define TRACE(...) ((void)0)
#define WARN(...) ((void)0)
static GUID CLSID_MMDeviceEnumerator,IID_IMMDeviceEnumerator;
static WINMM_MMDevice *g_out_mmdevices,*g_in_mmdevices;
static WINMM_MMDevice **g_out_map,**g_in_map;
static UINT g_outmmdevices_count,g_inmmdevices_count,g_out_voicecom_id,g_in_voicecom_id;
static WINMM_Device *g_out_mapper_devices[MAX_DEVICES],*g_in_mapper_devices[MAX_DEVICES];
static CRITICAL_SECTION g_devthread_lock;
static IMMDeviceEnumerator *g_devenum;
static IMMNotificationClient g_notif;
static BOOL g_metadata_committed;
static ULONGLONG g_default_change_serial;
static WINMM_MMDevice *g_out_voicecom_device,*g_in_voicecom_device;
static INIT_ONCE init_once;
static HRESULT WINAPI notif_OnDefaultDeviceChanged(IMMNotificationClient *,EDataFlow,ERole,const WCHAR *);

static unsigned cases,checks;
static unsigned operations,allocations,fail_operation,fail_allocation;
static unsigned com_depth,locks,registered,registration_calls,map_lock_depth;
static unsigned endpoints[2],defaults[2][3];
static int changed_mode;
static int change_during_sampling;
static IMMDeviceEnumerator *registered_enumerator;
static IMMNotificationClient *registered_client;
static int registration_pending;
static int always_fail_operations,communications_fallback[2];
static unsigned recovery_sleeps;
static unsigned failure_schedule[4],failure_schedule_count;
struct allocation {void *base,*ptr;size_t size;int live;};
static struct allocation ledger[512];
static void require(int ok,const char *what)
{
    ++checks;
    if(!ok){fprintf(stderr,"FAIL case=%u check=%s operations=%u allocations=%u\n",cases,what,operations,allocations);exit(1);}
}
static int fail(void)
{
    unsigned i;++operations;
    if(always_fail_operations||operations==fail_operation)return 1;
    for(i=0;i<failure_schedule_count;++i)if(operations==failure_schedule[i])return 1;
    return 0;
}
static void *tracked_calloc(size_t n,size_t size)
{
    size_t bytes;unsigned i;unsigned char *base;
    ++allocations;if(allocations==fail_allocation)return NULL;
    require(!size||n<=(SIZE_MAX-64)/size,"allocation arithmetic");bytes=n*size;
    for(i=0;i<512&&ledger[i].live;++i){}
    require(i<512,"ledger capacity");base=calloc(1,bytes+64);require(base!=NULL,"fixture backing allocation");
    memset(base,0xa5,32);memset(base+32+bytes,0xa5,32);
    ledger[i]=(struct allocation){base,base+32,bytes,1};return base+32;
}
static void tracked_free(void *ptr)
{
    unsigned i,j;unsigned char *base;
    if(!ptr)return;
    for(i=0;i<512&&(!ledger[i].live||ledger[i].ptr!=ptr);++i){}
    require(i<512,"free owns live allocation");base=ledger[i].base;
    for(j=0;j<32;++j)require(base[j]==0xa5&&base[32+ledger[i].size+j]==0xa5,"allocation redzones intact");
    ledger[i].live=0;free(base);
}
static unsigned live_allocations(void){unsigned i,n=0;for(i=0;i<512;++i)n+=ledger[i].live!=0;return n;}
static int live_pointer(void *p){unsigned i;for(i=0;i<512;++i)if(ledger[i].live&&ledger[i].ptr==p)return 1;return 0;}
static WCHAR *device_id(int flow,unsigned index)
{
    WCHAR *p=tracked_calloc(32,sizeof(*p));if(p)swprintf(p,32,L"endpoint-%d-%u",flow,index);return p;
}
static HRESULT CoInitialize(void *unused)
{
    (void)unused;if(fail())return E_FAIL;if(changed_mode)return RPC_E_CHANGED_MODE;++com_depth;return S_OK;
}
static void CoUninitialize(void){require(com_depth>0,"balanced COM uninitialize");--com_depth;}
static HRESULT CoCreateInstance(const GUID *cls,void *outer,unsigned context,const GUID *iid,void **out)
{
    (void)cls;(void)outer;(void)context;(void)iid;*out=NULL;if(fail())return E_FAIL;
    *out=tracked_calloc(1,sizeof(IMMDeviceEnumerator));return *out?S_OK:E_OUTOFMEMORY;
}
static void IMMDeviceEnumerator_Release(IMMDeviceEnumerator *e)
{
    require(e!=registered_enumerator,"enumerator outlives its subscription");tracked_free(e);
}
static HRESULT IMMDeviceEnumerator_EnumAudioEndpoints(IMMDeviceEnumerator *e,EDataFlow flow,unsigned state,IMMDeviceCollection **out)
{
    (void)e;(void)state;*out=NULL;if(fail())return E_FAIL;
    *out=tracked_calloc(1,sizeof(**out));if(!*out)return E_OUTOFMEMORY;(*out)->flow=flow;return S_OK;
}
static HRESULT IMMDeviceCollection_GetCount(IMMDeviceCollection *c,UINT *count)
{
    if(fail())return E_FAIL;*count=endpoints[c->flow];return S_OK;
}
static void IMMDeviceCollection_Release(IMMDeviceCollection *c){tracked_free(c);}
static HRESULT IMMDeviceCollection_Item(IMMDeviceCollection *c,UINT index,IMMDevice **out)
{
    *out=NULL;if(fail())return E_FAIL;require(index<endpoints[c->flow],"collection index");
    *out=tracked_calloc(1,sizeof(**out));if(!*out)return E_OUTOFMEMORY;
    (*out)->flow=c->flow;(*out)->index=(int)index;return S_OK;
}
static HRESULT IMMDeviceEnumerator_GetDefaultAudioEndpoint(IMMDeviceEnumerator *e,EDataFlow flow,ERole role,IMMDevice **out)
{
    unsigned index=defaults[flow][role];
    (void)e;*out=NULL;if(fail())return E_FAIL;
    if(role==eCommunications&&communications_fallback[flow])index=defaults[flow][eConsole];
    if(index==UINT_MAX)return E_NOTFOUND;
    *out=tracked_calloc(1,sizeof(**out));if(!*out)return E_OUTOFMEMORY;
    (*out)->flow=flow;(*out)->index=(int)index;
    if(change_during_sampling&&flow==eCapture&&role==eCommunications)
    {
        change_during_sampling=0;defaults[0][0]=1;
        notif_OnDefaultDeviceChanged(&g_notif,eRender,eConsole,L"endpoint-0-1");
    }
    return S_OK;
}
static HRESULT IMMDevice_GetId(IMMDevice *d,WCHAR **out)
{
    int failed=fail();*out=device_id(d->flow,(unsigned)d->index);
    /* A deliberately partial output on failure exercises defensive ownership. */
    return failed?E_FAIL:(*out?S_OK:E_OUTOFMEMORY);
}
static void IMMDevice_Release(IMMDevice *d){require(d!=NULL,"no null COM release");tracked_free(d);}
static HRESULT WINMM_GetFriendlyName(IMMDevice *d,WCHAR *out,UINT length)
{
    if(fail())return E_FAIL;swprintf(out,length,L"fixture-%d-%d",d->flow,d->index);return S_OK;
}
static HRESULT CoCreateGuid(GUID *g){if(fail())return E_FAIL;g->value=7;return S_OK;}
static BOOL InitializeCriticalSectionEx(CRITICAL_SECTION *lock,DWORD spin,DWORD flags)
{
    (void)spin;(void)flags;if(fail())return FALSE;
    lock->DebugInfo=tracked_calloc(1,sizeof(*lock->DebugInfo));
    /* Match the reviewed Wine contract: TRUE can coexist with NULL DebugInfo. */
    lock->initialized=1;++locks;return TRUE;
}
static void DeleteCriticalSection(CRITICAL_SECTION *lock)
{
    require(locks>0&&lock->initialized,"delete initialized metadata lock");tracked_free(lock->DebugInfo);lock->DebugInfo=NULL;lock->initialized=0;--locks;
}
static void EnterCriticalSection(CRITICAL_SECTION *lock){require(lock==&g_devthread_lock,"only map lock entered");++map_lock_depth;}
static void LeaveCriticalSection(CRITICAL_SECTION *lock){require(lock==&g_devthread_lock&&map_lock_depth>0,"balanced map lock");--map_lock_depth;}
static HRESULT reroute_mapper_device(WINMM_Device *d,BOOL output){(void)d;(void)output;require(0,"fixture has no opened mapper devices");return E_FAIL;}
static HRESULT IMMDeviceEnumerator_RegisterEndpointNotificationCallback(IMMDeviceEnumerator *e,IMMNotificationClient *n)
{
    WCHAR id[32];int failed=fail();(void)n;++registration_calls;
    require(live_pointer(e)&&!registered&&!registration_pending&&n==&g_notif&&g_devenum==NULL&&map_lock_depth==0,"subscribe live enumerator once before publication outside map lock");
    require(!g_metadata_committed&&!g_outmmdevices_count&&!g_inmmdevices_count,"no partial tables published to early callbacks");
    registration_pending=1;
    if(endpoints[0]){swprintf(id,32,L"endpoint-0-%u",defaults[0][0]);notif_OnDefaultDeviceChanged(&g_notif,eRender,eConsole,id);}
    notif_OnDefaultDeviceChanged(&g_notif,eCapture,eConsole,NULL);
    notif_OnDefaultDeviceChanged(&g_notif,99,eConsole,L"ignored");
    registration_pending=0;
    if(failed)return E_FAIL;
    registered=1;registered_enumerator=e;registered_client=n;return S_OK;
}
static HRESULT IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(IMMDeviceEnumerator *e,IMMNotificationClient *n)
{
    require(map_lock_depth==0&&registered&&live_pointer(e)&&e==registered_enumerator&&n==registered_client,"unregister matching live subscription exactly once");
    registered=0;registered_enumerator=NULL;registered_client=NULL;return S_OK;
}
static void deliver_default(EDataFlow flow,ERole role,const WCHAR *id)
{
    require(registered&&registered_enumerator==g_devenum&&live_pointer(g_devenum)&&registered_client==&g_notif,"provider delivery needs committed live subscription");
    notif_OnDefaultDeviceChanged(registered_client,flow,role,id);
}
/* Serial gate double only: actual INIT_ONCE concurrency is a separate Wine check. */
static BOOL InitOnceExecuteOnce(INIT_ONCE *once,BOOL (*callback)(INIT_ONCE *,void *,void **),void *param,void **context)
{
    require(map_lock_depth==0,"metadata gate precedes map lock");
    if(*once)return TRUE;
    if(!callback(once,param,context))return FALSE;
    *once=1;return TRUE;
}
#define calloc tracked_calloc
#define free tracked_free
#define CoTaskMemFree tracked_free
#include "metadata-under-test.inc"
#undef calloc
#undef free
#ifdef PROVIDER_HELPERS
static struct {IMMDeviceEnumerator IMMDeviceEnumerator_iface;} enumerator;
static void fixture_sleep(unsigned ms){require(ms==50||ms==100,"bounded recovery delay");++recovery_sleeps;}
#define Sleep fixture_sleep
#include "provider-under-test.inc"
#undef Sleep
#endif

static void clear_success(void)
{
    WINMM_MMDevice *out=g_out_mmdevices,*in=g_in_mmdevices;WINMM_MMDevice **om=g_out_map,**im=g_in_map;
    IMMDeviceEnumerator *e=g_devenum;UINT oc=g_outmmdevices_count,ic=g_inmmdevices_count;
    require(e&&registered&&registered_enumerator==e&&registered_client==&g_notif,"successful snapshot retains matching subscription");
    if(e)IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(e,&g_notif);
    g_out_mmdevices=g_in_mmdevices=NULL;g_out_map=g_in_map=NULL;
    g_outmmdevices_count=g_inmmdevices_count=0;g_out_voicecom_id=g_in_voicecom_id=UINT_MAX;g_devenum=NULL;
    g_out_voicecom_device=g_in_voicecom_device=NULL;g_metadata_committed=FALSE;
    init_once=0;
    WINMM_FreeMMDevices(out,oc);WINMM_FreeMMDevices(in,ic);tracked_free(om);tracked_free(im);
    if(e)IMMDeviceEnumerator_Release(e);
}
static void balanced(void)
{
    require(!live_allocations()&&!locks&&!com_depth&&!registered&&!registered_enumerator&&!registered_client&&!registration_pending&&!map_lock_depth,"all fixture-owned state balanced");
    require(!g_out_mmdevices&&!g_in_mmdevices&&!g_out_map&&!g_in_map&&!g_outmmdevices_count&&!g_inmmdevices_count&&!g_devenum,"no partial globals escape");
}
static void reset(void)
{
    ++cases;balanced();operations=allocations=fail_operation=fail_allocation=registration_calls=0;changed_mode=0;change_during_sampling=0;g_default_change_serial=0;
    always_fail_operations=0;communications_fallback[0]=communications_fallback[1]=0;recovery_sleeps=0;
    memset(failure_schedule,0,sizeof(failure_schedule));failure_schedule_count=0;
    endpoints[0]=3;endpoints[1]=2;defaults[0][eConsole]=2;defaults[0][eCommunications]=0;defaults[1][eConsole]=0;defaults[1][eCommunications]=1;
}
static void mapping(UINT flow,UINT first,UINT voice)
{
    WINMM_MMDevice **map=flow?g_in_map:g_out_map;UINT count=flow?g_inmmdevices_count:g_outmmdevices_count;
    unsigned seen=0;UINT i;
    require(count==endpoints[flow],"exact endpoint count");require(!count||map[0]->index==first,"default or first collection endpoint first");
    require((flow?g_in_voicecom_id:g_out_voicecom_id)==voice,"communications index follows final map");
    for(i=0;i<count;++i){require(map[i]->index<count&&!(seen&(1u<<map[i]->index)),"each endpoint appears once");seen|=1u<<map[i]->index;}
}
int main(void)
{
    unsigned max_operations,max_allocations,i;BOOL ok;DWORD_PTR size;WCHAR id[32];
    reset();ok=WINMM_InitMMDevices(NULL,NULL,NULL);require(ok,"ordinary init");mapping(0,2,1);mapping(1,0,1);
    max_operations=operations;max_allocations=allocations;
    deliver_default(eRender,eConsole,L"endpoint-0-0");mapping(0,0,0);
    deliver_default(eRender,eCommunications,L"endpoint-0-1");mapping(0,0,2);
    deliver_default(eRender,eCommunications,NULL);mapping(0,0,UINT_MAX);
    deliver_default(eRender,eConsole,L"not-in-snapshot");mapping(0,0,UINT_MAX);
    clear_success();balanced();
    reset();defaults[0][eConsole]=defaults[0][eCommunications]=defaults[1][eConsole]=defaults[1][eCommunications]=UINT_MAX;
    require(WINMM_InitMMDevices(NULL,NULL,NULL),"no defaults still initializes");mapping(0,0,UINT_MAX);mapping(1,0,UINT_MAX);clear_success();balanced();
    reset();defaults[0][eCommunications]=2;require(WINMM_InitMMDevices(NULL,NULL,NULL),"shared default role");mapping(0,2,0);clear_success();balanced();
    reset();endpoints[0]=0;require(WINMM_InitMMDevices(NULL,NULL,NULL),"capture-only metadata");mapping(1,0,1);clear_success();balanced();
    reset();endpoints[1]=0;require(WINMM_InitMMDevices(NULL,NULL,NULL),"output-only metadata");mapping(0,2,1);clear_success();balanced();
    reset();endpoints[0]=endpoints[1]=0;require(!WINMM_InitMMDevices(NULL,NULL,NULL),"empty stays retryable");require(registration_calls==1,"empty attempt subscribes then unwinds");balanced();
    endpoints[0]=3;endpoints[1]=2;require(WINMM_InitMMDevices(NULL,NULL,NULL),"retry after empty");clear_success();balanced();
    reset();changed_mode=1;require(WINMM_InitMMDevices(NULL,NULL,NULL),"existing COM apartment accepted");clear_success();balanced();
    reset();change_during_sampling=1;require(!WINMM_InitMMDevices(NULL,NULL,NULL),"default change rejects stale snapshot");balanced();
    require(WINMM_InitMMDevices(NULL,NULL,NULL),"default change snapshot can retry");mapping(0,1,1);clear_success();balanced();
    reset();require(WINMM_QueryInstanceIDSize(0,NULL,TRUE)==MMSYSERR_INVALPARAM,"null size rejected before init");require(!operations,"invalid output has no side effects");
    require(WINMM_QueryInstanceIDSize(0,&size,TRUE)==MMSYSERR_NOERROR,"first size query initializes metadata");
    require(WINMM_QueryInstanceID(0,id,sizeof(id),TRUE)==MMSYSERR_NOERROR&&!wcscmp(id,L"endpoint-0-2"),"query reads committed endpoint ID");
    require(size==(wcslen(id)+1)*sizeof(WCHAR),"size includes full endpoint terminator");
    require(WINMM_QueryInstanceID(0,id,0,TRUE)==MMSYSERR_ERROR,"short ID output rejected");
    require(WINMM_QueryInstanceID(99,id,sizeof(id),TRUE)==MMSYSERR_INVALHANDLE,"invalid endpoint query rejected");clear_success();balanced();
    reset();fail_operation=3;require(WINMM_QueryInstanceIDSize(0,&size,TRUE)==MMSYSERR_NODRIVER,"failed subscription cannot expose pending metadata");balanced();
    fail_operation=0;require(WINMM_QueryInstanceIDSize(0,&size,TRUE)==MMSYSERR_NOERROR,"first-call query retries failed snapshot");clear_success();balanced();
    for(i=1;i<=max_operations;++i){reset();fail_operation=i;require(!WINMM_InitMMDevices(NULL,NULL,NULL),"each dependency failure rejects initialization");balanced();}
    for(i=1;i<=max_allocations;++i){reset();fail_allocation=i;require(!WINMM_InitMMDevices(NULL,NULL,NULL),"each allocation failure rejects initialization");balanced();}
#ifdef PROVIDER_HELPERS
    {
        WCHAR *old[4]={0},*next[4]={0};
        reset();communications_fallback[0]=1;
        require(sample_default_role_ids(old,FALSE)==S_OK&&!wcscmp(old[0],old[1]),"provider samples effective fallback roles");
        defaults[0][0]=1;fail_operation=operations+2;
        require(sample_default_role_ids(next,TRUE)==S_OK&&recovery_sleeps==1,"transient ID failure recovers existing subscription sample");
        require(!wcscmp(next[0],L"endpoint-0-1")&&!wcscmp(next[1],next[0]),"communications fallback follows changed console");
        require(!wcscmp(old[0],L"endpoint-0-2"),"old complete snapshot untouched by retries");
        free_default_role_ids(old);free_default_role_ids(next);memset(old,0,sizeof(old));memset(next,0,sizeof(next));balanced();
        reset();defaults[0][eCommunications]=2;require(sample_default_role_ids(old,FALSE)==S_OK,"explicit equal roles snapshot");
        defaults[0][0]=1;require(sample_default_role_ids(next,TRUE)==S_OK,"explicit role resample");
        require(!wcscmp(next[0],L"endpoint-0-1")&&!wcscmp(next[1],L"endpoint-0-2"),"explicit equal communications role stays fixed");
        free_default_role_ids(old);free_default_role_ids(next);memset(old,0,sizeof(old));memset(next,0,sizeof(next));balanced();
        reset();always_fail_operations=1;require(FAILED(sample_default_role_ids(next,TRUE))&&recovery_sleeps==2&&operations==3,"persistent failure has three attempts and two bounded delays");
        require(!next[0]&&!next[1]&&!next[2]&&!next[3],"failed partial sample cleared");always_fail_operations=0;balanced();
        reset();failure_schedule[0]=4;failure_schedule[1]=5;failure_schedule_count=2;
        require(sample_default_role_ids(next,TRUE)==S_OK&&recovery_sleeps==2,"partial allocation then earlier failure can recover without repeated free");
        free_default_role_ids(next);memset(next,0,sizeof(next));balanced();
        reset();failure_schedule[0]=4;failure_schedule[1]=5;failure_schedule[2]=6;failure_schedule_count=3;
        require(FAILED(sample_default_role_ids(next,TRUE))&&recovery_sleeps==2,"partial allocation then earlier failures exhaust safely");
        require(!next[0]&&!next[1]&&!next[2]&&!next[3],"exhausted partial outputs all cleared");balanced();
        for(i=1;i<=8;++i){reset();fail_operation=i;require(FAILED(sample_default_role_ids(next,FALSE)),"initial provider read failure stays a real failure");require(!next[0]&&!next[1]&&!next[2]&&!next[3],"every failed initial output cleared");require(!recovery_sleeps,"no initial hidden recovery");balanced();}
    }
#endif
    printf("{\"type\":\"complete\",\"scope\":\"native-exact-body-with-doubles\",\"cases\":%u,\"checks\":%u,\"operation_failures\":%u,\"allocation_failures\":%u,\"ok\":true}\n",cases,checks,max_operations,max_allocations);
    return fflush(stdout)||ferror(stdout)?2:0;
}
