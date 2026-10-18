#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#ifndef OWNED_EXIT_CODE
#define OWNED_EXIT_CODE 0
#endif
#ifndef OWNED_SLEEP_MS
#define OWNED_SLEEP_MS 0
#endif
int main(void)
{
    puts("owned-manual-installer-started");
    fflush(stdout);
    Sleep(OWNED_SLEEP_MS);
    ExitProcess((DWORD)OWNED_EXIT_CODE);
}
