/* General DirectShow lifecycle check. RenderFile may select another decoder;
 * this alone does not reproduce the KiriKiri WMA DMO drain loop or prove
 * decoded frame correctness. Use wma-dmo-empty-output for that status bug.
 * Build with i686-w64-mingw32-gcc -municode -O2 -o dshow-stop-cycle.exe
 *   dshow-stop-cycle.c -lstrmiids -lole32 -loleaut32 -luuid
 * A watchdog bounds each operation, including Stop and final COM release.
 */
#define COBJMACROS
#include <windows.h>
#include <dshow.h>
#include <stdio.h>
#include <stdlib.h>

static volatile LONG phase, phase_started, done;

static void begin_phase(LONG value)
{
    InterlockedExchange(&phase_started, (LONG)GetTickCount());
    InterlockedExchange(&phase, value);
}

static DWORD WINAPI watchdog(void *unused)
{
    DWORD started = GetTickCount();
    (void)unused;
    while (!InterlockedCompareExchange(&done, 0, 0))
    {
        DWORD elapsed = GetTickCount() - (DWORD)InterlockedCompareExchange(&phase_started, 0, 0);
        if (elapsed > 10000 || GetTickCount() - started > 60000)
        {
            fprintf(stderr, "TIMEOUT phase=%ld elapsed_ms=%lu\n",
                    InterlockedCompareExchange(&phase, 0, 0), elapsed);
            fflush(stderr);
            TerminateProcess(GetCurrentProcess(), 124);
            return 124;
        }
        Sleep(20);
    }
    return 0;
}

int wmain(int argc, WCHAR **argv)
{
    IGraphBuilder *graph = NULL;
    IMediaControl *control = NULL;
    IMediaSeeking *seeking = NULL;
    HRESULT hr;
    int result = 1;
    unsigned int count = 32;
    HANDLE timer;

    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "usage: dshow-stop-cycle <WMV/WMA file> [cycles 1..100]\n");
        return 2;
    }
    if (argc == 3)
    {
        WCHAR *end;
        unsigned long requested = wcstoul(argv[2], &end, 10);
        if (!*argv[2] || *end || requested < 1 || requested > 100) return 2;
        count = (unsigned int)requested;
    }
    begin_phase(1);
    timer = CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    if (!timer) return 3;
    hr = CoInitialize(NULL);
    if (FAILED(hr)) goto finish;
    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
            &IID_IGraphBuilder, (void **)&graph);
    if (FAILED(hr)) goto cleanup;
    hr = IGraphBuilder_QueryInterface(graph, &IID_IMediaControl, (void **)&control);
    if (FAILED(hr)) goto cleanup;

    begin_phase(2);
    hr = IGraphBuilder_RenderFile(graph, argv[1], NULL);
    printf("RenderFile=%#lx\n", hr);
    fflush(stdout);
    if (hr != S_OK) goto cleanup;
    hr = IGraphBuilder_QueryInterface(graph, &IID_IMediaSeeking, (void **)&seeking);
    if (FAILED(hr)) goto cleanup;
    for (unsigned int i = 0; i < count; ++i)
    {
        LONGLONG position = 0;
        OAFilterState state;
        begin_phase(3);
        hr = IMediaSeeking_SetPositions(seeking, &position, AM_SEEKING_AbsolutePositioning,
                NULL, AM_SEEKING_NoPositioning);
        if (FAILED(hr)) goto cleanup;
        hr = IMediaControl_Run(control);
        if (FAILED(hr)) goto cleanup;
        hr = IMediaControl_GetState(control, 1000, &state);
        if (hr != S_OK || state != State_Running) { result = 1; goto cleanup; }
        /* Allow the decoder to produce output before stopping mid-stream. */
        Sleep(150);
        if (i & 1)
        {
            hr = IMediaControl_Pause(control);
            if (FAILED(hr)) goto cleanup;
            hr = IMediaControl_GetState(control, 1000, &state);
            if (hr != S_OK || state != State_Paused) { result = 1; goto cleanup; }
        }
        begin_phase(4);
        DWORD started = GetTickCount();
        hr = IMediaControl_Stop(control);
        printf("cycle=%u Stop=%#lx stop_ms=%lu\n", i + 1, hr, GetTickCount() - started);
        fflush(stdout);
        if (FAILED(hr)) goto cleanup;
    }
    result = 0;
cleanup:
    begin_phase(5);
    if (control) IMediaControl_Release(control);
    if (seeking) IMediaSeeking_Release(seeking);
    if (graph) IGraphBuilder_Release(graph);
    CoUninitialize();
finish:
    if (result) fprintf(stderr, "failed hr=%#lx\n", hr);
    InterlockedExchange(&done, 1);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
    return result;
}
