/* Cold WinMM wave/instance entry plus concurrent mixer metadata. No input stream
 * or audio buffer is opened/submitted. A real output handle is opened/closed.
 * Requires run-mac.py --suite winmm-startup for mandatory owned-root cleanup. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <mmddk.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks,failures;
static HANDLE ready,stop;
struct result {MMRESULT opened,caps,closed;DWORD returned;};
static struct result results[4];
static void fatal(void){TerminateProcess(GetCurrentProcess(),121);ExitProcess(121);}
static void check(const char *name,int ok,uint64_t actual,uint64_t expected)
{
    ++checks;if(!ok)++failures;
    printf("{\"type\":\"check\",\"name\":\"%s\",\"ok\":%s,\"actual\":%" PRIu64 ",\"expected\":%" PRIu64 "}\n",name,ok?"true":"false",actual,expected);
    if(fflush(stdout)||ferror(stdout))fatal();
}
static DWORD WINAPI watchdog(void *unused)
{
    DWORD status;(void)unused;status=WaitForSingleObject(stop,25000);
    if(status!=WAIT_OBJECT_0)TerminateProcess(GetCurrentProcess(),124);
    return status==WAIT_OBJECT_0?0:124;
}
static DWORD WINAPI metadata(void *opaque)
{
    unsigned i=(unsigned)(uintptr_t)opaque;HMIXER mixer=NULL;MIXERCAPSW caps={0};
    struct result *r=&results[i];
    if(WaitForSingleObject(ready,5000)!=WAIT_OBJECT_0)return 1;
    r->opened=mixerOpen(&mixer,0,0,0,MIXER_OBJECTF_MIXER);
    if(r->opened==MMSYSERR_NOERROR&&mixer)
    {
        r->caps=mixerGetDevCapsW((UINT_PTR)mixer,&caps,sizeof(caps));
        if(r->caps==MMSYSERR_NOERROR&&!caps.szPname[0])r->caps=MMSYSERR_ERROR;
        r->closed=mixerClose(mixer);
    }
    return r->opened||r->caps||r->closed?1:0;
}
int main(int argc,char **argv)
{
    HANDLE timer,threads[4];DWORD status,code;unsigned i,count;MMRESULT first,closed=MMSYSERR_NOERROR;
    int instance=argc==2&&!strcmp(argv[1],"instance"),payload=0;
    if(argc!=2||(!instance&&strcmp(argv[1],"wave")))return 2;
    printf("{\"type\":\"begin\",\"subject\":\"winmm-startup\",\"mode\":\"correctness\",\"first\":\"%s\",\"pointer_bits\":%u,\"pid\":%lu}\n",argv[1],(unsigned)(8*sizeof(void*)),GetCurrentProcessId());
    if(fflush(stdout)||ferror(stdout))fatal();
    stop=CreateEventW(NULL,TRUE,FALSE,NULL);check("watchdog.stop_event",stop!=NULL,stop!=NULL,1);if(!stop)fatal();
    timer=CreateThread(NULL,0,watchdog,NULL,0,NULL);check("watchdog.create",timer!=NULL,timer!=NULL,1);if(!timer)fatal();
    /* No WinMM call may precede this selected cold entry. */
    if(instance)
    {
        DWORD_PTR size=0;WCHAR *id=NULL;
        first=waveOutMessage((HWAVEOUT)0,DRV_QUERYFUNCTIONINSTANCEIDSIZE,(DWORD_PTR)&size,0);
        if(first==MMSYSERR_NOERROR&&size>=sizeof(WCHAR)&&size<=4096&&!(size%sizeof(WCHAR)))
        {
            id=malloc((size_t)size);if(!id)fatal();memset(id,0xa5,(size_t)size);
            first=waveOutMessage((HWAVEOUT)0,DRV_QUERYFUNCTIONINSTANCEID,(DWORD_PTR)id,size);
            payload=first==MMSYSERR_NOERROR&&id[0]&&id[size/sizeof(WCHAR)-1]==0;free(id);
        }
    }
    else
    {
        WAVEFORMATEX format={WAVE_FORMAT_PCM,2,48000,192000,4,16,0};HWAVEOUT output=NULL;
        first=waveOutOpen(&output,WAVE_MAPPER,&format,0,0,CALLBACK_NULL);
        payload=first==MMSYSERR_NOERROR&&output!=NULL;
        if(output)closed=waveOutClose(output);
    }
    check("first.code",first==MMSYSERR_NOERROR,first,MMSYSERR_NOERROR);
    check("first.cleanup",closed==MMSYSERR_NOERROR,closed,MMSYSERR_NOERROR);
    check("first.payload",payload,payload,1);
    if(first||closed||!payload)fatal();
    count=waveOutGetNumDevs();check("output.count",count>0,count>0,1);
    ready=CreateEventW(NULL,TRUE,FALSE,NULL);check("ready.create",ready!=NULL,ready!=NULL,1);if(!ready)fatal();
    for(i=0;i<4;++i)
    {
        results[i].opened=results[i].caps=results[i].closed=MMSYSERR_ERROR;
        threads[i]=CreateThread(NULL,0,metadata,(void*)(uintptr_t)i,0,NULL);
        check("thread.create",threads[i]!=NULL,threads[i]!=NULL,1);if(!threads[i])fatal();
    }
    status=SetEvent(ready)!=FALSE;check("ready.set",status,status,1);if(!status)fatal();
    status=WaitForMultipleObjects(4,threads,TRUE,10000);check("thread.join",status==WAIT_OBJECT_0,status,WAIT_OBJECT_0);if(status!=WAIT_OBJECT_0)fatal();
    for(i=0;i<4;++i)
    {
        check("worker.open",!results[i].opened,results[i].opened,0);
        check("worker.caps",!results[i].caps,results[i].caps,0);
        check("worker.close",!results[i].closed,results[i].closed,0);
        if(!GetExitCodeThread(threads[i],&code))fatal();
        check("worker.exit",code==0,code,0);
        status=CloseHandle(threads[i])!=FALSE;check("thread.close",status,status,1);if(!status)fatal();
    }
    status=CloseHandle(ready)!=FALSE;check("ready.close",status,status,1);if(!status)fatal();
    status=SetEvent(stop)!=FALSE;check("watchdog.signal",status,status,1);if(!status)fatal();
    status=WaitForSingleObject(timer,5000);check("watchdog.join",status==WAIT_OBJECT_0,status,WAIT_OBJECT_0);if(status!=WAIT_OBJECT_0)fatal();
    if(!GetExitCodeThread(timer,&code))fatal();
    check("watchdog.exit",code==0,code,0);
    status=CloseHandle(timer)!=FALSE;check("watchdog.close",status,status,1);if(!status)fatal();
    status=CloseHandle(stop)!=FALSE;check("watchdog.stop_close",status,status,1);if(!status)fatal();
    printf("{\"type\":\"complete\",\"mode\":\"correctness\",\"ok\":%s,\"checks\":%u,\"failures\":%u,\"output_error\":%s}\n",failures?"false":"true",checks,failures,ferror(stdout)?"true":"false");
    return failures||fflush(stdout)||ferror(stdout)?1:0;
}
