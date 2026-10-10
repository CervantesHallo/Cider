/* Native observations of our own process/thread tokens only.
 * --user uses Win32 APIs. --kernel requires the independently prepared driver.
 * Neither mode installs/starts a driver or changes the original process token.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "../shared/protocol.h"
#include "../shared/source-version.h"

#ifndef _MSC_FULL_VER
#define CR_MSC_FULL_VER 0
#else
#define CR_MSC_FULL_VER _MSC_FULL_VER
#endif

static const char *privilege_names[CR_PRIVILEGE_COUNT] =
    {"SeChangeNotifyPrivilege", "SeBackupPrivilege", "SeRestorePrivilege"};
static LUID privilege_values[CR_PRIVILEGE_COUNT];
static BOOL context_restore_failed = FALSE;
_Static_assert(sizeof(CR_REQUEST) == 32, "request ABI");
_Static_assert(sizeof(CR_RESPONSE) == 300, "response ABI");

static void os_observation(void)
{
    union {
        FARPROC address;
        LONG (WINAPI *query)(OSVERSIONINFOW *);
    } function;
    OSVERSIONINFOW version = {0};
    LONG status = (LONG)0xc0000139u;
    function.address = GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    version.dwOSVersionInfoSize = sizeof(version);
    if (function.address) status = function.query(&version);
    printf(",\"os_version_query_status\":%ld,\"os_version\":", (long)status);
    if (status < 0) {
        printf("null");
    } else {
        printf("{\"major\":%lu,\"minor\":%lu,\"build\":%lu}",
               (unsigned long)version.dwMajorVersion, (unsigned long)version.dwMinorVersion,
               (unsigned long)version.dwBuildNumber);
    }
}

static int error_record(const char *stage, DWORD error)
{
    printf("{\"schema\":\"cider.windows-reference-error/v1\",\"stage\":\"%s\",\"win32_error\":%lu}\n",
           stage, (unsigned long)error);
    return 1;
}

static void number_array(const ULONG *values, unsigned count)
{
    unsigned i;
    putchar('[');
    for (i = 0; i < count; ++i) printf("%s%lu", i ? "," : "", (unsigned long)values[i]);
    putchar(']');
}

static BOOL luid_equal(LUID a, LUID b)
{
    return a.LowPart == b.LowPart && a.HighPart == b.HighPart;
}

static int select_token(HANDLE *token, BOOL *impersonating)
{
    if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, token)) {
        *impersonating = TRUE;
        return 0;
    }
    if (GetLastError() != ERROR_NO_TOKEN) return error_record("OpenThreadToken", GetLastError());
    *impersonating = FALSE;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, token))
        return error_record("OpenProcessToken", GetLastError());
    return 0;
}

static int token_snapshot(HANDLE token, BOOL impersonating, const char *label)
{
    DWORD size = 0, capacity, error;
    const DWORD privileges_offset = (DWORD)offsetof(TOKEN_PRIVILEGES, Privileges);
    TOKEN_PRIVILEGES *privileges;
    TOKEN_TYPE type;
    SECURITY_IMPERSONATION_LEVEL level = SecurityAnonymous;
    ULONG present[CR_PRIVILEGE_COUNT] = {0}, attributes[CR_PRIVILEGE_COUNT] = {0};
    unsigned i;
    DWORD j;
    if (!GetTokenInformation(token, TokenType, &type, sizeof(type), &size))
        return error_record("TokenType", GetLastError());
    if (impersonating && !GetTokenInformation(token, TokenImpersonationLevel, &level, sizeof(level), &size))
        return error_record("TokenImpersonationLevel", GetLastError());
    SetLastError(ERROR_SUCCESS);
    if (GetTokenInformation(token, TokenPrivileges, NULL, 0, &size) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        return error_record("TokenPrivileges.size", GetLastError());
    if (size < privileges_offset || size > 1024u * 1024u)
        return error_record("TokenPrivileges.size-validation", ERROR_INVALID_DATA);
    capacity = size;
    privileges = (TOKEN_PRIVILEGES *)malloc(size);
    if (!privileges) return error_record("TokenPrivileges.allocate", ERROR_NOT_ENOUGH_MEMORY);
    if (!GetTokenInformation(token, TokenPrivileges, privileges, size, &size)) {
        error = GetLastError();
        free(privileges);
        return error_record("TokenPrivileges", error);
    }
    if (size > capacity || size < privileges_offset ||
        privileges->PrivilegeCount > (size - privileges_offset) / sizeof(LUID_AND_ATTRIBUTES)) {
        free(privileges);
        return error_record("TokenPrivileges.count-validation", ERROR_INVALID_DATA);
    }
    for (i = 0; i < CR_PRIVILEGE_COUNT; ++i)
        for (j = 0; j < privileges->PrivilegeCount; ++j)
            if (luid_equal(privileges->Privileges[j].Luid, privilege_values[i])) {
                present[i] = 1;
                attributes[i] = privileges->Privileges[j].Attributes;
                break;
            }
    free(privileges);
    printf("{\"schema\":\"cider.windows-token-observation/v1\",\"case\":\"%s\","
           "\"thread_impersonating\":%s,\"token_type\":%u,\"impersonation_level\":%u,\"present\":",
           label, impersonating ? "true" : "false", (unsigned)type, (unsigned)level);
    number_array(present, CR_PRIVILEGE_COUNT);
    printf(",\"attributes\":");
    number_array(attributes, CR_PRIVILEGE_COUNT);
    printf("}\n");
    return 0;
}

static int user_checks(HANDLE token, const char *label, BOOL source_impersonating)
{
    unsigned row, i;
    const unsigned rows = CR_PRIVILEGE_COUNT + 2;
    size_t size = sizeof(PRIVILEGE_SET) + (CR_PRIVILEGE_COUNT - 1) * sizeof(LUID_AND_ATTRIBUTES);
    PRIVILEGE_SET *set = (PRIVILEGE_SET *)malloc(size);
    if (!set) return error_record("PrivilegeCheck.allocate", ERROR_NOT_ENOUGH_MEMORY);
    for (row = 0; row < rows; ++row) {
        BOOL result = FALSE, ok;
        DWORD error;
        ULONG before[CR_PRIVILEGE_COUNT] = {0}, after[CR_PRIVILEGE_COUNT] = {0};
        unsigned count = row < CR_PRIVILEGE_COUNT ? 1 : CR_PRIVILEGE_COUNT;
        memset(set, 0, size);
        set->PrivilegeCount = count;
        set->Control = row == CR_PRIVILEGE_COUNT + 1 ? PRIVILEGE_SET_ALL_NECESSARY : 0;
        for (i = 0; i < count; ++i)
            set->Privilege[i].Luid = privilege_values[count == 1 ? row : i];
        SetLastError(ERROR_SUCCESS);
        ok = PrivilegeCheck(token, set, &result);
        error = ok ? ERROR_SUCCESS : GetLastError();
        for (i = 0; i < count; ++i) after[i] = set->Privilege[i].Attributes;
        printf("{\"schema\":\"cider.windows-privilege-observation/v1\",\"case\":\"%s\","
               "\"api\":\"PrivilegeCheck\",\"token_origin\":\"%s\",\"row\":%u,\"count\":%u,\"control\":%lu,"
               "\"call_succeeded\":%s,\"result\":%s,\"win32_error\":%lu,\"attributes_before\":",
               label, source_impersonating ? "selected-thread-token" : "private-primary-identification-copy",
               row, count, (unsigned long)set->Control, ok ? "true" : "false",
               result ? "true" : "false", (unsigned long)error);
        number_array(before, count);
        printf(",\"attributes_after\":");
        number_array(after, count);
        printf("}\n");
        if (!ok) { free(set); return 1; }
    }
    free(set);
    return 0;
}

static int kernel_checks(HANDLE device, const char *label)
{
    CR_REQUEST request = {0};
    CR_RESPONSE response = {0};
    DWORD bytes = 0;
    unsigned i;
    request.version = CR_PROTOCOL_VERSION;
    request.size = sizeof(request);
    for (i = 0; i < CR_PRIVILEGE_COUNT; ++i) request.privileges[i] = privilege_values[i];
    if (!DeviceIoControl(device, CR_IOCTL_OBSERVE, &request, sizeof(request),
                         &response, sizeof(response), &bytes, NULL))
        return error_record("DeviceIoControl", GetLastError());
    if (bytes != sizeof(response) || response.version != CR_PROTOCOL_VERSION || response.size != sizeof(response))
        return error_record("DeviceIoControl.response", ERROR_INVALID_DATA);
    printf("{\"schema\":\"cider.windows-kernel-observation/v1\",\"case\":\"%s\","
           "\"token_query_status\":%ld,\"irql\":%lu,\"requestor_mode\":%lu,"
           "\"current_process_is_requestor\":%lu,\"current_thread_is_requestor\":%lu,\"captured_client_present\":%lu,"
           "\"captured_impersonation_level\":%lu,\"primary_reference_matches_capture\":%lu,"
           "\"client_reference_matches_capture\":%lu,\"client_reference_present\":%lu,"
           "\"client_reference_level\":%lu,\"client_copy_on_open\":%lu,\"client_effective_only\":%lu,"
           "\"lifecycle_calls\":{\"capture\":%lu,\"lock\":%lu,\"unlock\":%lu,\"release\":%lu,"
           "\"primary_reference\":%lu,\"primary_dereference\":%lu,\"client_reference\":%lu,"
           "\"client_dereference\":%lu},\"privileges_present\":",
           label, (long)response.token_query_status, (unsigned long)response.irql,
           (unsigned long)response.requestor_mode, (unsigned long)response.current_process_is_requestor,
           (unsigned long)response.current_thread_is_requestor,
           (unsigned long)response.captured_client_present, (unsigned long)response.captured_impersonation_level,
           (unsigned long)response.primary_reference_matches_capture, (unsigned long)response.client_reference_matches_capture,
           (unsigned long)response.client_reference_present, (unsigned long)response.client_reference_level,
           (unsigned long)response.client_copy_on_open, (unsigned long)response.client_effective_only,
           (unsigned long)response.context_capture_calls, (unsigned long)response.context_lock_calls,
           (unsigned long)response.context_unlock_calls, (unsigned long)response.context_release_calls,
           (unsigned long)response.primary_reference_calls, (unsigned long)response.primary_dereference_calls,
           (unsigned long)response.client_reference_calls, (unsigned long)response.client_dereference_calls);
    number_array(response.privileges_present, CR_PRIVILEGE_COUNT);
    printf(",\"privilege_attributes\":");
    number_array(response.privilege_attributes, CR_PRIVILEGE_COUNT);
    printf(",\"single_user\":");
    number_array(response.single_user, CR_PRIVILEGE_COUNT);
    printf(",\"single_kernel\":");
    number_array(response.single_kernel, CR_PRIVILEGE_COUNT);
    printf(",\"sets\":[");
    for (i = 0; i < CR_SET_COUNT; ++i) {
        const CR_SET_OBSERVATION *row = &response.sets[i];
        printf("%s{\"mode\":%lu,\"control\":%lu,\"count\":%lu,\"result\":%lu,\"attributes_before\":",
               i ? "," : "", (unsigned long)row->mode, (unsigned long)row->control,
               (unsigned long)row->count, (unsigned long)row->result);
        number_array(row->attributes_before, CR_PRIVILEGE_COUNT);
        printf(",\"attributes_after\":");
        number_array(row->attributes_after, CR_PRIVILEGE_COUNT);
        putchar('}');
    }
    printf("]}\n");
    return response.token_query_status < 0 ? 1 : 0;
}

static int observe_case(const char *label, HANDLE device)
{
    HANDLE token = NULL, check_token = NULL;
    BOOL impersonating;
    int result;
    result = select_token(&token, &impersonating);
    if (!result) {
        result |= token_snapshot(token, impersonating, label);
        if (impersonating) {
            result |= user_checks(token, label, TRUE);
        } else if (!DuplicateTokenEx(token, TOKEN_QUERY, NULL, SecurityIdentification,
                                     TokenImpersonation, &check_token)) {
            result |= error_record("DuplicateTokenEx.primary-check-copy", GetLastError());
        } else {
            /* PrivilegeCheck's documented input is an impersonation token.
             * This does not change the calling thread or the kernel's subject. */
            result |= user_checks(check_token, label, FALSE);
            CloseHandle(check_token);
        }
    }
    if (device != INVALID_HANDLE_VALUE) result |= kernel_checks(device, label);
    if (token) CloseHandle(token);
    return result;
}

static int impersonated_case(HANDLE primary, HANDLE device, const char *label, DWORD change, BOOL adjust)
{
    HANDLE token = NULL;
    TOKEN_PRIVILEGES mutation;
    DWORD error;
    int result;
    if (!DuplicateTokenEx(primary, TOKEN_QUERY | TOKEN_IMPERSONATE | TOKEN_ADJUST_PRIVILEGES,
                           NULL, SecurityImpersonation, TokenImpersonation, &token))
        return error_record("DuplicateTokenEx", GetLastError());
    if (adjust) {
        mutation.PrivilegeCount = 1;
        mutation.Privileges[0].Luid = privilege_values[0];
        mutation.Privileges[0].Attributes = change;
        SetLastError(ERROR_SUCCESS);
        if (!AdjustTokenPrivileges(token, FALSE, &mutation, 0, NULL, NULL) ||
            (error = GetLastError()) != ERROR_SUCCESS) {
            error = GetLastError();
            CloseHandle(token);
            return error_record("AdjustTokenPrivileges.own-copy", error);
        }
    }
    if (!SetThreadToken(NULL, token)) {
        error = GetLastError();
        CloseHandle(token);
        return error_record("SetThreadToken.own-copy", error);
    }
    result = observe_case(label, device);
    if (!RevertToSelf()) {
        context_restore_failed = TRUE;
        result = error_record("RevertToSelf", GetLastError());
    }
    CloseHandle(token);
    return result;
}

int main(int argc, char **argv)
{
    HANDLE primary = NULL, initial = NULL, device = INVALID_HANDLE_VALUE;
    DWORD error;
    unsigned i;
    int result = 1;
    BOOL kernel;
    if (argc != 2 || (strcmp(argv[1], "--user") && strcmp(argv[1], "--kernel"))) {
        fprintf(stderr, "Usage: cider-reference.exe --user | --kernel\n");
        return 2;
    }
    kernel = !strcmp(argv[1], "--kernel");
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_IMPERSONATE, TRUE, &initial) &&
        (error = GetLastError()) != ERROR_NO_TOKEN)
        return error_record("OpenThreadToken.initial", error);
    if (!RevertToSelf()) { result = error_record("RevertToSelf.initial", GetLastError()); goto cleanup; }
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &primary)) {
        result = error_record("OpenProcessToken.primary", GetLastError()); goto cleanup;
    }
    for (i = 0; i < CR_PRIVILEGE_COUNT; ++i)
        if (!LookupPrivilegeValueA(NULL, privilege_names[i], &privilege_values[i])) {
            result = error_record("LookupPrivilegeValue", GetLastError()); goto cleanup;
        }
    if (kernel) {
        device = CreateFileW(CR_WIN32_NAME, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (device == INVALID_HANDLE_VALUE) {
            result = error_record("Open.reference-device", GetLastError()); goto cleanup;
        }
    }
    printf("{\"schema\":\"cider.windows-reference-session/v1\",\"protocol\":%u,"
           "\"source_commit\":\"%s\",\"msc_full_ver\":%lu,\"scope\":\"%s\","
           "\"pointer_bits\":%u,\"initial_thread_impersonation\":%s,"
           "\"runtime_has_wine_version_export\":%s",
           CR_PROTOCOL_VERSION, CR_SOURCE_COMMIT, (unsigned long)CR_MSC_FULL_VER,
           kernel ? "native-kernel-and-win32" : "win32-only",
           (unsigned)(sizeof(void *) * 8), initial ? "true" : "false",
           GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version") ? "true" : "false");
    os_observation();
    printf(",\"privileges\":[");
    for (i = 0; i < CR_PRIVILEGE_COUNT; ++i)
        printf("%s{\"name\":\"%s\",\"luid_low\":%lu,\"luid_high\":%ld}", i ? "," : "",
               privilege_names[i], (unsigned long)privilege_values[i].LowPart, (long)privilege_values[i].HighPart);
    printf("]}\n");
    result = observe_case("primary-before", device);
    result |= impersonated_case(primary, device, "impersonation-copy", 0, FALSE);
    if (context_restore_failed) goto cleanup;
    result |= impersonated_case(primary, device, "impersonation-disabled", 0, TRUE);
    if (context_restore_failed) goto cleanup;
    result |= impersonated_case(primary, device, "impersonation-removed", SE_PRIVILEGE_REMOVED, TRUE);
    if (context_restore_failed) goto cleanup;
    result |= observe_case("primary-after", device);
cleanup:
    if (initial) {
        if (!SetThreadToken(NULL, initial)) result = error_record("Restore.initial-thread-token", GetLastError());
        CloseHandle(initial);
    } else if (!RevertToSelf()) result = error_record("Restore.primary", GetLastError());
    if (device != INVALID_HANDLE_VALUE) CloseHandle(device);
    if (primary) CloseHandle(primary);
    printf("{\"schema\":\"cider.windows-reference-completion/v1\",\"completed\":%s,"
           "\"scope\":\"%s\"}\n", result ? "false" : "true", kernel ? "native-kernel-and-win32" : "win32-only");
    if (fflush(stdout) != 0 || ferror(stdout)) return 3;
    return result;
}
