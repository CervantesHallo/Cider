/* Synthetic processes only: no account, network, service, VM, or driver access. */
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

int wmain(int argc, wchar_t **argv)
{
    int i;
    if (argc < 2) return 2;
    if (wcscmp(argv[1], L"echo") == 0) {
        for (i = 2; i < argc; ++i) {
            size_t j;
            printf("%zu:", wcslen(argv[i]));
            for (j = 0; argv[i][j]; ++j) printf("%04x", (unsigned int)argv[i][j]);
            putchar('\n');
        }
        return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 3;
    }
    if (wcscmp(argv[1], L"exit37") == 0) return 37;
    if (wcscmp(argv[1], L"sleep") == 0) { Sleep(20000); return 0; }
    if (wcscmp(argv[1], L"spawn") == 0 || wcscmp(argv[1], L"breakaway") == 0) {
        wchar_t image[1024], command[1100];
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        DWORD n = GetModuleFileNameW(NULL, image, 1024);
        DWORD flags = wcscmp(argv[1], L"breakaway") == 0 ? CREATE_BREAKAWAY_FROM_JOB : 0;
        if (!n || n >= 1024) return 3;
        if (swprintf(command, 1100, L"\"%ls\" sleep", image) < 0) return 3;
        ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
        if (!CreateProcessW(image, command, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi))
            return flags && GetLastError() == ERROR_ACCESS_DENIED ? 0 : 3;
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        return flags ? 4 : 0;
    }
    if (wcscmp(argv[1], L"stop") == 0 && argc == 3) {
        HANDLE file = CreateFileW(argv[2], GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) return 3;
        CloseHandle(file); Sleep(20000); return 0;
    }
    return 2;
}
