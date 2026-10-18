#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static void json_string(const wchar_t *value)
{
    int count = WideCharToMultiByte(CP_UTF8, 0, value, -1, NULL, 0, NULL, NULL);
    char *bytes = count > 0 ? malloc((size_t)count) : NULL;
    if (!bytes) ExitProcess(111);
    WideCharToMultiByte(CP_UTF8, 0, value, -1, bytes, count, NULL, NULL);
    putchar('"');
    for (const unsigned char *p = (unsigned char *)bytes; *p; ++p) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p < 32) printf("\\u%04x", (unsigned int)*p);
        else putchar(*p);
    }
    putchar('"'); free(bytes);
}

int wmain(int argc, wchar_t **argv)
{
    wchar_t cwd[32768], environment[256];
    DWORD code;
    if (argc < 2) return 64;
    code = (DWORD)wcstoul(argv[1], NULL, 0);
    if (!GetCurrentDirectoryW(32768, cwd)) return 65;
    if (!GetEnvironmentVariableW(L"EXIT_CAPTURE_VALUE", environment, 256)) environment[0] = 0;
    printf("{\"kind\":\"owned-exit-probe\",\"requestedExitCode\":%lu,\"cwd\":", (unsigned long)code);
    json_string(cwd); printf(",\"environment\":"); json_string(environment);
    printf(",\"arguments\":[");
    for (int i = 1; i < argc; ++i) { if (i > 1) putchar(','); json_string(argv[i]); }
    printf("]}\n"); fflush(stdout);
    if (argc > 3 && !wcscmp(argv[2], L"--touch")) {
        HANDLE marker = CreateFileW(argv[3], GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (marker == INVALID_HANDLE_VALUE) return 66;
        CloseHandle(marker);
    }
    if (argc > 2 && !wcscmp(argv[2], L"--sleep")) Sleep(30000);
    ExitProcess(code);
}
