#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
static unsigned paints,ticks,pixel_ok;
static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
 if(message==WM_PAINT) {
  PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);RECT area;GetClientRect(window,&area);
  HBRUSH brush=CreateSolidBrush(RGB(32,144,96));FillRect(dc,&area,brush);DeleteObject(brush);
  if(GetPixel(dc,40,40)==RGB(32,144,96))pixel_ok++;
  paints++;EndPaint(window,&paint);return 0;
 }
 if(message==WM_TIMER) {ticks++;InvalidateRect(window,NULL,FALSE);if(ticks>=40)DestroyWindow(window);return 0;}
 if(message==WM_DESTROY) {PostQuitMessage(0);return 0;}
 return DefWindowProcW(window,message,wp,lp);
}
static int candidate_winmm_loaded(void) {
 BYTE *base=(BYTE *)GetModuleHandleW(L"winmm.dll");
 if(!base)return 0;
 IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)base;
 IMAGE_NT_HEADERS *nt=(IMAGE_NT_HEADERS *)(base+dos->e_lfanew);
 IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
 const char marker[]="winmm-repair/src/dlls/winmm/waveform.c";
 DWORD image_size=nt->OptionalHeader.SizeOfImage;
 for(WORD n=0;n<nt->FileHeader.NumberOfSections;n++) {
  DWORD rva=sections[n].VirtualAddress,size=sections[n].Misc.VirtualSize;
  if(!(sections[n].Characteristics&IMAGE_SCN_MEM_READ) || (sections[n].Characteristics&IMAGE_SCN_MEM_DISCARDABLE) || rva>image_size || size>image_size-rva)continue;
  for(DWORD i=0;i+sizeof(marker)-1<=size;i++)if(!memcmp(base+rva+i,marker,sizeof(marker)-1))return 1;
 }
 return 0;
}
int main(void) {
 printf("{\"kind\":\"winmm-module\",\"candidateModuleLoaded\":%s}\n",candidate_winmm_loaded()?"true":"false");fflush(stdout);
 WNDCLASSW cls={0};cls.lpfnWndProc=window_proc;cls.hInstance=GetModuleHandleW(NULL);cls.lpszClassName=L"CiderOwnedRuntimeWindow";
 if(!RegisterClassW(&cls))return 10;
 HWND window=CreateWindowExW(0,cls.lpszClassName,L"Cider runtime acceptance — private fixture",WS_OVERLAPPEDWINDOW|WS_VISIBLE,160,160,640,360,NULL,NULL,cls.hInstance,NULL);
 if(!window)return 11;
 printf("{\"kind\":\"window-ready\",\"windowsPID\":%lu}\n",GetCurrentProcessId());fflush(stdout);
 SetTimer(window,1,250,NULL);MSG msg;
 while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 printf("{\"kind\":\"window-result\",\"paintCount\":%u,\"timerCount\":%u,\"surfacePixelMatches\":%u}\n",paints,ticks,pixel_ok);fflush(stdout);
 WAVEFORMATEX format={0};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=44100;format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=176400;
 HWAVEOUT device=NULL;MMRESULT opened=waveOutOpen(&device,WAVE_MAPPER,&format,0,0,CALLBACK_NULL);
 if(opened!=MMSYSERR_NOERROR){printf("{\"kind\":\"audio-result\",\"openCode\":%u}\n",opened);return 20;}
 short silent[4410*2]={0};WAVEHDR header={0};header.lpData=(LPSTR)silent;header.dwBufferLength=sizeof(silent);
 MMRESULT prepared=waveOutPrepareHeader(device,&header,sizeof(header));MMRESULT written=prepared?prepared:waveOutWrite(device,&header,sizeof(header));
 ULONGLONG deadline=GetTickCount64()+5000;
 while(!written && !(header.dwFlags&WHDR_DONE) && GetTickCount64()<deadline)Sleep(10);
 int done=!!(header.dwFlags&WHDR_DONE);waveOutReset(device);
 MMRESULT unprepared=prepared?prepared:waveOutUnprepareHeader(device,&header,sizeof(header));MMRESULT closed=waveOutClose(device);
 printf("{\"kind\":\"audio-result\",\"openCode\":%u,\"prepareCode\":%u,\"writeCode\":%u,\"bufferCompleted\":%s,\"unprepareCode\":%u,\"closeCode\":%u}\n",opened,prepared,written,done?"true":"false",unprepared,closed);fflush(stdout);
 return paints&&pixel_ok&&done&&!prepared&&!written&&!unprepared&&!closed?0:21;
}
