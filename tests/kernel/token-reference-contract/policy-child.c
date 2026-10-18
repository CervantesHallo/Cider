/* A self-copy with a denied basename exercises the engine's child policy.
 * This file contains no game code. The child must never be started. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
int wmain(int argc, wchar_t **argv)
{
    wchar_t source[32768], target[32768], marker[32768], command[65536], *slash;
    PROCESS_INFORMATION child = {0};
    STARTUPINFOW startup = {0};
    DWORD error = 0, cleanup_error = 0;
    BOOL started, marker_absent;
    if (!GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version")) return 125;
    if (!GetModuleFileNameW(NULL, source, 32768)) return 1;
    wcscpy(marker, source); slash = wcsrchr(marker, L'\\'); if (!slash) return 2;
    wcscpy(slash + 1, L"owned-child-ran.marker");
    if (argc == 2 && !wcscmp(argv[1], L"--policy-child")) {
        HANDLE file = CreateFileW(marker, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        return 99;
    }
    if (GetFileAttributesW(marker) != INVALID_FILE_ATTRIBUTES) return 3;
    wcscpy(target, source); slash = wcsrchr(target, L'\\');
    wcscpy(slash + 1, L"YuanShen.exe");
    if (!CopyFileW(source, target, TRUE)) return 4;
    if (swprintf(command, 65536, L"\"%ls\" --policy-child", target) < 0) return 5;
    startup.cb = sizeof(startup);
    started = CreateProcessW(target, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &child);
    if (!started) error = GetLastError();
    else {
        if (WaitForSingleObject(child.hProcess, 5000) != WAIT_OBJECT_0) {
            TerminateProcess(child.hProcess, 124);
            if (WaitForSingleObject(child.hProcess, 5000) != WAIT_OBJECT_0) cleanup_error = ERROR_TIMEOUT;
        }
        CloseHandle(child.hThread); CloseHandle(child.hProcess);
    }
    marker_absent = GetFileAttributesW(marker) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    if (!DeleteFileW(target) && !cleanup_error) cleanup_error = GetLastError();
    printf("{\"schema\":\"cider.owned-child-policy/v1\",\"childStarted\":%s,\"error\":%lu,"
        "\"markerAbsent\":%s,\"cleanupError\":%lu}\n", started ? "true" : "false", error,
        marker_absent ? "true" : "false", cleanup_error);
    return started || error != ERROR_ACCESS_DISABLED_BY_POLICY || !marker_absent || cleanup_error;
}
