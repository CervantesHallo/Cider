#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include "protocol.h"

int wmain(int argc, wchar_t **argv)
{
    SC_HANDLE manager = NULL, service = NULL;
    HANDLE device = INVALID_HANDLE_VALUE;
    OWNED_RESULT result = {0};
    SERVICE_STATUS state;
    DWORD bytes = 0, error = 0, cleanup_error = 0;
    /* The fixture is intentionally not a native Windows driver loader. */
    if (argc != 2 || !GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version")) return 125;
    manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager) { error = GetLastError(); goto done; }
    service = CreateServiceW(manager, OWNED_SERVICE, OWNED_SERVICE, SERVICE_START | SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS,
        SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, argv[1], NULL, NULL, NULL, NULL, NULL);
    if (!service) { error = GetLastError(); goto done; }
    if (!StartServiceW(service, 0, NULL)) { error = GetLastError(); goto done; }
    device = CreateFileW(OWNED_WIN32, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (device == INVALID_HANDLE_VALUE) { error = GetLastError(); goto done; }
    if (!DeviceIoControl(device, OWNED_IOCTL, NULL, 0, &result, sizeof(result), &bytes, NULL)) error = GetLastError();
done:
    if (device != INVALID_HANDLE_VALUE) CloseHandle(device);
    if (service) {
        if (!ControlService(service, SERVICE_CONTROL_STOP, &state) && GetLastError() != ERROR_SERVICE_NOT_ACTIVE) cleanup_error = GetLastError();
        if (!cleanup_error) {
            ULONGLONG until = GetTickCount64() + 10000;
            do {
                if (!QueryServiceStatus(service, &state)) { cleanup_error = GetLastError(); break; }
                if (state.dwCurrentState == SERVICE_STOPPED) break;
                Sleep(25);
            } while (GetTickCount64() < until);
            if (!cleanup_error && state.dwCurrentState != SERVICE_STOPPED) cleanup_error = ERROR_TIMEOUT;
        }
        if (!DeleteService(service) && !cleanup_error) cleanup_error = GetLastError();
        CloseServiceHandle(service);
    }
    if (manager) CloseServiceHandle(manager);
    printf("{\"schema\":\"cider.wine-owned-object-runtime/v2\",\"win32Error\":%lu,\"serviceCleanupError\":%lu,"
        "\"responseBytes\":%lu,\"checks\":%lu,\"failures\":%lu,\"firstFailure\":%lu,\"firstStatus\":%ld,"
        "\"eventRounds\":%lu,\"callbacks\":%lu,\"unsupportedInfoStatus\":%ld,"
        "\"concurrentReferences\":%lu,\"concurrentClosedHandles\":%lu}\n",
        error, cleanup_error, bytes, result.checks, result.failures, result.first_failure, result.first_status,
        result.event_rounds, result.callbacks, result.unsupported_info_status,
        result.concurrent_references, result.concurrent_closed_handles);
    return error || cleanup_error || bytes != sizeof(result) || result.version != 2 || result.bytes != sizeof(result)
        || result.failures || result.event_rounds != 256 || result.callbacks != 64
        || !result.concurrent_references || !result.concurrent_closed_handles
        || result.concurrent_references + result.concurrent_closed_handles != 64;
}
