/* Benign negative child-launch fixture: never point this at an installed game. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
int main(int argc,char **argv)
{
    WCHAR path[MAX_PATH],command[MAX_PATH+64];
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    DWORD length,error;
    BOOL started;
    int ok;
    if(argc==2 && !strcmp(argv[1],"--unexpected-child")) {
        puts("{\"type\":\"unexpected-policy-child\"}");
        return 98;
    }
    length=GetEnvironmentVariableW(L"WAITALL_DENY_PROBE",path,MAX_PATH);
    if(!length || length>=MAX_PATH || wcscmp(path,L"C:\\CiderTests\\yuanshen.exe")) return 64;
    if(swprintf(command,MAX_PATH+64,L"\"%ls\" --unexpected-child",path)<0) return 65;
    memset(&startup,0,sizeof(startup));startup.cb=sizeof(startup);
    memset(&process,0,sizeof(process));
    SetLastError(ERROR_SUCCESS);
    started=CreateProcessW(path,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process);
    error=GetLastError();
    if(started) {
        TerminateProcess(process.hProcess,99);
        WaitForSingleObject(process.hProcess,5000);
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
    }
    ok=!started && error==ERROR_ACCESS_DISABLED_BY_POLICY;
    printf("{\"type\":\"child-policy-negative\",\"started\":%s,\"error\":%lu,\"expected\":1260,\"ok\":%s}\n",started?"true":"false",(unsigned long)error,ok?"true":"false");
    return ok && fflush(stdout)==0 && !ferror(stdout) ? 0:1;
}
