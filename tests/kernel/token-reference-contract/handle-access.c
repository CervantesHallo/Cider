/* Read-only, own-process public API contract observations. No driver, token
 * mutation, privilege adjustment, impersonation, foreign PID or private RPC. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

int main(void)
{
    HANDLE opened = NULL, queried = NULL, duplicated = NULL;
    TOKEN_STATISTICS statistics;
    DWORD bytes, open_error, query_error = ERROR_INVALID_HANDLE;
    DWORD duplicate_error = ERROR_INVALID_HANDLE, duplicate_query_error = ERROR_INVALID_HANDLE;
    DWORD baseline_error;
    BOOL opened_ok, query_ok = FALSE, baseline_ok, duplicate_ok = FALSE, duplicate_query_ok = FALSE;
    DWORD cleanup_error = ERROR_SUCCESS;
    BOOL wine = GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version") != NULL;

    SetLastError(ERROR_SUCCESS);
    opened_ok = OpenProcessToken(GetCurrentProcess(), 0, &opened);
    open_error = GetLastError();
    if (opened_ok) {
        SetLastError(ERROR_SUCCESS);
        query_ok = GetTokenInformation(opened, TokenStatistics, &statistics, sizeof(statistics), &bytes);
        query_error = GetLastError();
    }
    SetLastError(ERROR_SUCCESS);
    baseline_ok = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &queried);
    baseline_error = GetLastError();
    if (baseline_ok) {
        SetLastError(ERROR_SUCCESS);
        duplicate_ok = DuplicateHandle(GetCurrentProcess(), queried, GetCurrentProcess(),
            &duplicated, 0, FALSE, 0);
        duplicate_error = GetLastError();
        if (duplicate_ok) {
            SetLastError(ERROR_SUCCESS);
            duplicate_query_ok = GetTokenInformation(duplicated, TokenStatistics,
                &statistics, sizeof(statistics), &bytes);
            duplicate_query_error = GetLastError();
        }
    }
    if (opened && !CloseHandle(opened)) cleanup_error = GetLastError();
    if (duplicated && !CloseHandle(duplicated)) cleanup_error = GetLastError();
    if (queried && !CloseHandle(queried)) cleanup_error = GetLastError();
    printf("{\"schema\":\"cider.own-token-zero-access-observation/v1\",\"wine\":%s,\"processBits\":%u,"
        "\"openZero\":{\"success\":%s,\"error\":%lu,\"querySuccess\":%s,\"queryError\":%lu},"
        "\"openQuery\":{\"success\":%s,\"error\":%lu},"
        "\"duplicateZero\":{\"success\":%s,\"error\":%lu,\"querySuccess\":%s,\"queryError\":%lu},"
        "\"cleanupError\":%lu,\"mutatedToken\":false}\n",
        wine ? "true" : "false", (unsigned int)(sizeof(void *) * 8),
        opened_ok ? "true" : "false", open_error, query_ok ? "true" : "false", query_error,
        baseline_ok ? "true" : "false", baseline_error,
        duplicate_ok ? "true" : "false", duplicate_error, duplicate_query_ok ? "true" : "false", duplicate_query_error,
        cleanup_error);
    if (fflush(stdout) == EOF || ferror(stdout)) return 1;
    return cleanup_error || !baseline_ok;
}
