/* Only copied into an empty, project-owned test prefix; never uses game files. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

static int check_child(const WCHAR *directory, const WCHAR *image, const char *label, BOOL deny)
{
    WCHAR path[MAX_PATH], command[MAX_PATH + 32];
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION process = {0};
    DWORD error, waited = WAIT_FAILED, code = STILL_ACTIVE;
    BOOL started, ok;
    if (swprintf(path, MAX_PATH, L"%ls\\%ls", directory, image) < 0 ||
        swprintf(command, MAX_PATH + 32, L"\"%ls\" --child", path) < 0) return 1;
    startup.cb = sizeof(startup);
    SetLastError(ERROR_SUCCESS);
    started = CreateProcessW(path, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                             NULL, NULL, &startup, &process);
    error = GetLastError();
    if (started)
    {
        waited = WaitForSingleObject(process.hProcess, 10000);
        if (waited == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &code);
        else
        {
            TerminateProcess(process.hProcess, 99);
            WaitForSingleObject(process.hProcess, 5000);
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    ok = deny ? !started && error == ERROR_ACCESS_DISABLED_BY_POLICY
              : started && waited == WAIT_OBJECT_0 && code == 73;
    printf("{\"case\":\"%s\",\"deniedExpected\":%s,\"started\":%s,"
           "\"error\":%lu,\"childExit\":%lu,\"ok\":%s}\n", label,
           deny ? "true" : "false", started ? "true" : "false",
           (unsigned long)error, (unsigned long)code, ok ? "true" : "false");
    return ok ? 0 : 1;
}

int wmain(int argc, WCHAR **argv)
{
    int failures = 0;
    if (argc == 2 && !wcscmp(argv[1], L"--child")) return 73;
    if (argc != 2 || (wcscmp(argv[1], L"C:\\CiderPreviewPolicy\\pe32") &&
                      wcscmp(argv[1], L"C:\\CiderPreviewPolicy\\pe64"))) return 64;
    /* Run the positive control first: a blanket child-launch failure must not pass. */
    failures += check_child(argv[1], L"allowed.exe", "allowed", FALSE);
    failures += check_child(argv[1], L"YuAnShEn.ExE", "yuanshen", TRUE);
    failures += check_child(argv[1], L"GenshinImpact.exe", "genshinimpact", TRUE);
    failures += check_child(argv[1], L"StarRail.exe", "starrail", TRUE);
    failures += check_child(argv[1], L"ZenlessZoneZero.exe", "zenlesszonezero", TRUE);
    if (fflush(stdout) || ferror(stdout)) return 2;
    return failures ? 1 : 0;
}
