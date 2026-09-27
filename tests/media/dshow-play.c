/* Plays a media file through DirectShow the way KiriKiri's krmovie does (IGraphBuilder::RenderFile, Run,
 * wait for EC_COMPLETE). Exit code: 0 played to the end, 2 could not build the graph, 3 did not finish.
 * Build: i686-w64-mingw32-gcc -O2 -o dshow-play.exe dshow-play.c -lstrmiids -lole32 -loleaut32 -luuid */
#define COBJMACROS
#include <windows.h>
#include <dshow.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    IGraphBuilder *graph = NULL;
    IMediaControl *control = NULL;
    IMediaEvent *event = NULL;
    long code = 0;
    HRESULT hr;

    if (argc < 2) { fprintf(stderr, "usage: dshow-play <file>\n"); return 1; }
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
    if (FAILED(hr)) { printf("FilterGraph: %#lx\n", hr); return 2; }
    hr = IGraphBuilder_RenderFile(graph, argv[1], NULL);
    printf("RenderFile: %#lx\n", hr);
    if (FAILED(hr)) return 2;
    IGraphBuilder_QueryInterface(graph, &IID_IMediaControl, (void **)&control);
    IGraphBuilder_QueryInterface(graph, &IID_IMediaEvent, (void **)&event);
    hr = IMediaControl_Run(control);
    printf("Run: %#lx\n", hr);
    hr = IMediaEvent_WaitForCompletion(event, 20000, &code);
    printf("WaitForCompletion: %#lx, event %ld\n", hr, code);
    IMediaControl_Stop(control);
    return (SUCCEEDED(hr) && code == EC_COMPLETE) ? 0 : 3;
}
