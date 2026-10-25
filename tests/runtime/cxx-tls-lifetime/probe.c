#define _WIN32_WINNT 0x0602
#include <windows.h>
#include <stdio.h>

typedef int (__cdecl *touch_fn)(void);
static touch_fn touch;
static LONG faults;
static LONG fault_reporting;

static LONG CALLBACK observe(EXCEPTION_POINTERS *p)
{
    if (p->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
    {
        if (InterlockedExchange(&fault_reporting, 1))
        {
            TerminateProcess(GetCurrentProcess(), 118);
            return EXCEPTION_CONTINUE_SEARCH;
        }
        InterlockedIncrement(&faults);
        printf("owned-cxx-tls-fault code=%08lx address=%p thread=%lu\n",
                p->ExceptionRecord->ExceptionCode, p->ExceptionRecord->ExceptionAddress,
                GetCurrentThreadId());
        fflush(stdout);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI worker(void *unused)
{
    (void)unused;
    return touch() == 0 ? 0 : 1;
}

static DWORD WINAPI watchdog(void *unused)
{
    (void)unused;
    Sleep(12000);
    /* Termination must not wait behind CRT logging held by a faulting thread. */
    TerminateProcess(GetCurrentProcess(), 124);
    return 0;
}

int main(void)
{
    HMODULE library;
    HANDLE thread, timer;
    PVOID handler;
    DWORD wait, code;
    BOOL unloaded;

    timer = CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    if (!timer) return 121;
    CloseHandle(timer);
    handler = AddVectoredExceptionHandler(1, observe);
    if (!handler) return 122;
    library = LoadLibraryA("owned-cxx-tls.dll");
    if (!library) return 123;
    touch = (touch_fn)(void *)GetProcAddress(library, "touch_tls");
    if (!touch || touch() != 0) return 125;
    thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (!thread) return 126;
    wait = WaitForSingleObject(thread, 2000);
    if (wait != WAIT_OBJECT_0 || !GetExitCodeThread(thread, &code) || code) return 127;
    CloseHandle(thread);
    printf("owned-cxx-tls-before-unload faults=%ld\n", faults);
    fflush(stdout);
    unloaded = FreeLibrary(library);
    printf("owned-cxx-tls-complete unloaded=%u faults=%ld\n", unloaded, faults);
    fflush(stdout);
    RemoveVectoredExceptionHandler(handler);
    return unloaded && !faults ? 0 : 1;
}
