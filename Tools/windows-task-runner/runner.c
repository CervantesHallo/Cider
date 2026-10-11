/* Ordinary CLI tasks only. This is process ownership, not a security sandbox. */
#define _WIN32_WINNT 0x0a00
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define COMMAND_CAP 32767
#define CLEANUP_MS 5000

struct deadline_watch {
    HANDLE finished;
    PVOID volatile job;
    ULONGLONG deadline;
};

/* Remains live even when the main thread is blocked in creation or receipt I/O. */
static DWORD WINAPI hard_deadline(void *parameter)
{
    struct deadline_watch *watch = parameter;
    for (;;) {
        ULONGLONG now = GetTickCount64();
        DWORD wait;
        if (now >= watch->deadline) {
            HANDLE job = InterlockedCompareExchangePointer(&watch->job, NULL, NULL);
            if (job) TerminateJobObject(job, ERROR_TIMEOUT);
            /* No invented cleanup receipt: kill-on-close remains the final safeguard. */
            TerminateProcess(GetCurrentProcess(), ERROR_TIMEOUT);
            ExitProcess(ERROR_TIMEOUT);
        }
        wait = (DWORD)(watch->deadline - now);
        if (wait > 250) wait = 250;
        if (WaitForSingleObject(watch->finished, wait) == WAIT_OBJECT_0) return 0;
    }
}

static BOOL reservation_valid(const wchar_t *id)
{
    size_t i;
    if (wcslen(id) != 36) return FALSE;
    for (i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (id[i] != L'-') return FALSE;
        } else if (!((id[i] >= L'0' && id[i] <= L'9') ||
                     (id[i] >= L'a' && id[i] <= L'f'))) return FALSE;
    }
    return TRUE;
}

/* Always quote each argv element, including empty strings and trailing slashes. */
static BOOL put(wchar_t *buffer, size_t *used, wchar_t ch)
{
    if (*used >= COMMAND_CAP - 1) return FALSE;
    buffer[(*used)++] = ch;
    return TRUE;
}

static BOOL quote(wchar_t *buffer, size_t *used, const wchar_t *arg)
{
    size_t slashes, n;
    if (!put(buffer, used, L'"')) return FALSE;
    while (*arg) {
        slashes = 0;
        while (*arg == L'\\') { ++slashes; ++arg; }
        n = (*arg == L'"' || !*arg) ? slashes * 2 : slashes;
        while (n--) if (!put(buffer, used, L'\\')) return FALSE;
        if (!*arg) break;
        if (*arg == L'"' && !put(buffer, used, L'\\')) return FALSE;
        if (!put(buffer, used, *arg++)) return FALSE;
    }
    return put(buffer, used, L'"');
}

static BOOL local_path(const wchar_t *input, wchar_t *output, BOOL image)
{
    wchar_t root[4];
    DWORD n, type;
    if (wcslen(input) < 3 || input[1] != L':' || input[2] != L'\\' ||
        !((input[0] >= L'A' && input[0] <= L'Z') ||
          (input[0] >= L'a' && input[0] <= L'z')) || wcschr(input + 2, L':'))
        return FALSE;
    n = GetFullPathNameW(input, COMMAND_CAP, output, NULL);
    if (!n || n >= COMMAND_CAP) return FALSE;
    root[0] = output[0]; root[1] = L':'; root[2] = L'\\'; root[3] = 0;
    type = GetDriveTypeW(root);
    return type == DRIVE_FIXED || (image && type == DRIVE_CDROM);
}

/* Reject existing junction/symlink parents; no network or device paths. */
static BOOL plain_parents(const wchar_t *path)
{
    wchar_t *copy = _wcsdup(path);
    size_t i;
    DWORD attr;
    BOOL ok = copy != NULL;
    if (!copy) return FALSE;
    for (i = 3; copy[i] && ok; ++i) {
        if (copy[i] != L'\\') continue;
        copy[i] = 0;
        attr = GetFileAttributesW(copy);
        ok = attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) &&
             !(attr & FILE_ATTRIBUTE_REPARSE_POINT);
        copy[i] = L'\\';
    }
    free(copy);
    return ok;
}

static BOOL window_open(DWORD needed_ms)
{
    FILETIME utc;
    ULARGE_INTEGER clock;
    SYSTEMTIME local;
    DWORD left;
    GetSystemTimeAsFileTime(&utc);
    clock.LowPart = utc.dwLowDateTime; clock.HighPart = utc.dwHighDateTime;
    clock.QuadPart += 8ULL * 60 * 60 * 10000000;
    utc.dwLowDateTime = clock.LowPart; utc.dwHighDateTime = clock.HighPart;
    if (!FileTimeToSystemTime(&utc, &local) || local.wHour < 9 || local.wHour >= 17)
        return FALSE;
    left = ((17U - local.wHour) * 3600U - local.wMinute * 60U - local.wSecond) * 1000U;
    left -= local.wMilliseconds;
    return needed_ms <= left;
}

static BOOL stop_requested(const wchar_t *path)
{
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) return TRUE;
    /* Unreadable state is not permission to continue. */
    return GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND;
}

static BOOL active_count(HANDLE job, DWORD *active, DWORD *total)
{
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info;
    if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation,
                                   &info, sizeof(info), NULL)) return FALSE;
    *active = info.ActiveProcesses; *total = info.TotalProcesses;
    return TRUE;
}

static BOOL receipt_write(HANDLE file, const wchar_t *id, const char *status,
                          BOOL clean, ULONGLONG elapsed, DWORD seconds, DWORD error,
                          BOOL started, BOOL exit_known, DWORD exit_code, DWORD total)
{
    char json[1400], exit_value[32];
    DWORD written;
    int length;
    const char *started_value = strcmp(status, "running") == 0 ? "null" :
                                (started ? "true" : "false");
    LARGE_INTEGER zero;
    zero.QuadPart = 0;
    if (exit_known) snprintf(exit_value, sizeof(exit_value), "%lu", (unsigned long)exit_code);
    else strcpy(exit_value, "null");
    length = snprintf(json, sizeof(json),
        "{\"schema\":\"cider.windows-run-cleanup/v1\",\"reservation_id\":\"%ls\","
        "\"runner_schema\":\"cider.windows-cli-job/v1\",\"status\":\"%s\","
        "\"cleanup_confirmed\":%s,\"elapsed_seconds\":%.3f,\"reserved_seconds\":%lu,"
        "\"elapsed_scope\":\"owned-child-cleanup-before-receipt-publication\","
        "\"win32_error\":%lu,\"child_started\":%s,\"primary_exit_code\":%s,"
        "\"job_total_processes\":%lu}\n", id, status, clean ? "true" : "false",
        (double)elapsed / 1000.0, (unsigned long)seconds, (unsigned long)error,
        started_value, exit_value, (unsigned long)total);
    if (length < 0 || (size_t)length >= sizeof(json)) return FALSE;
    return SetFilePointerEx(file, zero, NULL, FILE_BEGIN) &&
        WriteFile(file, json, (DWORD)length, &written, NULL) && written == (DWORD)length &&
        SetEndOfFile(file) && FlushFileBuffers(file);
}

int wmain(int argc, wchar_t **argv)
{
    wchar_t *end, *command = NULL, *receipt = NULL, *partial = NULL, *exe = NULL, *stop = NULL;
    unsigned long parsed;
    DWORD seconds, budget_ms, error = ERROR_SUCCESS, active = 0, total = 0, exit_code = 0;
    DWORD attr;
    ULONGLONG elapsed;
    ULONGLONG begin = GetTickCount64(), deadline, run_deadline;
    size_t used = 0;
    int i, result = 1;
    HANDLE job = NULL, file = INVALID_HANDLE_VALUE, image = INVALID_HANDLE_VALUE;
    HANDLE monitor = NULL;
    struct deadline_watch watch;
    HANDLE stdio[3] = {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    STARTUPINFOEXW startup;
    PROCESS_INFORMATION process;
    SIZE_T list_size = 0;
    BOOL list_ready = FALSE, created = FALSE, assigned = FALSE, started = FALSE;
    BOOL clean = TRUE, exit_known = FALSE, in_job = FALSE;
    const char *status = "setup-failed";
    ZeroMemory(&process, sizeof(process)); ZeroMemory(&startup, sizeof(startup));
    ZeroMemory(&watch, sizeof(watch));
    if (argc < 6 || !reservation_valid(argv[1])) {
        fputs("Usage: runner UUID SECONDS RECEIPT STOP_FILE ABSOLUTE_EXE [ARG ...]\n", stderr);
        return 2;
    }
    for (end = argv[2]; *end; ++end) if (*end < L'0' || *end > L'9') return 2;
    parsed = wcstoul(argv[2], &end, 10);
    if (!*argv[2] || *end || parsed < 10 || parsed > 600) return 2;
    seconds = (DWORD)parsed; budget_ms = seconds * 1000U;
    deadline = begin + budget_ms; run_deadline = deadline - CLEANUP_MS;
    watch.deadline = deadline;
    watch.finished = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!watch.finished) return 1;
    monitor = CreateThread(NULL, 0, hard_deadline, &watch, 0, NULL);
    if (!monitor) { CloseHandle(watch.finished); return 1; }
    receipt = calloc(COMMAND_CAP, sizeof(wchar_t)); partial = calloc(COMMAND_CAP, sizeof(wchar_t));
    exe = calloc(COMMAND_CAP, sizeof(wchar_t)); stop = calloc(COMMAND_CAP, sizeof(wchar_t));
    command = calloc(COMMAND_CAP, sizeof(wchar_t));
    if (!receipt || !partial || !exe || !stop || !command) goto done;
    if (!local_path(argv[3], receipt, FALSE) || !plain_parents(receipt) ||
        !local_path(argv[4], stop, FALSE) || !plain_parents(stop) ||
        !local_path(argv[5], exe, TRUE) || !plain_parents(exe) ||
        wcslen(receipt) > COMMAND_CAP - 9 || !window_open(budget_ms) || stop_requested(stop))
        goto done;
    if (GetFileAttributesW(receipt) != INVALID_FILE_ATTRIBUTES ||
        GetLastError() != ERROR_FILE_NOT_FOUND) goto done;
    wcscpy(partial, receipt); wcscat(partial, L".partial");
    file = CreateFileW(partial, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) goto done;
    attr = GetFileAttributesW(exe);
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) goto done;
    /* Keep the image open without write/delete sharing while creating the child. */
    image = CreateFileW(exe, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                        FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (image == INVALID_HANDLE_VALUE) goto done;
    for (i = 5; i < argc; ++i) {
        if ((i != 5 && !put(command, &used, L' ')) || !quote(command, &used, i == 5 ? exe : argv[i])) goto done;
    }
    command[used] = 0;
    job = CreateJobObjectW(NULL, NULL); /* Unnamed, not inherited by the child. */
    if (!job) goto done;
    InterlockedExchangePointer(&watch.job, job);
    ZeroMemory(&limits, sizeof(limits));
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) goto done;
    stdio[0] = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                          &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (stdio[0] == INVALID_HANDLE_VALUE) goto done;
    if (!DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_OUTPUT_HANDLE), GetCurrentProcess(),
                         &stdio[1], 0, TRUE, DUPLICATE_SAME_ACCESS) ||
        !DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_ERROR_HANDLE), GetCurrentProcess(),
                         &stdio[2], 0, TRUE, DUPLICATE_SAME_ACCESS)) goto done;
    InitializeProcThreadAttributeList(NULL, 2, 0, &list_size);
    startup.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, list_size);
    if (!startup.lpAttributeList || !InitializeProcThreadAttributeList(startup.lpAttributeList, 2, 0, &list_size)) goto done;
    list_ready = TRUE;
    if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   stdio, sizeof(stdio), NULL, NULL)) goto done;
    /* Bind at creation; no unowned interval between CreateProcess and AssignProcess. */
    if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST,
                                   &job, sizeof(job), NULL, NULL)) goto done;
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = stdio[0]; startup.StartupInfo.hStdOutput = stdio[1];
    startup.StartupInfo.hStdError = stdio[2];
    if (GetTickCount64() >= run_deadline || !window_open(0) || stop_requested(stop)) goto done;
    if (!CreateProcessW(exe, command, NULL, NULL, TRUE,
                        CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
                        &startup.StartupInfo, &process)) goto done;
    created = TRUE; clean = FALSE;
    if (!IsProcessInJob(process.hProcess, job, &in_job) || !in_job) goto done;
    assigned = TRUE;
    if (!receipt_write(file, argv[1], "running", FALSE, GetTickCount64() - begin,
                        seconds, 0, FALSE, FALSE, 0, 1)) goto done;
    if (GetTickCount64() >= run_deadline || !window_open(0) || stop_requested(stop)) goto done;
    if (ResumeThread(process.hThread) == (DWORD)-1) goto done;
    started = TRUE;
#ifdef CIDER_RUNNER_DEADLINE_FIXTURE
    /* A separate fixture binary, never enabled by a runtime flag. */
    Sleep(INFINITE);
#endif
    for (;;) {
        if (!active_count(job, &active, &total)) goto done;
        if (!active) { clean = TRUE; status = "completed"; break; }
        if (stop_requested(stop)) { status = "stopped"; break; }
        if (!window_open(0)) { status = "window-closed"; break; }
        if (GetTickCount64() >= run_deadline) { status = "timeout"; break; }
        Sleep(50);
    }
    error = ERROR_SUCCESS;
done:
    if (strcmp(status, "setup-failed") == 0) error = GetLastError();
    if (created && !clean) {
        BOOL terminated = assigned ? TerminateJobObject(job, ERROR_CANCELLED) :
                                     TerminateProcess(process.hProcess, ERROR_CANCELLED);
        if (!terminated) error = GetLastError();
        do {
            if (assigned) {
                if (active_count(job, &active, &total) && !active) clean = TRUE;
            } else if (WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0) clean = TRUE;
            if (clean || GetTickCount64() >= deadline) break;
            Sleep(20);
        } while (TRUE);
    }
    if (created && WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0)
        exit_known = GetExitCodeProcess(process.hProcess, &exit_code);
    if (!clean) status = "cleanup-unconfirmed";
    elapsed = GetTickCount64() - begin;
    if (file != INVALID_HANDLE_VALUE) {
        BOOL written = receipt_write(file, argv[1], status, clean, elapsed,
                                     seconds, error, started, exit_known, exit_code, total);
        CloseHandle(file); file = INVALID_HANDLE_VALUE;
        if (written && MoveFileExW(partial, receipt, MOVEFILE_WRITE_THROUGH) && clean &&
            strcmp(status, "completed") == 0 && exit_known && exit_code == 0) result = 0;
    }
    if (process.hThread) CloseHandle(process.hThread);
    if (process.hProcess) CloseHandle(process.hProcess);
    if (image != INVALID_HANDLE_VALUE) CloseHandle(image);
    for (i = 0; i < 3; ++i) if (stdio[i] != INVALID_HANDLE_VALUE && stdio[i]) CloseHandle(stdio[i]);
    if (list_ready) DeleteProcThreadAttributeList(startup.lpAttributeList);
    if (startup.lpAttributeList) HeapFree(GetProcessHeap(), 0, startup.lpAttributeList);
    free(command); free(receipt); free(partial); free(exe); free(stop);
    SetEvent(watch.finished);
    if (WaitForSingleObject(monitor, 1000) != WAIT_OBJECT_0) ExitProcess(ERROR_TIMEOUT);
    CloseHandle(monitor); CloseHandle(watch.finished);
    if (job) CloseHandle(job);
    return result;
}
