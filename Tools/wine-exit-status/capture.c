/* Candidate only: capture a child process's complete Windows exit status.
 * This is not a process-tree watchdog or a trusted launch-permission source. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define COMMAND_CAP 32767

static BOOL append(wchar_t *buffer, size_t *used, wchar_t value)
{
    if (*used >= COMMAND_CAP - 1) return FALSE;
    buffer[(*used)++] = value;
    return TRUE;
}

/* Preserve argv using minimal Windows quoting. Some GUI programs parse the raw
 * command line themselves, so quoting an ordinary switch can change its meaning. */
static BOOL quote(wchar_t *buffer, size_t *used, const wchar_t *argument)
{
    size_t slashes, count;
    if (*argument && !wcspbrk(argument, L" \t\"")) {
        while (*argument) if (!append(buffer, used, *argument++)) return FALSE;
        return TRUE;
    }
    if (!append(buffer, used, L'"')) return FALSE;
    while (*argument) {
        slashes = 0;
        while (*argument == L'\\') { ++slashes; ++argument; }
        count = (!*argument || *argument == L'"') ? slashes * 2 : slashes;
        while (count--) if (!append(buffer, used, L'\\')) return FALSE;
        if (!*argument) break;
        if (*argument == L'"' && !append(buffer, used, L'\\')) return FALSE;
        if (!append(buffer, used, *argument++)) return FALSE;
    }
    return append(buffer, used, L'"');
}

static BOOL plain_path(const wchar_t *input, wchar_t *output, BOOL directory)
{
    DWORD length, attributes;
    size_t i;
    wchar_t saved;
    if (wcslen(input) < 3 || input[1] != L':' || input[2] != L'\\' ||
        !((input[0] >= L'A' && input[0] <= L'Z') ||
          (input[0] >= L'a' && input[0] <= L'z')) || wcschr(input + 2, L':'))
        return FALSE;
    length = GetFullPathNameW(input, COMMAND_CAP, output, NULL);
    if (!length || length >= COMMAND_CAP) return FALSE;
    for (i = 3; output[i]; ++i) {
        if (output[i] != L'\\') continue;
        saved = output[i]; output[i] = 0;
        attributes = GetFileAttributesW(output);
        output[i] = saved;
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            !(attributes & FILE_ATTRIBUTE_DIRECTORY) || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            return FALSE;
    }
    if (!directory) return TRUE;
    attributes = GetFileAttributesW(output);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

static BOOL duration(const wchar_t *text, DWORD *milliseconds)
{
    unsigned long value = 0;
    if (!*text) return FALSE;
    for (; *text; ++text) {
        if (*text < L'0' || *text > L'9' || value > 360000UL) return FALSE;
        value = value * 10 + (unsigned long)(*text - L'0');
        if (value > 3600000UL) return FALSE;
    }
    if (!value) return FALSE;
    *milliseconds = (DWORD)value;
    return TRUE;
}

static BOOL write_result(HANDLE file, const char *phase, DWORD error, DWORD pid,
                         BOOL created, BOOL terminated, BOOL exit_known, DWORD exit_code,
                         ULONGLONG elapsed)
{
    char json[768], exit_value[32];
    DWORD written;
    int length;
    if (exit_known) snprintf(exit_value, sizeof(exit_value), "%lu", (unsigned long)exit_code);
    else snprintf(exit_value, sizeof(exit_value), "null");
    length = snprintf(json, sizeof(json),
        "{\"schema\":\"cider.windows-exit-status/v1\",\"phase\":\"%s\","
        "\"win32_error\":%lu,\"child_pid\":%lu,\"child_created\":%s,"
        "\"primary_terminated\":%s,\"windows_exit_code\":%s,"
        "\"elapsed_ms\":%llu,\"cleanup_scope\":\"primary-process-only\"}\n",
        phase, (unsigned long)error, (unsigned long)pid,
        created ? "true" : "false", terminated ? "true" : "false", exit_value,
        (unsigned long long)elapsed);
    return length > 0 && (size_t)length < sizeof(json) &&
           WriteFile(file, json, (DWORD)length, &written, NULL) && written == (DWORD)length &&
           FlushFileBuffers(file);
}

int wmain(int argc, wchar_t **argv)
{
    wchar_t *result_path = NULL, *image = NULL, *cwd = NULL, *command = NULL;
    DWORD milliseconds, error = ERROR_SUCCESS, wait, attributes;
    DWORD exit_code = 0;
    size_t used = 0;
    int i, result = 2;
    HANDLE file = INVALID_HANDLE_VALUE;
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    BY_HANDLE_FILE_INFORMATION information;
    BOOL created = FALSE, terminated = FALSE, exit_known = FALSE;
    ULONGLONG begin = GetTickCount64();
    const char *phase = "setup-failed";
    ZeroMemory(&startup, sizeof(startup)); startup.cb = sizeof(startup);
    ZeroMemory(&process, sizeof(process));
    if (argc < 5 || !duration(argv[2], &milliseconds)) goto done;
    result_path = calloc(COMMAND_CAP, sizeof(wchar_t));
    image = calloc(COMMAND_CAP, sizeof(wchar_t));
    command = calloc(COMMAND_CAP, sizeof(wchar_t));
    if (!result_path || !image || !command) { error = ERROR_NOT_ENOUGH_MEMORY; goto done; }
    if (!plain_path(argv[1], result_path, FALSE) || !plain_path(argv[4], image, FALSE)) {
        error = ERROR_INVALID_NAME; goto done;
    }
    attributes = GetFileAttributesW(image);
    if (attributes == INVALID_FILE_ATTRIBUTES) { error = GetLastError(); goto done; }
    if (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) {
        error = ERROR_INVALID_NAME; goto done;
    }
    if (wcscmp(argv[3], L"-") != 0) {
        cwd = calloc(COMMAND_CAP, sizeof(wchar_t));
        if (!cwd) { error = ERROR_NOT_ENOUGH_MEMORY; goto done; }
        if (!plain_path(argv[3], cwd, TRUE)) { error = ERROR_INVALID_NAME; goto done; }
    }
    if (!quote(command, &used, image)) { error = ERROR_BAD_LENGTH; goto done; }
    for (i = 5; i < argc; ++i) {
        if (!append(command, &used, L' ') || !quote(command, &used, argv[i])) {
            error = ERROR_BAD_LENGTH; goto done;
        }
    }
    command[used] = 0;
    /* Reserve a fresh result, never overwrite an old report or inherit its handle. */
    file = CreateFileW(result_path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) { error = GetLastError(); goto done; }
    if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandle(file, &information) ||
        information.nNumberOfLinks != 1 ||
        (information.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        error = ERROR_INVALID_HANDLE; goto done;
    }
    /* Explicit image + writable command buffer; no credential, service, or token operations. */
    if (!CreateProcessW(image, command, NULL, NULL, FALSE, 0, NULL, cwd, &startup, &process)) {
        error = GetLastError(); phase = "create-failed"; goto done;
    }
    created = TRUE;
    wait = WaitForSingleObject(process.hProcess, milliseconds);
    if (wait == WAIT_OBJECT_0) {
        terminated = TRUE;
        if (GetExitCodeProcess(process.hProcess, &exit_code)) {
            exit_known = TRUE; phase = "captured";
        } else { error = GetLastError(); phase = "query-failed"; }
    } else {
        error = wait == WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError();
        phase = wait == WAIT_TIMEOUT ? "timeout" : "wait-failed";
        /* Only this created primary process is ours; child hand-offs are intentionally not claimed clean. */
        if (TerminateProcess(process.hProcess, ERROR_CANCELLED) &&
            WaitForSingleObject(process.hProcess, 5000) == WAIT_OBJECT_0) {
            terminated = TRUE;
            exit_known = GetExitCodeProcess(process.hProcess, &exit_code);
        }
    }
done:
    if (!created && !error) error = ERROR_INVALID_PARAMETER;
    if (file != INVALID_HANDLE_VALUE) {
        BOOL written = write_result(file, phase, error, process.dwProcessId, created,
                                    terminated, exit_known, exit_code, GetTickCount64() - begin);
        if (written && created && terminated && exit_known && !error && strcmp(phase, "captured") == 0) result = 0;
        CloseHandle(file);
    }
    if (process.hThread) CloseHandle(process.hThread);
    if (process.hProcess) CloseHandle(process.hProcess);
    free(result_path); free(image); free(cwd); free(command);
    /* Zero means a complete captured report, not that the child succeeded. Read windows_exit_code. */
    return result;
}
