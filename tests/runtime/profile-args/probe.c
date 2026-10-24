/* Owned, bounded child-process probe for the original CommandLineAppend rule.
 * Writes one newly-created AppDefaults key in an empty lab prefix, removes it,
 * and joins each owned child before continuing. If reaping is unprovable it
 * terminates itself; run ONLY through run-mac.py --suite profile-args, which
 * requires GUI shutdown and owned-prefix draining even on an ordinary failure.
 * Not a native Windows oracle. */
#define _WIN32_WINNT 0x0602
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

static unsigned checks, failures;
static void check(const char *name, int ok, unsigned long actual, unsigned long expected)
{
    ++checks; failures += !ok;
    printf("{\"type\":\"check\",\"name\":\"%s\",\"ok\":%s,\"actual\":%lu,\"expected\":%lu}\n",name,ok?"true":"false",actual,expected);
    if(fflush(stdout)||ferror(stdout)){TerminateProcess(GetCurrentProcess(),120);ExitProcess(120);}
}
static void fatal(void){TerminateProcess(GetCurrentProcess(),121);ExitProcess(121);}
static int child(int argc, wchar_t **argv)
{
    int appended, limit;
    if(argc<5||wcscmp(argv[3],L"space arg")||argv[4][0])return 40;
    appended=!wcscmp(argv[2],L"append")||!wcscmp(argv[2],L"unterminated")||!wcscmp(argv[2],L"implicit");
    limit=!wcscmp(argv[2],L"limit")||!wcscmp(argv[2],L"limit-nul");
    if(appended)
    {
        if(argc!=8||wcscmp(argv[5],L"--added")||wcscmp(argv[6],L"two words")||wcscmp(argv[7],L"中文参数"))return 41;
    }
    else if(limit)
    {
        size_t i,length=!wcscmp(argv[2],L"limit")?1024:1023;
        if(argc!=6||wcslen(argv[5])!=length)return 41;
        for(i=0;i<length;++i)if(argv[5][i]!=L'A')return 41;
    }
    else if(argc!=5)return 41;
    return 42;
}
static DWORD run_child(const WCHAR *exe, const WCHAR *scenario, int implicit)
{
    WCHAR command[2*MAX_PATH];STARTUPINFOW start={0};PROCESS_INFORMATION info={0};DWORD code=999,status;
    int n=swprintf(command,sizeof(command)/sizeof(command[0]),L"\"%s\" --child %s \"space arg\" \"\"",exe,scenario);
    if(n<0||(unsigned)n>=sizeof(command)/sizeof(command[0]))fatal();
    start.cb=sizeof(start);
    if(!CreateProcessW(implicit?NULL:exe,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&start,&info))return 1000;
    status=WaitForSingleObject(info.hProcess,10000);
    if(status!=WAIT_OBJECT_0)
    {
        if(!TerminateProcess(info.hProcess,122)||WaitForSingleObject(info.hProcess,5000)!=WAIT_OBJECT_0)fatal();
        code=1001;
    }
    else if(!GetExitCodeProcess(info.hProcess,&code))code=1002;
    if(!CloseHandle(info.hThread)||!CloseHandle(info.hProcess))fatal();
    return code;
}
int wmain(int argc,wchar_t **argv)
{
    WCHAR exe[MAX_PATH],key[MAX_PATH+40],*base,large[1026];HKEY reg=NULL;
    DWORD length,disposition,code,dword=65;LSTATUS status;unsigned i;
    const WCHAR value[]=L"--added \"two words\" 中文参数";
    struct example{const WCHAR *scenario;const char *name;DWORD type;const BYTE *data;DWORD size;int implicit;} cases[]={
        {L"missing","missing",REG_NONE,NULL,0,0},
        {L"append","append",REG_SZ,(const BYTE*)value,sizeof(value),0},
        {L"unterminated","unterminated",REG_SZ,(const BYTE*)value,sizeof(value)-sizeof(WCHAR),0},
        {L"implicit","implicit",REG_SZ,(const BYTE*)value,sizeof(value),1},
        {L"odd","odd",REG_SZ,(const BYTE*)L"AB",3,0},
        {L"type","type",REG_DWORD,(const BYTE*)&dword,sizeof(dword),0},
        {L"empty","empty",REG_SZ,(const BYTE*)L"",sizeof(WCHAR),0},
        {L"oversized","oversized",REG_SZ,(const BYTE*)large,sizeof(large),0},
        {L"limit","limit",REG_SZ,(const BYTE*)large,1024*sizeof(WCHAR),0},
        {L"limit-nul","limit-nul",REG_SZ,(const BYTE*)large,1024*sizeof(WCHAR),0}
    };
    if(argc>1&&!wcscmp(argv[1],L"--child"))return child(argc,argv);
    printf("{\"type\":\"begin\",\"mode\":\"correctness\",\"pointer_bits\":%u,\"pid\":%lu,\"subject\":\"profile-arguments\"}\n",(unsigned)(8*sizeof(void*)),GetCurrentProcessId());
    if(fflush(stdout)||ferror(stdout))return 2;
    length=GetModuleFileNameW(NULL,exe,MAX_PATH);if(!length||length>=MAX_PATH)fatal();
    base=wcsrchr(exe,L'\\');base=base?base+1:exe;
    if(swprintf(key,sizeof(key)/sizeof(key[0]),L"Software\\Wine\\AppDefaults\\%s",base)<0)fatal();
    status=RegCreateKeyExW(HKEY_CURRENT_USER,key,0,NULL,0,KEY_QUERY_VALUE|KEY_SET_VALUE,NULL,&reg,&disposition);
    if(status||disposition!=REG_CREATED_NEW_KEY){if(!status)RegCloseKey(reg);check("new_owned_key",0,status,0);return 1;}
    check("new_owned_key",1,disposition,REG_CREATED_NEW_KEY);
    for(i=0;i<sizeof(large)/sizeof(large[0]);++i)large[i]=L'A';
    large[1025]=0;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
    {
        if(!wcscmp(cases[i].scenario,L"limit-nul"))large[1023]=0;
        if(cases[i].data)status=RegSetValueExW(reg,L"CommandLineAppend",0,cases[i].type,cases[i].data,cases[i].size);
        else{status=RegDeleteValueW(reg,L"CommandLineAppend");if(status==ERROR_FILE_NOT_FOUND)status=ERROR_SUCCESS;}
        check("registry_write",status==ERROR_SUCCESS,status,ERROR_SUCCESS);if(status)break;
        code=run_child(exe,cases[i].scenario,cases[i].implicit);check(cases[i].name,code==42,code,42);
    }
    status=RegCloseKey(reg);check("registry_close",status==ERROR_SUCCESS,status,ERROR_SUCCESS);
    status=RegDeleteKeyW(HKEY_CURRENT_USER,key);check("registry_delete",status==ERROR_SUCCESS,status,ERROR_SUCCESS);
    printf("{\"type\":\"complete\",\"mode\":\"correctness\",\"ok\":%s,\"checks\":%u,\"failures\":%u,\"output_error\":%s}\n",failures?"false":"true",checks,failures,ferror(stdout)?"true":"false");
    return failures||fflush(stdout)||ferror(stdout)?1:0;
}
