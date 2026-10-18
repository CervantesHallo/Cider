#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include "protocol.h"

static BOOL same_id(LUID a, LUID b) { return a.LowPart == b.LowPart && a.HighPart == b.HighPart; }
static DWORD own_id(LUID *id)
{
    HANDLE token;
    TOKEN_STATISTICS stats;
    DWORD bytes, error = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return GetLastError();
    if (!GetTokenInformation(token, TokenStatistics, &stats, sizeof(stats), &bytes)) error = GetLastError();
    if (!CloseHandle(token) && !error) error = GetLastError();
    if (!error) *id = stats.TokenId;
    return error;
}
static DWORD collect(DWORD code, TOKEN_RESULT *r)
{
    HANDLE device;
    DWORD bytes = 0, error = 0;
    device = CreateFileW(TOKEN_WIN32, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (device == INVALID_HANDLE_VALUE) return GetLastError();
    if (!DeviceIoControl(device, code, NULL, 0, r, sizeof(*r), &bytes, NULL)) error = GetLastError();
    if (!CloseHandle(device) && !error) error = GetLastError();
    if (!error && (bytes != sizeof(*r) || r->version != 2 || r->bytes != sizeof(*r))) error = ERROR_INVALID_DATA;
    return error;
}
int wmain(int argc, wchar_t **argv)
{
    SC_HANDLE manager = NULL, service = NULL;
    SERVICE_STATUS state = {0};
    TOKEN_RESULT result = {0}, released = {0};
    PROCESS_INFORMATION child = {0};
    STARTUPINFOW startup = {0};
    wchar_t command[65536], path[32768];
    LUID id = {0};
    DWORD error = 0, cleanup_error = 0, child_code = 0;
    ULONG checks = 0, failures = 0;
    BOOL child_exited = FALSE, child_handles_closed = FALSE;
    if (argc != 2 || !GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version")) return 125;
    error = own_id(&id);
    if (error) goto done;
    if (!wcscmp(argv[1], L"--child")) {
        error = collect(TOKEN_HOLD, &result);
        return error || result.failures || !same_id(result.target_id, id);
    }
    manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager) { error = GetLastError(); goto done; }
    service = CreateServiceW(manager, TOKEN_SERVICE, TOKEN_SERVICE,
        SERVICE_START | SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS,
        SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, argv[1], NULL, NULL, NULL, NULL, NULL);
    if (!service) { error = GetLastError(); goto done; }
    if (!StartServiceW(service, 0, NULL)) { error = GetLastError(); goto done; }
    error = collect(TOKEN_CHECK, &result);
    if (error) goto done;
    checks += result.checks + 4; failures += result.failures;
    if (!same_id(result.target_id, id)) ++failures;
    if (result.rounds != 64) ++failures;
    if (result.concurrent_rounds != 64) ++failures;
    if (result.expected_faults != 2) ++failures;
    if (!GetModuleFileNameW(NULL, path, 32768)) { error = GetLastError(); goto done; }
    if (swprintf(command, 65536, L"\"%ls\" --child", path) < 0) { error = ERROR_INSUFFICIENT_BUFFER; goto done; }
    startup.cb = sizeof(startup);
    if (!CreateProcessW(path, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &child)) { error = GetLastError(); goto done; }
    if (WaitForSingleObject(child.hProcess, 15000) != WAIT_OBJECT_0) { error = ERROR_TIMEOUT; goto done; }
    child_exited = TRUE;
    if (!GetExitCodeProcess(child.hProcess, &child_code)) { error = GetLastError(); goto done; }
    ++checks; if (child_code) ++failures;
    if (!CloseHandle(child.hThread)) { error = GetLastError(); goto done; }
    child.hThread = NULL;
    if (!CloseHandle(child.hProcess)) { error = GetLastError(); goto done; }
    child.hProcess = NULL;
    child_handles_closed = TRUE;
    error = collect(TOKEN_RELEASE, &released);
    if (error) goto done;
    checks += released.checks + 1; failures += released.failures;
    if (same_id(released.held_id, id)) ++failures;
done:
    if (child.hProcess && !child_exited) {
        if (!TerminateProcess(child.hProcess, 124) || WaitForSingleObject(child.hProcess, 5000) != WAIT_OBJECT_0)
            cleanup_error = ERROR_TIMEOUT;
    }
    if (child.hThread && !CloseHandle(child.hThread) && !cleanup_error) cleanup_error = GetLastError();
    if (child.hProcess && !CloseHandle(child.hProcess) && !cleanup_error) cleanup_error = GetLastError();
    if (service) {
        if (!ControlService(service, SERVICE_CONTROL_STOP, &state) && GetLastError() != ERROR_SERVICE_NOT_ACTIVE && !cleanup_error)
            cleanup_error = GetLastError();
        {
            ULONGLONG until = GetTickCount64() + 10000;
            do {
                if (!QueryServiceStatus(service, &state)) { if (!cleanup_error) cleanup_error = GetLastError(); break; }
                if (state.dwCurrentState == SERVICE_STOPPED) break;
                Sleep(25);
            } while (GetTickCount64() < until);
            if (state.dwCurrentState != SERVICE_STOPPED && !cleanup_error) cleanup_error = ERROR_TIMEOUT;
        }
        if (!DeleteService(service) && !cleanup_error) cleanup_error = GetLastError();
        CloseServiceHandle(service);
    }
    if (manager) CloseServiceHandle(manager);
    printf("{\"schema\":\"cider.owned-token-runtime/v2\",\"win32Error\":%lu,\"cleanupError\":%lu,"
        "\"checks\":%lu,\"failures\":%lu,\"firstFailure\":%lu,\"firstStatus\":%ld,"
        "\"rounds\":%lu,\"concurrentRounds\":%lu,\"expectedFaults\":%lu,\"childExited\":%s,\"childExitCode\":%lu,\"childHandlesClosedBeforeRelease\":%s}\n",
        error, cleanup_error, checks, failures, result.first_failure, result.first_status,
        result.rounds, result.concurrent_rounds, result.expected_faults, child_exited ? "true" : "false", child_code, child_handles_closed ? "true" : "false");
    return error || cleanup_error || failures || !checks || !child_exited;
}
