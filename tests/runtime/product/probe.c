#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
static void json_string(const wchar_t *text) {
 int n=WideCharToMultiByte(CP_UTF8,0,text,-1,NULL,0,NULL,NULL);
 char *s=malloc(n>0?(size_t)n:1); if(!s) ExitProcess(70);
 if(n>0) WideCharToMultiByte(CP_UTF8,0,text,-1,s,n,NULL,NULL);else s[0]=0;
 putchar('"'); for(unsigned char *p=(unsigned char *)s;*p;p++) {
  if(*p=='"'||*p=='\\') {putchar('\\');putchar(*p);} else if(*p<32) printf("\\u%04x",*p);else putchar(*p);
 } putchar('"');free(s);
}
int wmain(int argc,wchar_t **argv) {
 wchar_t cwd[32768]={0},env[32768]={0};DWORD delay=0;int code=0;
 GetCurrentDirectoryW(32768,cwd);GetEnvironmentVariableW(L"ACCEPTANCE_VALUE",env,32768);
 printf("{\"kind\":\"cider-owned-win32-probe\",\"windowsPID\":%lu,\"cwd\":",GetCurrentProcessId());json_string(cwd);
 printf(",\"fixtureEnvironment\":");json_string(env);printf(",\"arguments\":[");
 for(int i=1;i<argc;i++){if(i>1)putchar(',');json_string(argv[i]);}
 printf("]}\n");fflush(stdout);
 for(int i=1;i+1<argc;i++){if(!wcscmp(argv[i],L"--hold"))delay=(DWORD)_wtoi(argv[i+1]);if(!wcscmp(argv[i],L"--exit"))code=_wtoi(argv[i+1]);}
 if(delay) { Sleep(delay); }
 return code;
}
