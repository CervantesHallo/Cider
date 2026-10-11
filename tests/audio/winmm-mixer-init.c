/* Mixer metadata must not eagerly probe every waveform format.
 * No microphone stream is opened and no audio buffers are captured.
 * Build: x86_64-w64-mingw32-gcc -O2 -o winmm-mixer-init.exe
 *   winmm-mixer-init.c -lwinmm
 * A local watchdog bounds hung initialization, including concurrent callers.
 */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

static HANDLE ready;
static volatile LONG finished;
static DWORD elapsed[4];
static MMRESULT results[4];
static UINT counts[4];

static DWORD WINAPI watchdog(void *unused)
{
    DWORD start = GetTickCount();
    (void)unused;
    while (!InterlockedCompareExchange(&finished, 0, 0))
    {
        if (GetTickCount() - start > 20000)
        {
            fprintf(stderr, "TIMEOUT mixer metadata initialization\n");
            fflush(stderr);
            TerminateProcess(GetCurrentProcess(), 124);
            return 124;
        }
        Sleep(20);
    }
    return 0;
}

static DWORD WINAPI worker(void *argument)
{
    unsigned int i = (unsigned int)(ULONG_PTR)argument;
    HMIXER mixer = NULL;
    MIXERCAPSW caps = {0};
    WaitForSingleObject(ready, INFINITE);
    DWORD start = GetTickCount();
    results[i] = mixerOpen(&mixer, 0, 0, 0, MIXER_OBJECTF_MIXER);
    elapsed[i] = GetTickCount() - start;
    if (results[i] == MMSYSERR_NOERROR)
    {
        counts[i] = mixerGetNumDevs();
        results[i] = mixerGetDevCapsW((UINT_PTR)mixer, &caps, sizeof(caps));
        MMRESULT closed = mixerClose(mixer);
        if (results[i] == MMSYSERR_NOERROR) results[i] = closed;
    }
    return 0;
}

int main(int argc, char **argv)
{
    HANDLE threads[4] = {0}, timer;
    int failed = 0;
    unsigned int caller_count = 4;
    if (argc == 2 && !strcmp(argv[1], "--serial")) caller_count = 1;
    else if (argc != 1) return 2;
    ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!ready) return 2;
    timer = CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    if (!timer) return 2;
    for (unsigned int i = 0; i < caller_count; ++i)
    {
        threads[i] = CreateThread(NULL, 0, worker, (void *)(ULONG_PTR)i, 0, NULL);
        if (!threads[i]) return 2;
    }
    SetEvent(ready);
    WaitForMultipleObjects(caller_count, threads, TRUE, INFINITE);
    for (unsigned int i = 0; i < caller_count; ++i)
    {
        printf("caller=%u mixer_result=%u init_ms=%lu count=%u\n",
                i, results[i], elapsed[i], counts[i]);
        if (results[i] != MMSYSERR_NOERROR || !counts[i] || counts[i] != counts[0]) failed = 1;
        CloseHandle(threads[i]);
    }
    HMIXER invalid = NULL;
    MMRESULT error = mixerOpen(&invalid, 0x7fffffff, 0, 0, MIXER_OBJECTF_MIXER);
    printf("invalid_mixer_result=%u\n", error);
    if (error != MMSYSERR_BADDEVICEID || invalid) failed = 1;
    WAVEOUTCAPSW output;
    error = waveOutGetDevCapsW(WAVE_MAPPER, &output, sizeof(output));
    printf("mapper_result=%u mapper_formats=%#lx\n", error, error ? 0 : output.dwFormats);
    if (error != MMSYSERR_NOERROR || output.dwFormats != 0xffffffff) failed = 1;
    /* A partial capture capability request does not include dwFormats. */
    UINT inputs = waveInGetNumDevs();
    for (UINT i = 0; i < inputs; ++i)
    {
        WAVEINCAPSW input = {0};
        error = waveInGetDevCapsW(i, &input, offsetof(WAVEINCAPSW, dwFormats));
        printf("capture_metadata=%u result=%u\n", i, error);
        if (error != MMSYSERR_NOERROR || !input.szPname[0]) failed = 1;
    }
    /* Genuine output capabilities and the open-HWAVE branch, without writing
     * audio buffers. No capture handle is created. */
    error = waveOutGetDevCapsW(0, &output, sizeof(output));
    printf("output_caps_result=%u formats=%#lx\n", error, error ? 0 : output.dwFormats);
    if (error != MMSYSERR_NOERROR || !output.dwFormats) failed = 1;
    WAVEFORMATEX pcm = {WAVE_FORMAT_PCM, 2, 48000, 192000, 4, 16, 0};
    HWAVEOUT handle = NULL;
    error = waveOutOpen(&handle, 0, &pcm, 0, 0, CALLBACK_NULL);
    printf("output_open_result=%u\n", error);
    if (error == MMSYSERR_NOERROR)
    {
        WAVEOUTCAPSW from_handle = {0};
        error = waveOutGetDevCapsW((UINT_PTR)handle, &from_handle, sizeof(from_handle));
        printf("output_handle_caps_result=%u formats=%#lx\n", error, error ? 0 : from_handle.dwFormats);
        if (error || from_handle.dwFormats != output.dwFormats) failed = 1;
        if (waveOutClose(handle) != MMSYSERR_NOERROR) failed = 1;
    }
    else failed = 1;
    fflush(stdout);
    InterlockedExchange(&finished, 1);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
    CloseHandle(ready);
    return failed;
}
