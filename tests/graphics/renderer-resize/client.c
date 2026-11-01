/* Standard Windows renderer resize, v1 (copied from owner-transition v2). Console entry point; link d3d11,
 * dxgi and uuid. No private Wine/Native exports, shaders, helper DLLs or hooks.
 *
 * Host:     client.exe --host C:\CiderTests\opposite.exe <32 hex nonce>
 * Renderer: peer.exe --renderer <8 hex Root2> <32 lowercase hex nonce>
 *           <8 hex read handle> <8 hex write handle>
 * Renderer owns Root1/A/B; controller owns Root2. Phase 0 bootstraps their
 * actual USER identities over the same 80-byte ordinary pipe before any D3D.
 * Wire integers use Windows little endian. Nonce halves are parsed as two
 * consecutive 16-digit hexadecimal integers. pattern 1 = A red / B blue;
 * pattern 2 = A blue / B red; pattern 3 = A red / B red;
 * pattern 0 = bootstrap or phase 4 GPU drain / local COM release / owned
 * child destruction. Root1 lives until the final externally gated ACK.
 *
 * The external parent owns PNG/Native-marker acceptance and a 210 s watchdog.
 * In-process clocks bound polling, not an uninterruptible driver/COM call.
 * All such calls have before/after budget observations. Work is <=150 s,
 * total <=180 s, with a separate <=10 s failure cleanup allowance. Neither
 * COM Release nor process exit proves Native ownership zero or display proof.
 * No success path uses TerminateProcess. A failure may attempt it once, only
 * against the PROCESS_INFORMATION returned by this host's CreateProcessW.
 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#ifndef __USE_MINGW_ANSI_STDIO
#define __USE_MINGW_ANSI_STDIO 1
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

enum {
    TOKEN_MAGIC = 0x43465231, TOKEN_VERSION = 1,
    INITIAL_WIDTH = 160, INITIAL_HEIGHT = 100,
    RESIZED_WIDTH = 200, RESIZED_HEIGHT = 120,
    READ_A = 1, READ_B = 2, PRESENT_A = 4, PRESENT_B = 8,
    LOCAL_COM_RELEASED = 16, BOOTSTRAP = 32, FRAME_FLAGS = 15,
    WORK_MS = 150000, TOTAL_MS = 180000, CLEANUP_MS = 10000,
    GATE_MS = 45000, GPU_MS = 5000, GRACE_MS = 5000
};

typedef struct capture_token64 {
    uint32_t magic, size, version, phase;
    uint32_t root, host_pid, renderer_pid, pattern;
    uint64_t nonce[2];
    uint32_t child_a, child_b, width, height;
} capture_token64;

typedef struct pipe_message80 {
    capture_token64 token;
    int32_t hresult;
    uint32_t flags;
    uint64_t sequence;
} pipe_message80;

_Static_assert(sizeof(capture_token64) == 64, "capture token size");
_Static_assert(offsetof(capture_token64, nonce) == 32, "nonce offset");
_Static_assert(offsetof(capture_token64, child_a) == 48, "children offset");
_Static_assert(offsetof(capture_token64, width) == 56, "dimensions offset");
_Static_assert(sizeof(pipe_message80) == 80, "pipe message size");
_Static_assert(offsetof(pipe_message80, hresult) == 64, "HRESULT offset");
_Static_assert(offsetof(pipe_message80, sequence) == 72, "sequence offset");
_Static_assert(sizeof(HRESULT) == 4, "Windows HRESULT size");

typedef struct target {
    IDXGISwapChain *swap;
    ID3D11Texture2D *back, *staging;
    ID3D11RenderTargetView *rtv;
    D3D11_MAPPED_SUBRESOURCE mapping;
    BOOL mapped;
    UINT width, height; /* last validated actual backbuffer GetDesc, not the request */
} target;

typedef struct gpu_state {
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    IDXGIDevice *dxgi;
    IDXGIAdapter *adapter;
    IDXGIFactory *factory;
    ID3D11Query *query;
    target targets[2];
    uintptr_t device_identity, context_identity, query_identity, swap_identity[2];
} gpu_state;

static const char *role = "host";
static char nonce_text[33];
static wchar_t title[80];
static uint64_t nonce_words[2];
static DWORD self_pid, host_pid, renderer_pid;
/* Active/expected aliases never authorize destruction. Root1 and child_owners
 * belong only to renderer; root2_owner belongs only to host. Record every
 * CreateWindow result in its owner slot BEFORE canonicalization/post-clock. */
static HWND root, roots[2], root1_owner, root2_owner, children[2], child_owners[2];
static uint32_t root_id, root_ids[2], child_ids[2];
static const wchar_t inactive_title[] = L"Cider renderer inactive";
static HANDLE output, input_pipe, output_pipe, peer_process, phase_file;
static uint64_t self_birth, peer_birth;
static LARGE_INTEGER frequency, first_qpc, last_qpc;
static ULONGLONG first_tick, last_tick;
static uint64_t elapsed_ms, cleanup_start;
static BOOL clock_ready, clock_bad, timeout_bad, io_bad, cleaning;
static unsigned int checks, failures, phase_now, phases_done;
static unsigned int swaps_done, frames_done, readbacks_done, presents_done;
static unsigned int occluded_done, pipe_sends, pipe_receives, releases_done;
static unsigned int resize_calls, resize_releases, pixels_read;
static UINT image_width = INITIAL_WIDTH, image_height = INITIAL_HEIGHT;
static unsigned int peer_frames_reported, peer_readbacks_reported;
static unsigned int peer_presents_reported, peer_frame_reports;
static BOOL local_released, gpu_drained, peer_release_reported;
static BOOL kernel_signaled, joined_zero, force_attempted, force_succeeded;
static DWORD joined_exit = STILL_ACTIVE;

/* Every emitted record is exactly one WriteFile including its newline. raw[]
 * values are documented at their call sites; HRESULT always preserves the
 * actual signed 32-bit value, including positive DXGI status codes. A partial
 * stdout write is sticky and is never repaired by splitting the record. */
static BOOL record(const char *event, const char *label, BOOL ok, HRESULT hr,
                   uint64_t a, uint64_t b, uint64_t c, uint64_t d)
{
    char line[2304];
    DWORD written = 0;
    BOOL wrote;
    int n;
    ++checks;
    if (!ok) ++failures;
    n = snprintf(line, sizeof(line),
        "{\"type\":\"renderer-consumer\",\"role\":\"%s\",\"pid\":%lu,"
        "\"bits\":%u,\"nonce\":\"%s\",\"event\":\"%s\",\"check\":%u,"
        "\"label\":\"%s\",\"ok\":%s,\"hr\":%" PRId32 ",\"hr_hex\":\"%08" PRIx32 "\","
        "\"raw\":[%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "],"
        "\"elapsed_ms\":%" PRIu64 ",\"phase\":%u,\"root\":%" PRIu32 ","
        "\"root1\":%" PRIu32 ",\"root2\":%" PRIu32 ","
        "\"child_a\":%" PRIu32 ",\"child_b\":%" PRIu32 ","
        "\"host_pid\":%lu,\"renderer_pid\":%lu,\"self_birth\":%" PRIu64 ","
        "\"peer_birth\":%" PRIu64 ",\"checks\":%u,\"failures\":%u,"
        "\"phases\":%u,\"swapchains\":%u,\"frames\":%u,\"readbacks\":%u,"
        "\"presents\":%u,\"occluded_presents\":%u,\"pipe_sends\":%u,"
        "\"pipe_receives\":%u,\"com_release_calls\":%u,"
        "\"resize_calls\":%u,\"resize_release_calls\":%u,\"pixels_read\":%u,"
        "\"image_width\":%u,\"image_height\":%u,"
        "\"local_com_released\":%s,\"gpu_drained\":%s,"
        "\"peer_frame_reports\":%u,\"peer_frames_reported\":%u,"
        "\"peer_readbacks_reported\":%u,\"peer_presents_reported\":%u,"
        "\"peer_local_release_reported\":%s,\"kernel_signaled\":%s,"
        "\"joined_zero\":%s,\"exit_code\":%lu,\"force_attempted\":%s,"
        "\"force_succeeded\":%s,\"io_error\":%s,\"clock_error\":%s,\"timeout\":%s}\n",
        role, (unsigned long)self_pid, (unsigned int)(8 * sizeof(void *)),
        nonce_text, event, checks, label, ok ? "true" : "false",
        (int32_t)hr, (uint32_t)hr, a, b, c, d, elapsed_ms, phase_now,
        root_id, root_ids[0], root_ids[1], child_ids[0], child_ids[1], (unsigned long)host_pid,
        (unsigned long)renderer_pid, self_birth, peer_birth, checks, failures,
        phases_done, swaps_done, frames_done, readbacks_done, presents_done,
        occluded_done, pipe_sends, pipe_receives, releases_done,
        resize_calls, resize_releases, pixels_read, image_width, image_height,
        local_released ? "true" : "false", gpu_drained ? "true" : "false",
        peer_frame_reports, peer_frames_reported, peer_readbacks_reported,
        peer_presents_reported, peer_release_reported ? "true" : "false",
        kernel_signaled ? "true" : "false", joined_zero ? "true" : "false",
        (unsigned long)joined_exit, force_attempted ? "true" : "false",
        force_succeeded ? "true" : "false", io_bad ? "true" : "false",
        clock_bad ? "true" : "false", timeout_bad ? "true" : "false");
    if (n < 0 || (size_t)n >= sizeof(line)) {
        if (!io_bad) ++failures;
        io_bad = TRUE;
        return FALSE;
    }
    wrote = WriteFile(output, line, (DWORD)n, &written, NULL);
    if (!wrote || written != (DWORD)n) {
        if (!io_bad) ++failures;
        io_bad = TRUE;
    }
    return ok && !io_bad;
}

static HRESULT winhr(DWORD error)
{
    return error ? HRESULT_FROM_WIN32(error) : E_FAIL;
}

static BOOL budget(void)
{
    LARGE_INTEGER qpc = {0};
    ULONGLONG tick;
    uint64_t qms, tms;
    BOOL got = QueryPerformanceCounter(&qpc);
    DWORD error = got ? 0 : GetLastError();
    tick = GetTickCount64();
    if (!clock_ready || !got || qpc.QuadPart < last_qpc.QuadPart ||
        tick < last_tick || qpc.QuadPart < first_qpc.QuadPart ||
        (uint64_t)(qpc.QuadPart - first_qpc.QuadPart) > UINT64_MAX / 1000) {
        if (!clock_bad) {
            clock_bad = TRUE;
            record("clock", "monotonic-clock", FALSE, winhr(error),
                   (uint64_t)qpc.QuadPart, (uint64_t)last_qpc.QuadPart,
                   tick, last_tick);
        }
        return FALSE;
    }
    last_qpc = qpc;
    last_tick = tick;
    qms = (uint64_t)(qpc.QuadPart - first_qpc.QuadPart) * 1000 /
          (uint64_t)frequency.QuadPart;
    tms = tick - first_tick;
    elapsed_ms = qms > tms ? qms : tms;
    if (elapsed_ms >= TOTAL_MS ||
        (!cleaning && elapsed_ms >= WORK_MS) ||
        (cleaning && elapsed_ms - cleanup_start >= CLEANUP_MS)) {
        if (!timeout_bad) {
            timeout_bad = TRUE;
            record("timeout", "process-budget", FALSE, HRESULT_FROM_WIN32(ERROR_TIMEOUT),
                   elapsed_ms, WORK_MS, TOTAL_MS, cleanup_start);
        }
        return FALSE;
    }
    return !clock_bad;
}

static BOOL note(const char *event, const char *label, BOOL ok, HRESULT hr,
                 uint64_t a, uint64_t b, uint64_t c, uint64_t d)
{
    BOOL timely = budget();
    return record(event, label, ok && timely, hr, a, b, c, d);
}

static BOOL healthy(void)
{
    return budget() && !failures && !io_bad;
}

static void start_cleanup(void)
{
    (void)budget();
    if (!cleaning) { cleaning = TRUE; cleanup_start = elapsed_ms; }
}

static BOOL close_owned(HANDLE *handle, const char *label)
{
    HANDLE old = *handle;
    BOOL closed;
    DWORD error;
    if (!old) return TRUE;
    (void)budget();
    closed = CloseHandle(old);
    error = closed ? 0 : GetLastError();
    if (closed) *handle = NULL; /* ownership changes BEFORE post-clock/log */
    else io_bad = TRUE;
    return note("close", label, closed, closed ? S_OK : winhr(error),
                (uint64_t)(uintptr_t)old, error, closed, 0);
}

static BOOL pump(void)
{
    MSG msg;
    unsigned int i;
    for (i = 0; i < 64; ++i) {
        BOOL found;
        if (!budget() && !cleaning) return FALSE;
        found = PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE);
        if (!budget() && !cleaning) return FALSE;
        if (!found) break;
        if (msg.message == WM_QUIT)
            return record("window", "unexpected-WM_QUIT", FALSE, E_ABORT, 0, 0, 0, 0);
        TranslateMessage(&msg);
        if (!budget() && !cleaning) return FALSE;
        DispatchMessageW(&msg);
        if (!budget() && !cleaning) return FALSE;
    }
    return budget();
}

static BOOL pause_pumping(DWORD milliseconds)
{
    DWORD wait, error;
    if (!pump() || !budget()) return FALSE;
    wait = MsgWaitForMultipleObjectsEx(0, NULL, milliseconds, QS_ALLINPUT,
                                       MWMO_INPUTAVAILABLE);
    error = wait == WAIT_FAILED ? GetLastError() : 0;
    if (!budget()) return FALSE;
    if (wait != WAIT_TIMEOUT && wait != WAIT_OBJECT_0)
        return record("wait", "message-wait", FALSE, winhr(error), wait, error, 0, 0);
    return pump();
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static BOOL parse_hex(const char *text, size_t count, uint64_t *value)
{
    size_t i;
    uint64_t n = 0;
    if (strlen(text) != count || count > 16) return FALSE;
    for (i = 0; i < count; ++i) {
        int digit = hex_digit(text[i]);
        if (digit < 0) return FALSE;
        n = (n << 4) | (unsigned int)digit;
    }
    *value = n;
    return TRUE;
}

static BOOL parse_nonce(const char *text)
{
    char half[17];
    size_t i;
    if (strlen(text) != 32) return FALSE;
    for (i = 0; i < 32; ++i) {
        int digit = hex_digit(text[i]);
        if (digit < 0) return FALSE;
        nonce_text[i] = "0123456789abcdef"[digit];
    }
    nonce_text[32] = 0;
    memcpy(half, nonce_text, 16); half[16] = 0;
    if (!parse_hex(half, 16, &nonce_words[0])) return FALSE;
    memcpy(half, nonce_text + 16, 16);
    if (!parse_hex(half, 16, &nonce_words[1])) return FALSE;
    wcscpy(title, L"Cider renderer consumer ");
    for (i = 0; i < 32; ++i) title[24 + i] = (wchar_t)nonce_text[i];
    title[56] = 0;
    return TRUE;
}

/* HWNDs are USER handles with a fixed 32-bit wire value. Accept only the
 * documented zero/sign extensions, never an arbitrary truncated pointer. */
static BOOL window_id(HWND window, uint32_t *id)
{
    uintptr_t raw = (uintptr_t)window;
    uint32_t low = (uint32_t)raw;
    if (!low || (raw != (uintptr_t)low &&
                 raw != (uintptr_t)(intptr_t)(int32_t)low)) return FALSE;
    *id = low;
    return TRUE;
}

static HWND window_from_id(uint32_t id)
{
    return (HWND)(intptr_t)(int32_t)id;
}

static BOOL parse_id(const char *text, uint32_t *id)
{
    uint64_t n;
    if (!parse_hex(text, 8, &n) || !n || n == UINT32_MAX) return FALSE;
    *id = (uint32_t)n;
    return TRUE;
}

static BOOL kernel_wire_id(HANDLE handle, uint32_t *id)
{
    uint64_t raw = (uint64_t)(uintptr_t)handle;
    uint32_t low = (uint32_t)raw;
    if (!low || low == UINT32_MAX || (uint64_t)low != raw) return FALSE;
    *id = low;
    return TRUE;
}

static uint64_t filetime_value(FILETIME time)
{
    return ((uint64_t)time.dwHighDateTime << 32) | time.dwLowDateTime;
}

static BOOL process_birth(HANDLE process, uint64_t *birth, const char *label)
{
    FILETIME created, exited, kernel, user;
    BOOL got;
    DWORD error;
    if (!budget()) return FALSE;
    got = GetProcessTimes(process, &created, &exited, &kernel, &user);
    error = got ? 0 : GetLastError();
    if (got) *birth = filetime_value(created);
    return note("identity", label, got && *birth != 0, got ? S_OK : winhr(error),
                got ? *birth : 0, error, 0, 0);
}

static BOOL opposite_process(HANDLE peer)
{
    BOOL own_wow = FALSE, other_wow = FALSE, got;
    DWORD error;
    if (!budget()) return FALSE;
    got = IsWow64Process(GetCurrentProcess(), &own_wow);
    error = got ? 0 : GetLastError();
    if (!note("identity", "self-WoW64", got, got ? S_OK : winhr(error),
              own_wow, sizeof(void *) * 8, error, 0)) return FALSE;
    if (!budget()) return FALSE;
    got = IsWow64Process(peer, &other_wow);
    error = got ? 0 : GetLastError();
    return note("identity", "actual-opposite-bitness", got && own_wow != other_wow &&
                own_wow == (sizeof(void *) == 4), got ? S_OK : winhr(error),
                own_wow, other_wow, error, 0);
}

static BOOL peer_alive(void)
{
    DWORD wait, error;
    if (!budget()) return FALSE;
    wait = WaitForSingleObject(peer_process, 0);
    error = wait == WAIT_FAILED ? GetLastError() : 0;
    return note("identity", "peer-still-alive", wait == WAIT_TIMEOUT,
                wait == WAIT_FAILED ? winhr(error) : S_OK, wait, error, 0, 0);
}

/* Gate polling must service USER messages without flooding/draining stdout
 * with successful identity records on every 5 ms iteration. Full birth/PID
 * validation still occurs at every phase boundary, in both processes. */
static BOOL probe_peer_alive(void)
{
    DWORD wait, error;
    if (!budget()) return FALSE;
    wait = WaitForSingleObject(peer_process, 0);
    error = wait == WAIT_FAILED ? GetLastError() : 0;
    if (!budget()) return FALSE;
    if (wait == WAIT_TIMEOUT) return TRUE;
    return record("identity", "gate-peer-not-alive", FALSE,
                  wait == WAIT_FAILED ? winhr(error) : E_FAIL, wait, error, 0, 0);
}

static BOOL verify_births(void)
{
    uint64_t own = 0, other = 0;
    DWORD pid, expected = !strcmp(role, "host") ? renderer_pid : host_pid;
    if (!peer_alive() || !process_birth(GetCurrentProcess(), &own, "self-birth-probe") ||
        !process_birth(peer_process, &other, "peer-birth-probe")) return FALSE;
    if (!budget()) return FALSE;
    pid = GetProcessId(peer_process);
    return note("identity", "unchanged-process-identities", pid == expected &&
                expected != self_pid && own == self_birth && other == peer_birth,
                pid ? S_OK : winhr(GetLastError()), own, other, pid, expected);
}

typedef struct title_scan {
    unsigned int count;
    HWND match;
} title_scan;

static BOOL CALLBACK count_capture_title(HWND window, LPARAM parameter)
{
    title_scan *scan = (title_scan *)(uintptr_t)parameter;
    wchar_t actual[80];
    int length;
    if (!budget()) return FALSE;
    length = GetWindowTextW(window, actual, (int)(sizeof(actual) / sizeof(actual[0])));
    if (length == 56 && !wcscmp(actual, title)) {
        ++scan->count;
        scan->match = window;
    }
    return budget() && scan->count <= 1;
}

static BOOL verify_roots(void)
{
    unsigned int i;
    title_scan scan = {0, NULL};
    BOOL enumerated;
    DWORD error;
    for (i = 0; i < 2; ++i) {
        RECT client = {0};
        BOOL resized = phase_now >= (i ? 2u : 3u);
        HWND parent, ancestor;
        DWORD pid = 0, tid;
        BOOL valid, visible, got;
        wchar_t actual[80];
        int length;
        if (!budget()) return FALSE;
        valid = IsWindow(roots[i]);
        if (!note("window", "both-roots-IsWindow", valid, S_OK, root_ids[i], i, 0, 0)) return FALSE;
        if (!budget()) return FALSE;
        parent = GetParent(roots[i]);
        if (!note("window", "both-roots-top-level-parent", parent == NULL, S_OK,
                  (uint64_t)(uintptr_t)parent, root_ids[i], i, 0)) return FALSE;
        if (!budget()) return FALSE;
        ancestor = GetAncestor(roots[i], GA_ROOT);
        if (!note("window", "both-roots-top-level-ancestor", ancestor == roots[i], S_OK,
                  (uint64_t)(uintptr_t)ancestor, root_ids[i], i, 0)) return FALSE;
        if (!budget()) return FALSE;
        tid = GetWindowThreadProcessId(roots[i], &pid);
        if (!note("window", "both-roots-actual-owner", tid && pid == (i ? host_pid : renderer_pid),
                  tid ? S_OK : winhr(GetLastError()), root_ids[i], pid, i ? host_pid : renderer_pid, i)) return FALSE;
        if (!budget()) return FALSE;
        got = GetClientRect(roots[i], &client);
        error = got ? 0 : GetLastError();
        if (!note("window", "both-roots-phase-client-size", got && !client.left && !client.top &&
                  client.right == (resized ? 540 : 440) && client.bottom == (resized ? 300 : 240), got ? S_OK : winhr(error),
                  root_ids[i], (uint64_t)(int64_t)client.right,
                  (uint64_t)(int64_t)client.bottom, error)) return FALSE;
        if (!budget()) return FALSE;
        length = GetWindowTextW(roots[i], actual, (int)(sizeof(actual) / sizeof(actual[0])));
        if (!note("window", "both-roots-active-or-inactive-title", length > 0 &&
                  !wcscmp(actual, roots[i] == root ? title : inactive_title),
                  length ? S_OK : winhr(GetLastError()), root_ids[i], i,
                  (uint64_t)(unsigned int)length, roots[i] == root)) return FALSE;
        if (!budget()) return FALSE;
        visible = IsWindowVisible(roots[i]);
        if (!note("window", "only-active-root-visible", visible == (roots[i] == root),
                  S_OK, root_ids[i], i, visible, roots[i] == root)) return FALSE;
    }
    if (!budget()) return FALSE;
    SetLastError(0);
    enumerated = EnumWindows(count_capture_title, (LPARAM)(uintptr_t)&scan);
    error = enumerated ? 0 : GetLastError();
    return note("window", "unique-canonical-capture-title", enumerated && scan.count == 1 &&
                scan.match == root, enumerated ? S_OK : winhr(error),
                scan.count, (uint64_t)(uintptr_t)scan.match, root_id, error);
}

/* The host changes only its expected token identity before waiting. The
 * renderer makes exactly four ordinary SetParent calls over the whole run.
 * No driver refresh, re-install, COM transfer, or device/context/swapchain reconstruction occurs. */
static BOOL expect_phase_root(unsigned int phase)
{
    unsigned int index = phase == 2 ? 1 : 0;
    uint32_t previous = root_id;
    root = roots[index]; root_id = root_ids[index];
    image_width = phase >= 2 ? RESIZED_WIDTH : INITIAL_WIDTH;
    image_height = phase >= 2 ? RESIZED_HEIGHT : INITIAL_HEIGHT;
    return note("reparent", "phase-expected-root", root && root_id, S_OK,
                phase, previous, root_id, index);
}

static BOOL reparent_children(unsigned int phase)
{
    HWND old = root;
    uint32_t old_id = root_id;
    unsigned int i;
    BOOL changed, previous_visibility, visible;
    DWORD error;
    wchar_t old_title[80];
    int length;
    if (!expect_phase_root(phase) || !verify_births() || !budget()) return FALSE;
    changed = SetWindowTextW(old, inactive_title);
    error = changed ? 0 : GetLastError();
    if (!note("reparent", "old-root-remove-canonical-title", changed,
              changed ? S_OK : winhr(error), old_id, root_id, error, 0)) return FALSE;
    if (!budget()) return FALSE;
    length = GetWindowTextW(old, old_title, (int)(sizeof(old_title) / sizeof(old_title[0])));
    if (!note("reparent", "old-root-title-removed-before-new-title", length > 0 &&
              !wcscmp(old_title, inactive_title), length ? S_OK : winhr(GetLastError()),
              old_id, root_id, (uint64_t)(unsigned int)length, 0)) return FALSE;
    if (!budget()) return FALSE;
    previous_visibility = ShowWindow(old, SW_HIDE); /* return = previous state */
    if (!note("reparent", "old-root-ShowWindow-hide-return", TRUE, S_OK,
              old_id, previous_visibility, SW_HIDE, 0) || !budget()) return FALSE;
    visible = IsWindowVisible(old);
    if (!note("reparent", "old-root-actually-hidden", !visible, S_OK,
              old_id, visible, 0, 0) || !budget()) return FALSE;
    changed = SetWindowTextW(root, title);
    error = changed ? 0 : GetLastError();
    if (!note("reparent", "new-root-assign-canonical-title", changed,
              changed ? S_OK : winhr(error), root_id, old_id, error, 0) || !budget()) return FALSE;
    previous_visibility = ShowWindow(root, SW_SHOWNOACTIVATE);
    if (!note("reparent", "new-root-ShowWindow-show-return", TRUE, S_OK,
              root_id, previous_visibility, SW_SHOWNOACTIVATE, 0) || !budget()) return FALSE;
    visible = IsWindowVisible(root);
    if (!note("reparent", "new-root-actually-visible", visible, S_OK,
              root_id, visible, 0, 0)) return FALSE;
    for (i = 0; i < 2; ++i) {
        HWND previous, actual;
        if (!budget()) return FALSE;
        actual = GetParent(children[i]);
        if (!note("reparent", "before-SetParent-actual-old-parent", actual == old, S_OK,
                  child_ids[i], (uint64_t)(uintptr_t)actual, old_id, i) || !budget()) return FALSE;
        SetLastError(0);
        previous = SetParent(children[i], root);
        error = GetLastError(); /* nonzero on a non-NULL success is retained, not invented failure */
        if (!note("reparent", "ordinary-SetParent-returned-old-parent", previous == old,
                  previous ? S_OK : winhr(error), child_ids[i],
                  (uint64_t)(uintptr_t)previous, root_id, error) || !budget()) return FALSE;
        actual = GetParent(children[i]);
        if (!note("reparent", "after-SetParent-actual-new-parent", actual == root, S_OK,
                  child_ids[i], (uint64_t)(uintptr_t)actual, root_id, i) || !budget()) return FALSE;
        actual = GetAncestor(children[i], GA_ROOT);
        if (!note("reparent", "after-SetParent-actual-new-ancestor", actual == root, S_OK,
                  child_ids[i], (uint64_t)(uintptr_t)actual, root_id, i) || !budget()) return FALSE;
    }
    /* Both ordinary SetParent calls have completed before changing geometry.
     * The renderer may size borrowed Root2, but never acquires destruction ownership. */
    {
        RECT outer = {0, 0, 540, 300}, client = {0};
        LONG_PTR style, exstyle;
        BOOL menu;
        if (!budget()) return FALSE;
        SetLastError(0);
        style = GetWindowLongPtrW(root, GWL_STYLE);
        error = GetLastError();
        if (!note("resize-window", "resize-root-actual-style", style != 0,
                  style ? S_OK : winhr(error), root_id, (uint64_t)(uintptr_t)style, error, 0) || !budget()) return FALSE;
        SetLastError(0);
        exstyle = GetWindowLongPtrW(root, GWL_EXSTYLE);
        error = GetLastError();
        if (!note("resize-window", "resize-root-actual-exstyle", (exstyle || !error),
                  error ? winhr(error) : S_OK, root_id, (uint64_t)(uintptr_t)exstyle, error, 0) || !budget()) return FALSE;
        menu = GetMenu(root) != NULL;
        if (!note("resize-window", "resize-root-no-menu", !menu, S_OK, root_id, menu, 0, 0) || !budget()) return FALSE;
        changed = AdjustWindowRectEx(&outer, (DWORD)style, menu, (DWORD)exstyle);
        error = changed ? 0 : GetLastError();
        if (!note("resize-window", "resize-root-AdjustWindowRectEx-client-540x300", changed,
                  changed ? S_OK : winhr(error), root_id,
                  (uint64_t)(int64_t)(outer.right - outer.left),
                  (uint64_t)(int64_t)(outer.bottom - outer.top), error) || !budget()) return FALSE;
        changed = SetWindowPos(root, NULL, 0, 0, outer.right - outer.left, outer.bottom - outer.top,
                               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        error = changed ? 0 : GetLastError();
        if (!note("resize-window", "resize-root-SetWindowPos-adjusted-outer", changed,
                  changed ? S_OK : winhr(error), root_id,
                  (uint64_t)(int64_t)(outer.right - outer.left),
                  (uint64_t)(int64_t)(outer.bottom - outer.top), error) || !budget()) return FALSE;
        changed = GetClientRect(root, &client);
        error = changed ? 0 : GetLastError();
        if (!note("resize-window", "resize-root-actual-client-540x300", changed && !client.left && !client.top &&
                  client.right == 540 && client.bottom == 300, changed ? S_OK : winhr(error),
                  root_id, (uint64_t)(int64_t)client.right, (uint64_t)(int64_t)client.bottom, error)) return FALSE;
    }
    for (i = 0; i < 2; ++i) {
        if (!budget()) return FALSE;
        changed = SetWindowPos(children[i], NULL, i ? 270 : 10, 10, 220, 120,
                               SWP_NOZORDER | SWP_NOACTIVATE);
        error = changed ? 0 : GetLastError();
        if (!note("resize-window", "resized-child-slot-220x120", changed,
                  changed ? S_OK : winhr(error), child_ids[i], i ? 270 : 10, 10, error)) return FALSE;
    }
    return verify_births(); /* full window tree/owner/geometry check follows before render/report */
}

static BOOL verify_windows(BOOL children_exist)
{
    wchar_t actual_title[80];
    DWORD pid = 0, tid;
    LONG_PTR style;
    int length;
    unsigned int i;
    const LONG width = phase_now >= 2 ? 220 : 180, height = phase_now >= 2 ? 120 : 100;
    const LONG second_x = phase_now >= 2 ? 270 : 210;
    BOOL valid;
    HWND related;
    if (!verify_roots() || !budget()) return FALSE;
    valid = IsWindow(root);
    if (!note("window", "actual-root-IsWindow", valid, S_OK, root_id, 0, 0, 0)) return FALSE;
    if (!budget()) return FALSE;
    related = GetAncestor(root, GA_ROOT);
    if (!note("window", "actual-root-ancestor", related == root, S_OK,
              (uint64_t)(uintptr_t)related, root_id, 0, 0)) return FALSE;
    if (!budget()) return FALSE;
    related = GetParent(root);
    if (!note("window", "actual-root-parent", related == NULL, S_OK,
              (uint64_t)(uintptr_t)related, root_id, 0, 0)) return FALSE;
    if (!budget()) return FALSE;
    valid = IsWindowVisible(root);
    if (!note("window", "actual-root-visible", valid, S_OK, root_id, 0, 0, 0)) return FALSE;
    if (!budget()) return FALSE;
    SetLastError(0);
    style = GetWindowLongPtrW(root, GWL_STYLE);
    {
        DWORD error = GetLastError();
        LONG_PTR required = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        if (!note("window", "actual-root-overlapped-style", (style != 0 || !error) &&
                  (style & required) == required && !(style & WS_CHILD), error ? winhr(error) : S_OK,
                  (uint64_t)(uintptr_t)style, root_id, error, 0)) return FALSE;
    }
    if (!budget()) return FALSE;
    tid = GetWindowThreadProcessId(root, &pid);
    if (!note("window", "root-owner", tid != 0 && pid == (root_id == root_ids[0] ? renderer_pid : host_pid),
              tid ? S_OK : winhr(GetLastError()), pid, root_id == root_ids[0] ? renderer_pid : host_pid, tid, 0)) return FALSE;
    if (!budget()) return FALSE;
    length = GetWindowTextW(root, actual_title, (int)(sizeof(actual_title) / sizeof(actual_title[0])));
    if (!note("window", "exact-nonce-title", length == 56 && !wcscmp(actual_title, title),
              length ? S_OK : winhr(GetLastError()), (uint64_t)(unsigned int)length, 56, 0, 0)) return FALSE;
    for (i = 0; i < 2; ++i) {
        if (!budget()) return FALSE;
        valid = IsWindow(children_exist ? children[i] : window_from_id(child_ids[i]));
        if (!children_exist) {
            if (!note("window", "child-actually-absent", !valid, S_OK, child_ids[i], i, 0, 0)) return FALSE;
            continue;
        }
        if (!note("window", "actual-child-IsWindow", valid, S_OK, child_ids[i], root_id, i, 0)) return FALSE;
        if (!budget()) return FALSE;
        related = GetParent(children[i]);
        if (!note("window", "actual-child-parent", related == root, S_OK,
                  (uint64_t)(uintptr_t)related, root_id, i, 0)) return FALSE;
        if (!budget()) return FALSE;
        related = GetAncestor(children[i], GA_ROOT);
        if (!note("window", "actual-child-root-ancestor", related == root, S_OK,
                  (uint64_t)(uintptr_t)related, root_id, i, 0)) return FALSE;
        if (!budget()) return FALSE;
        valid = IsWindowVisible(children[i]);
        if (!note("window", "actual-child-visible", valid, S_OK, child_ids[i], root_id, i, 0)) return FALSE;
        if (!budget()) return FALSE;
        pid = 0; tid = GetWindowThreadProcessId(children[i], &pid);
        if (!note("window", "child-owner-is-renderer", tid != 0 && pid == renderer_pid,
                  tid ? S_OK : winhr(GetLastError()), pid, renderer_pid, tid, i)) return FALSE;
        if (!budget()) return FALSE;
        SetLastError(0);
        style = GetWindowLongPtrW(children[i], GWL_STYLE);
        {
            DWORD error = GetLastError();
            LONG_PTR required = WS_CHILD | WS_VISIBLE | WS_BORDER;
            if (!note("window", "child-style", (style != 0 || !error) &&
                      (style & required) == required, error ? winhr(error) : S_OK,
                      (uint64_t)(uintptr_t)style, i, error, 0)) return FALSE;
        }
        {
            RECT bounds = {0};
            POINT points[2];
            BOOL got;
            int offset;
            DWORD error;
            if (!budget()) return FALSE;
            got = GetWindowRect(children[i], &bounds);
            error = got ? 0 : GetLastError();
            if (!note("window", "actual-child-phase-window-size", got &&
                      bounds.right - bounds.left == width && bounds.bottom - bounds.top == height,
                      got ? S_OK : winhr(error), child_ids[i],
                      (uint64_t)(int64_t)(bounds.right - bounds.left),
                      (uint64_t)(int64_t)(bounds.bottom - bounds.top), error)) return FALSE;
            points[0].x = bounds.left; points[0].y = bounds.top;
            points[1].x = bounds.right; points[1].y = bounds.bottom;
            if (!budget()) return FALSE;
            SetLastError(0);
            offset = MapWindowPoints(NULL, root, points, 2);
            error = GetLastError();
            if (!note("window", "actual-child-phase-client-slot", (offset || !error) &&
                      points[0].x == (i ? second_x : 10) && points[0].y == 10 &&
                      points[1].x == (i ? second_x : 10) + width && points[1].y == 10 + height,
                      error ? winhr(error) : S_OK, child_ids[i],
                      (uint64_t)(int64_t)points[0].x, (uint64_t)(int64_t)points[0].y, error)) return FALSE;
            if (!budget()) return FALSE;
            got = GetClientRect(children[i], &bounds);
            error = got ? 0 : GetLastError();
            if (!note("window", "actual-bordered-child-client-size", got && !bounds.left && !bounds.top &&
                      bounds.right == width - 2 && bounds.bottom == height - 2,
                      got ? S_OK : winhr(error), child_ids[i],
                      (uint64_t)(int64_t)bounds.right, (uint64_t)(int64_t)bounds.bottom, error) || !budget()) return FALSE;
            SetLastError(0);
            actual_title[0] = 0;
            length = GetWindowTextW(children[i], actual_title, (int)(sizeof(actual_title) / sizeof(actual_title[0])));
            error = GetLastError();
            if (!note("window", "actual-child-empty-title", !length && !actual_title[0] && !error,
                      error ? winhr(error) : S_OK, child_ids[i], (uint64_t)(unsigned int)length, error, i)) return FALSE;
        }
    }
    return TRUE;
}

static capture_token64 make_token(unsigned int phase)
{
    capture_token64 token;
    memset(&token, 0, sizeof(token));
    token.magic = TOKEN_MAGIC; token.size = sizeof(token); token.version = TOKEN_VERSION;
    token.phase = phase; token.root = root_id; token.host_pid = host_pid;
    token.renderer_pid = renderer_pid; token.pattern = phase == 4 ? 0 : phase;
    token.nonce[0] = nonce_words[0]; token.nonce[1] = nonce_words[1];
    token.child_a = child_ids[0]; token.child_b = child_ids[1];
    token.width = image_width; token.height = image_height;
    return token;
}

static BOOL send_message(const pipe_message80 *message)
{
    DWORD wrote = 0, error;
    BOOL sent;
    capture_token64 expected = make_token(phase_now);
    if (!healthy() || !pump()) return FALSE;
    if (!note("token", "phase-report-token-root-pattern", !memcmp(&message->token, &expected, sizeof(expected)),
              S_OK, message->token.phase, message->token.pattern, message->token.root,
              sizeof(message->token)) || !budget()) return FALSE;
    /* Each fresh 4096-byte pipe carries only 5*80 bytes in total. A single
     * synchronous 80-byte write cannot fill it, even if its reader stalls. */
    sent = WriteFile(output_pipe, message, sizeof(*message), &wrote, NULL);
    error = sent ? 0 : GetLastError();
    if (sent && wrote == sizeof(*message)) ++pipe_sends;
    if (!sent || wrote != sizeof(*message)) io_bad = TRUE;
    if (!note("pipe", "write-one-message", sent && wrote == sizeof(*message),
              sent ? S_OK : winhr(error), wrote, message->sequence, message->flags, error)) return FALSE;
    return pump();
}

static BOOL receive_message(pipe_message80 *message, unsigned int phase)
{
    size_t offset = 0;
    capture_token64 expected = make_token(phase);
    memset(message, 0, sizeof(*message));
    while (offset < sizeof(*message)) {
        DWORD available = 0, got = 0, error, request;
        BOOL peeked, read;
        if (!healthy() || !pump()) return FALSE;
        peeked = PeekNamedPipe(input_pipe, NULL, 0, NULL, &available, NULL);
        error = peeked ? 0 : GetLastError();
        if (!peeked) {
            io_bad = TRUE;
            return note("pipe", "peek-before-read", FALSE, winhr(error), offset, error, 0, 0);
        }
        if (!budget()) return FALSE;
        if (!available) { if (!pause_pumping(5)) return FALSE; continue; }
        request = (DWORD)(sizeof(*message) - offset);
        if (request > available) request = available;
        if (!budget()) return FALSE;
        read = ReadFile(input_pipe, (BYTE *)message + offset, request, &got, NULL);
        error = read ? 0 : GetLastError();
        if (read && got <= request) offset += got; /* ledger before clock */
        if (!read || !got || got > request) {
            io_bad = TRUE;
            return note("pipe", "bounded-partial-read", FALSE, read ? E_FAIL : winhr(error),
                          got, request, offset, error);
        }
        if (!budget()) return FALSE;
    }
    ++pipe_receives;
    if (!phase && !strcmp(role, "host")) {
        const capture_token64 *t = &message->token;
        /* Root2/PIDs/births come from the host's actual owned objects, never
         * from a bootstrap claim. New USER values are only borrowed aliases. */
        expected.root = t->root; expected.child_a = t->child_a; expected.child_b = t->child_b;
        if (!note("bootstrap", "bootstrap-envelope-bound-to-actual-PI",
                  pipe_receives == 1 && !message->sequence && message->flags == BOOTSTRAP &&
                  message->hresult == S_OK && !memcmp(t, &expected, sizeof(expected)),
                  (HRESULT)message->hresult, t->host_pid, t->renderer_pid, host_pid, renderer_pid)) return FALSE;
        if (!note("bootstrap", "bootstrap-distinct-canonical-USER-handles",
                  t->root && t->root != UINT32_MAX && t->child_a && t->child_a != UINT32_MAX &&
                  t->child_b && t->child_b != UINT32_MAX &&
                  t->root != root_ids[1] && t->child_a != root_ids[1] && t->child_b != root_ids[1] &&
                  t->root != t->child_a && t->root != t->child_b && t->child_a != t->child_b,
                  S_OK, t->root, root_ids[1], t->child_a, t->child_b)) return FALSE;
        root_ids[0] = t->root; child_ids[0] = t->child_a; child_ids[1] = t->child_b;
        root = roots[0] = window_from_id(root_ids[0]); root_id = root_ids[0];
        children[0] = window_from_id(child_ids[0]); children[1] = window_from_id(child_ids[1]);
        /* No D3D objects exist yet. Independently query IsWindow, top-level
         * roots, owner PIDs, child parents/ancestors and actual process births
         * before permitting the exact echo that unlocks renderer D3D setup. */
        if (!verify_births() || !verify_windows(TRUE) ||
            !record("bootstrap", "bootstrap-actual-owner-tree-verified", TRUE, S_OK,
                    root_ids[0], root_ids[1], renderer_pid, host_pid)) return FALSE;
        expected = make_token(0);
    }
    return note("pipe", "validate-complete-message", pipe_receives == phase + 1 &&
                message->sequence == phase && !memcmp(&message->token, &expected, sizeof(expected)) &&
                message->flags == (!phase ? BOOTSTRAP : phase == 4 ? LOCAL_COM_RELEASED : FRAME_FLAGS) &&
                (!phase ? message->hresult == S_OK : SUCCEEDED((HRESULT)message->hresult)), (HRESULT)message->hresult,
                message->sequence, message->flags, offset, pipe_receives);
}

static BOOL pipe_empty(unsigned int sent_count)
{
    DWORD available = 0, error;
    BOOL peeked;
    if (!healthy() || !pump()) return FALSE;
    peeked = PeekNamedPipe(input_pipe, NULL, 0, NULL, &available, NULL);
    error = peeked ? 0 : GetLastError();
    if (!peeked) io_bad = TRUE;
    return note("pipe", "exactly-five-messages", peeked && !available &&
                pipe_sends == sent_count && pipe_receives == 5, peeked ? S_OK : winhr(error),
                available, pipe_sends, pipe_receives, error);
}

static BOOL gate_deadline(uint64_t start)
{
    if (!budget()) return FALSE;
    if (elapsed_ms - start < GATE_MS) return TRUE;
    timeout_bad = TRUE;
    return record("timeout", "ready-plus-gate-45s", FALSE, HRESULT_FROM_WIN32(ERROR_TIMEOUT),
                  elapsed_ms - start, GATE_MS, phase_now, 0);
}

static BOOL gate_path(wchar_t *path, size_t count, const wchar_t *kind, unsigned int phase)
{
    wchar_t wide_nonce[33];
    size_t i;
    int n;
    for (i = 0; i < 33; ++i) wide_nonce[i] = (wchar_t)nonce_text[i];
    n = swprintf(path, count, L"C:\\CiderTests\\consumer-%ls-%ls-%u.bin", kind, wide_nonce, phase);
    return record("file", "bounded-phase-path", n > 0 && (size_t)n < count,
                  S_OK, phase, n > 0 ? (uint64_t)n : 0, 0, 0);
}

static BOOL ready_and_gate(unsigned int phase)
{
    wchar_t path[160];
    capture_token64 token = make_token(phase), actual;
    uint64_t start;
    BOOL ok = FALSE, result;
    DWORD wrote = 0, error;
    if (!healthy() || !pump()) return FALSE;
    start = elapsed_ms; /* includes gate preflight and ready CREATE/Write/Flush/Close */
    if (!gate_path(path, sizeof(path) / sizeof(path[0]), L"gate", phase)) return FALSE;
    {
        DWORD attributes = GetFileAttributesW(path);
        DWORD missing_error = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_FILE_EXISTS;
        if (!note("file", "gate-absent-before-ready", attributes == INVALID_FILE_ATTRIBUTES &&
                  missing_error == ERROR_FILE_NOT_FOUND, HRESULT_FROM_WIN32(missing_error),
                  attributes, missing_error, phase, 0)) return FALSE;
    }
    if (!gate_path(path, sizeof(path) / sizeof(path[0]), L"ready", phase) ||
        !gate_deadline(start)) return FALSE;
    phase_file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    error = phase_file == INVALID_HANDLE_VALUE ? GetLastError() : 0;
    if (phase_file == INVALID_HANDLE_VALUE) phase_file = NULL;
    if (!note("file", "ready-CREATE_NEW", phase_file != NULL, phase_file ? S_OK : winhr(error),
              (uint64_t)(uintptr_t)phase_file, error, sizeof(token), phase)) goto done;
    if (!gate_deadline(start)) goto done;
    result = WriteFile(phase_file, &token, sizeof(token), &wrote, NULL);
    error = result ? 0 : GetLastError();
    ok = result && wrote == sizeof(token);
    if (!ok) io_bad = TRUE;
    if (!note("file", "ready-write-token64", ok, result ? S_OK : winhr(error),
              wrote, sizeof(token), error, phase)) ok = FALSE;
    (void)budget();
    result = FlushFileBuffers(phase_file);
    error = result ? 0 : GetLastError();
    if (!result) io_bad = TRUE;
    if (!note("file", "ready-flush", result, result ? S_OK : winhr(error),
              result, error, phase, 0)) ok = FALSE;
    if (!close_owned(&phase_file, "ready-close")) ok = FALSE;
    if (!ok || !gate_deadline(start)) goto done;
    if (!record("ready", phase < 4 ? "screen-ready" : "native-drain-ready", TRUE, S_OK,
                phase, token.pattern, token.root, sizeof(token))) goto done;
    if (!gate_path(path, sizeof(path) / sizeof(path[0]), L"gate", phase)) goto done;
    for (;;) {
        BY_HANDLE_FILE_INFORMATION info;
        LARGE_INTEGER size;
        DWORD got = 0, eof_bytes = 0;
        BYTE extra = 0;
        if (!healthy() || !pump() || !gate_deadline(start) || !probe_peer_alive()) goto done;
        if (!gate_deadline(start)) goto done;
        phase_file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        error = phase_file == INVALID_HANDLE_VALUE ? GetLastError() : 0;
        if (phase_file == INVALID_HANDLE_VALUE) phase_file = NULL;
        if (!budget() || !gate_deadline(start)) goto done;
        if (!phase_file) {
            if (error != ERROR_FILE_NOT_FOUND) {
                io_bad = TRUE;
                record("file", "gate-open", FALSE, winhr(error), error, phase, 0, 0);
                goto done;
            }
            if (!pause_pumping(5)) goto done;
            continue;
        }
        result = GetFileInformationByHandle(phase_file, &info);
        error = result ? 0 : GetLastError();
        if (!result) io_bad = TRUE;
        if (!note("file", "gate-not-reparse-or-directory", result &&
                  !(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)),
                  result ? S_OK : winhr(error), result ? info.dwFileAttributes : 0, error, phase, 0)) goto done;
        if (!gate_deadline(start)) goto done;
        result = GetFileSizeEx(phase_file, &size);
        error = result ? 0 : GetLastError();
        if (!result) io_bad = TRUE;
        if (!note("file", "gate-exact-size64", result && size.QuadPart == (LONGLONG)sizeof(token),
                  result ? S_OK : winhr(error), result ? (uint64_t)size.QuadPart : 0,
                  sizeof(token), error, phase)) goto done;
        if (!gate_deadline(start)) goto done;
        result = ReadFile(phase_file, &actual, sizeof(actual), &got, NULL);
        error = result ? 0 : GetLastError();
        if (!result || got != sizeof(actual)) io_bad = TRUE;
        if (!note("file", "gate-read-exact-token", result && got == sizeof(actual) &&
                  !memcmp(&actual, &token, sizeof(token)), result ? S_OK : winhr(error),
                  got, sizeof(actual), error, phase)) goto done;
        if (!gate_deadline(start)) goto done;
        result = ReadFile(phase_file, &extra, 1, &eof_bytes, NULL);
        error = result ? 0 : GetLastError();
        if (!result) io_bad = TRUE;
        if (!note("file", "gate-actual-EOF", result && eof_bytes == 0,
                  result ? S_OK : winhr(error), eof_bytes, error, phase, 0)) goto done;
        ok = close_owned(&phase_file, "gate-close");
        if (!ok || !gate_deadline(start)) goto done;
        return record("gate", "parent-token-accepted", TRUE, S_OK, phase, token.pattern,
                      elapsed_ms - start, sizeof(token));
    }
done:
    if (phase_file) (void)close_owned(&phase_file, "phase-file-failure-close");
    return FALSE;
}

static BOOL gpu_complete(gpu_state *gpu, HRESULT *actual_hr)
{
    uint64_t start;
    unsigned int polls = 0;
    HRESULT hr = E_PENDING;
    BOOL complete = FALSE;
    if (!budget()) return FALSE;
    start = elapsed_ms;
    ID3D11DeviceContext_End(gpu->context, (ID3D11Asynchronous *)gpu->query);
    if (!note("gpu", "event-End", TRUE, S_OK, start, 0, 0, 0)) return FALSE;
    if (elapsed_ms - start >= GPU_MS) {
        timeout_bad = TRUE;
        return record("gpu", "event-End-late", FALSE, hr, elapsed_ms - start, GPU_MS, 0, 0);
    }
    if (!budget()) return FALSE;
    ID3D11DeviceContext_Flush(gpu->context);
    if (!note("gpu", "event-Flush", TRUE, S_OK, start, 0, 0, 0)) return FALSE;
    for (;;) {
        if (!pump() || !budget()) return FALSE;
        if (elapsed_ms - start >= GPU_MS) {
            timeout_bad = TRUE;
            return record("gpu", "event-five-second-limit", FALSE, hr,
                          polls, complete, elapsed_ms - start, GPU_MS);
        }
        hr = ID3D11DeviceContext_GetData(gpu->context, (ID3D11Asynchronous *)gpu->query,
                                        &complete, sizeof(complete), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        ++polls;
        *actual_hr = hr;
        if (!budget())
            return record("gpu", "GetData-post-budget", FALSE, hr, polls, complete, elapsed_ms - start, GPU_MS);
        if (elapsed_ms - start >= GPU_MS) {
            timeout_bad = TRUE;
            return record("gpu", "event-late-return", FALSE, hr, polls, complete,
                          elapsed_ms - start, GPU_MS);
        }
        if (hr == S_OK)
            return record("gpu", "actual-event-completion", complete, hr, polls, complete,
                          elapsed_ms - start, 0);
        if (hr != S_FALSE)
            return record("gpu", "event-GetData-error", FALSE, hr, polls, complete, 0, 0);
        if (!pause_pumping(1)) return FALSE;
    }
}

/* All descriptors come from the retained actual interfaces. Requests are only
 * acceptance constraints; GetDesc supplies dimensions for the CPU read bounds. */
static BOOL describe_swap(gpu_state *gpu, unsigned int i, const char *label, UINT width, UINT height)
{
    target *t = &gpu->targets[i];
    DXGI_SWAP_CHAIN_DESC actual;
    HRESULT hr;
    memset(&actual, 0, sizeof(actual));
    if (!budget()) return FALSE;
    hr = IDXGISwapChain_GetDesc(t->swap, &actual);
    return note("gpu", label, SUCCEEDED(hr) && actual.OutputWindow == children[i] &&
                actual.BufferCount == 2 && actual.BufferDesc.Width == width && actual.BufferDesc.Height == height &&
                actual.BufferDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM &&
                actual.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD && actual.Windowed &&
                actual.SampleDesc.Count == 1 && actual.SampleDesc.Quality == 0 &&
                actual.BufferUsage == DXGI_USAGE_RENDER_TARGET_OUTPUT && !actual.Flags,
                hr, i, actual.BufferDesc.Width, actual.BufferDesc.Height, (uint64_t)(uintptr_t)t->swap);
}

static BOOL acquire_target(gpu_state *gpu, unsigned int i)
{
    target *t = &gpu->targets[i];
    D3D11_TEXTURE2D_DESC texture;
    HRESULT hr;
    if (!budget()) return FALSE;
    hr = IDXGISwapChain_GetBuffer(t->swap, 0, &IID_ID3D11Texture2D, (void **)&t->back);
    if (!note("gpu", "actual-backbuffer-zero", SUCCEEDED(hr) && t->back, hr, i, 0, 0, 0)) return FALSE;
    if (!budget()) return FALSE;
    ID3D11Texture2D_GetDesc(t->back, &texture);
    t->width = texture.Width; t->height = texture.Height;
    if (!note("gpu", "actual-BGRA-backbuffer-layout", texture.Width == image_width &&
          texture.Height == image_height && texture.Format == DXGI_FORMAT_B8G8R8A8_UNORM &&
          texture.MipLevels == 1 && texture.ArraySize == 1 && texture.SampleDesc.Count == 1 &&
          texture.SampleDesc.Quality == 0 && texture.Usage == D3D11_USAGE_DEFAULT &&
          (texture.BindFlags & D3D11_BIND_RENDER_TARGET) != 0,
          S_OK, texture.Width, texture.Height, texture.Format, i)) return FALSE;
    if (!budget()) return FALSE;
    hr = ID3D11Device_CreateRenderTargetView(gpu->device, (ID3D11Resource *)t->back, NULL, &t->rtv);
    if (!note("gpu", "actual-RTV", SUCCEEDED(hr) && t->rtv, hr, i, 0, 0, 0)) return FALSE;
    texture.Usage = D3D11_USAGE_STAGING; texture.BindFlags = 0;
    texture.CPUAccessFlags = D3D11_CPU_ACCESS_READ; texture.MiscFlags = 0;
    if (!budget()) return FALSE;
    hr = ID3D11Device_CreateTexture2D(gpu->device, &texture, NULL, &t->staging);
    if (!note("gpu", "actual-CPU-read-staging", SUCCEEDED(hr) && t->staging, hr, i, 0, 0, 0)) return FALSE;
    if (!budget()) return FALSE;
    ID3D11Texture2D_GetDesc(t->staging, &texture);
    if (!note("gpu", "staging-layout", texture.Width == image_width && texture.Height == image_height &&
          texture.Format == DXGI_FORMAT_B8G8R8A8_UNORM && texture.MipLevels == 1 &&
          texture.ArraySize == 1 && texture.SampleDesc.Count == 1 && texture.SampleDesc.Quality == 0 &&
          texture.Usage == D3D11_USAGE_STAGING && texture.CPUAccessFlags == D3D11_CPU_ACCESS_READ &&
          !texture.BindFlags && !texture.MiscFlags, S_OK, i, texture.Usage,
          texture.CPUAccessFlags, texture.Format)) return FALSE;
    return TRUE;
}

static BOOL gpu_identity(gpu_state *gpu, const char *label)
{
    return note("gpu", label, gpu->device && gpu->context && gpu->query &&
                (uintptr_t)gpu->device == gpu->device_identity &&
                (uintptr_t)gpu->context == gpu->context_identity &&
                (uintptr_t)gpu->query == gpu->query_identity &&
                (uintptr_t)gpu->targets[0].swap == gpu->swap_identity[0] &&
                (uintptr_t)gpu->targets[1].swap == gpu->swap_identity[1] &&
                gpu->targets[0].swap && gpu->targets[1].swap, S_OK,
                (uint64_t)(uintptr_t)gpu->device, (uint64_t)(uintptr_t)gpu->context,
                (uint64_t)(uintptr_t)gpu->targets[0].swap, (uint64_t)(uintptr_t)gpu->targets[1].swap) &&
           note("gpu", "retained-event-query-identity", (uintptr_t)gpu->query == gpu->query_identity,
                S_OK, (uint64_t)(uintptr_t)gpu->query, gpu->query_identity, 0, 0);
}

static BOOL gpu_create(gpu_state *gpu)
{
    D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_11_0, obtained = (D3D_FEATURE_LEVEL)0;
    D3D11_QUERY_DESC query_desc;
    HRESULT hr;
    unsigned int i;
    if (!healthy() || !pump()) return FALSE;
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                           &requested, 1, D3D11_SDK_VERSION, &gpu->device, &obtained, &gpu->context);
    if (!note("gpu", "D3D11CreateDevice-hardware-one-device", SUCCEEDED(hr) && gpu->device &&
              gpu->context && obtained == requested, hr, obtained, requested,
              (uint64_t)(uintptr_t)gpu->device, (uint64_t)(uintptr_t)gpu->context)) return FALSE;
    if (!budget()) return FALSE;
    hr = ID3D11Device_QueryInterface(gpu->device, &IID_IDXGIDevice, (void **)&gpu->dxgi);
    if (!note("gpu", "device-QI-IDXGIDevice", SUCCEEDED(hr) && gpu->dxgi, hr, 0, 0, 0, 0)) return FALSE;
    if (!budget()) return FALSE;
    hr = IDXGIDevice_GetAdapter(gpu->dxgi, &gpu->adapter);
    if (!note("gpu", "device-adapter", SUCCEEDED(hr) && gpu->adapter, hr, 0, 0, 0, 0)) return FALSE;
    if (!budget()) return FALSE;
    hr = IDXGIAdapter_GetParent(gpu->adapter, &IID_IDXGIFactory, (void **)&gpu->factory);
    if (!note("gpu", "adapter-factory", SUCCEEDED(hr) && gpu->factory, hr, 0, 0, 0, 0)) return FALSE;
    memset(&query_desc, 0, sizeof(query_desc)); query_desc.Query = D3D11_QUERY_EVENT;
    if (!budget()) return FALSE;
    hr = ID3D11Device_CreateQuery(gpu->device, &query_desc, &gpu->query);
    if (!note("gpu", "completion-query", SUCCEEDED(hr) && gpu->query, hr, 0, 0, 0, 0)) return FALSE;
    for (i = 0; i < 2; ++i) {
        DXGI_SWAP_CHAIN_DESC desc;
        target *t = &gpu->targets[i];
        memset(&desc, 0, sizeof(desc));
        desc.BufferDesc.Width = image_width; desc.BufferDesc.Height = image_height;
        desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.BufferCount = 2; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = children[i]; desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE; desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        if (!healthy() || !verify_windows(TRUE) || !verify_births() || !budget()) return FALSE;
        hr = IDXGIFactory_CreateSwapChain(gpu->factory, (IUnknown *)gpu->device, &desc, &t->swap);
        if (SUCCEEDED(hr) && t->swap) ++swaps_done;
        if (!note("gpu", "CreateSwapChain-local-child", SUCCEEDED(hr) && t->swap, hr,
                  i, child_ids[i], renderer_pid, self_pid)) return FALSE;
        if (!describe_swap(gpu, i, "actual-swap-description", image_width, image_height) ||
            !acquire_target(gpu, i)) return FALSE;
    }
    gpu->device_identity = (uintptr_t)gpu->device;
    gpu->context_identity = (uintptr_t)gpu->context;
    gpu->query_identity = (uintptr_t)gpu->query;
    for (i = 0; i < 2; ++i) gpu->swap_identity[i] = (uintptr_t)gpu->targets[i].swap;
    return TRUE;
}

static BOOL readable_mapping(const D3D11_MAPPED_SUBRESOURCE *mapping, UINT width, UINT height)
{
    uintptr_t at, end;
    size_t span;
    if (!width || !height || !mapping->pData || mapping->RowPitch < width * 4 || mapping->RowPitch > 1024 * 1024)
        return record("gpu", "map-pointer-and-rowpitch", FALSE, E_FAIL,
                      (uint64_t)(uintptr_t)mapping->pData, mapping->RowPitch, 0, 0);
    span = (size_t)(height - 1) * mapping->RowPitch + (size_t)width * 4;
    at = (uintptr_t)mapping->pData;
    if (at > UINTPTR_MAX - span)
        return record("gpu", "map-range-overflow", FALSE, E_FAIL, at, span, 0, 0);
    end = at + span;
    /* Map(READ) owns a CPU-accessible resource until Unmap. Host Metal memory
     * may not be represented in Wine's VirtualQuery view registry. That separate
     * metadata limitation is not a precondition of the D3D11 data contract; the
     * actual bounded pixel loads below remain the readback proof. */
    return record("gpu", "map-bounded-CPU-range", end > at, S_OK,
                  span, mapping->RowPitch, at, end);
}

static uint32_t crc_pixel(uint32_t crc, uint32_t pixel)
{
    unsigned int byte, bit;
    for (byte = 0; byte < 4; ++byte) {
        crc ^= (pixel >> (8 * byte)) & 255;
        for (bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xedb88320) : 0);
    }
    return crc;
}

static BOOL render_frame(gpu_state *gpu, unsigned int index, unsigned int phase, HRESULT *present_hr)
{
    target *t = &gpu->targets[index];
    BOOL red = phase == 3 || ((phase != 2) == (index == 0)), pixels_ok;
    const float red_color[4] = {1, 0, 0, 1}, blue_color[4] = {0, 0, 1, 1};
    uint32_t expected = red ? UINT32_C(0xffff0000) : UINT32_C(0xff0000ff);
    unsigned int x, y, tested = 0, mismatches = 0;
    uint32_t crc = UINT32_MAX, expected_crc = UINT32_MAX;
    D3D11_TEXTURE2D_DESC back_desc, staging_desc;
    HRESULT hr, completion_hr = E_PENDING;
    if (!healthy() || !pump() || !budget()) return FALSE;
    ID3D11Texture2D_GetDesc(t->back, &back_desc);
    t->width = back_desc.Width; t->height = back_desc.Height;
    if (!note("gpu", "frame-backbuffer-actual-dimensions", t->width == image_width && t->height == image_height &&
              back_desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM && back_desc.MipLevels == 1 &&
              back_desc.ArraySize == 1 && back_desc.SampleDesc.Count == 1 && !back_desc.SampleDesc.Quality &&
              back_desc.Usage == D3D11_USAGE_DEFAULT && (back_desc.BindFlags & D3D11_BIND_RENDER_TARGET),
              S_OK, index, t->width, t->height, back_desc.Format) || !budget()) return FALSE;
    ID3D11Texture2D_GetDesc(t->staging, &staging_desc);
    if (!note("gpu", "frame-staging-actual-dimensions", staging_desc.Width == t->width &&
              staging_desc.Height == t->height && staging_desc.Format == back_desc.Format &&
              staging_desc.MipLevels == 1 && staging_desc.ArraySize == 1 && staging_desc.SampleDesc.Count == 1 &&
              !staging_desc.SampleDesc.Quality && staging_desc.Usage == D3D11_USAGE_STAGING &&
              staging_desc.CPUAccessFlags == D3D11_CPU_ACCESS_READ && !staging_desc.BindFlags && !staging_desc.MiscFlags,
              S_OK, index, staging_desc.Width, staging_desc.Height, staging_desc.Format) || !budget()) return FALSE;
    ID3D11DeviceContext_ClearRenderTargetView(gpu->context, t->rtv, red ? red_color : blue_color);
    ++frames_done;
    if (!note("gpu", "actual-clear-RTV", TRUE, S_OK, index, expected, phase, frames_done)) return FALSE;
    if (!budget()) return FALSE;
    ID3D11DeviceContext_CopyResource(gpu->context, (ID3D11Resource *)t->staging, (ID3D11Resource *)t->back);
    if (!note("gpu", "actual-copy-backbuffer-to-staging", TRUE, S_OK, index, phase, 0, 0)) return FALSE;
    if (!gpu_complete(gpu, &completion_hr) || !budget()) return FALSE;
    memset(&t->mapping, 0, sizeof(t->mapping));
    {
        uint64_t map_limit = elapsed_ms + GPU_MS;
        unsigned int attempts = 0;
        do {
            if (!budget()) return FALSE;
            hr = ID3D11DeviceContext_Map(gpu->context, (ID3D11Resource *)t->staging, 0,
                                        D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &t->mapping);
            if (SUCCEEDED(hr)) t->mapped = TRUE; /* MUST precede post-clock */
            ++attempts;
            if (!budget()) return FALSE;
            if (elapsed_ms >= map_limit)
                return record("gpu", "Map-READ-late-result", FALSE, hr,
                              index, attempts, map_limit, elapsed_ms);
            if (hr != DXGI_ERROR_WAS_STILL_DRAWING) break;
            if (!note("gpu", "Map-READ-allowed-pending", TRUE, hr,
                      index, attempts, map_limit, elapsed_ms)) return FALSE;
            if (elapsed_ms >= map_limit || attempts >= 10000)
                return record("gpu", "Map-READ-readiness-deadline", FALSE, hr,
                              index, attempts, map_limit, elapsed_ms);
            if (!pump() || !budget()) return FALSE;
            Sleep(1); /* yield only; the next actual Map grants readiness */
        } while (TRUE);
    }
    if (!note("gpu", "actual-Map-READ", SUCCEEDED(hr), hr, index, t->mapping.RowPitch,
              (uint64_t)(uintptr_t)t->mapping.pData, t->mapped)) return FALSE;
    if (!readable_mapping(&t->mapping, t->width, t->height)) return FALSE; /* cleanup owns Unmap */
    for (y = 0; y < t->height; ++y) {
        const BYTE *row = (const BYTE *)t->mapping.pData + (size_t)y * t->mapping.RowPitch;
        for (x = 0; x < t->width; ++x) {
            uint32_t actual;
            memcpy(&actual, row + 4 * x, sizeof(actual));
            if (actual != expected) ++mismatches;
            crc = crc_pixel(crc, actual); expected_crc = crc_pixel(expected_crc, expected);
            ++tested; ++pixels_read;
        }
    }
    crc ^= UINT32_MAX; expected_crc ^= UINT32_MAX;
    pixels_ok = tested == t->width * t->height && !mismatches && crc == expected_crc;
    if (pixels_ok) ++readbacks_done;
    /* Always unmap this successful Map, even after a failed/late observation. */
    (void)budget();
    ID3D11DeviceContext_Unmap(gpu->context, (ID3D11Resource *)t->staging, 0);
    t->mapped = FALSE;
    if (!note("gpu", index ? "readback-B-all-expected-pixels-and-Unmap" : "readback-A-all-expected-pixels-and-Unmap",
              pixels_ok, hr, expected, tested, mismatches, t->mapping.RowPitch) ||
        !note("gpu", "readback-pixel-CRC32", crc == expected_crc, S_OK,
              index, crc, t->width, t->height)) return FALSE;
    if (!healthy() || !budget()) return FALSE;
    hr = IDXGISwapChain_Present(t->swap, 0, 0);
    *present_hr = hr;
    if (SUCCEEDED(hr)) ++presents_done;
    if (hr == DXGI_STATUS_OCCLUDED) ++occluded_done;
    if (!note("gpu", "actual-Present-HRESULT", SUCCEEDED(hr), hr, index, phase,
              hr == DXGI_STATUS_OCCLUDED, presents_done)) return FALSE;
    return pump();
}

#define DROP_COM(pointer, interface_name, label) do { \
    if ((pointer) != NULL) { \
        ULONG remaining; \
        (void)budget(); \
        remaining = interface_name##_Release(pointer); \
        (pointer) = NULL; \
        ++releases_done; \
        (void)note("com-release", label, TRUE, S_OK, remaining, releases_done, 0, 0); \
    } \
} while (0)

/* Resize returns only the six owned old target references. The remaining
 * device/context/query/swapchain owners stay in their original slots throughout.
 * A late/failed observation never erases a successfully acquired/released owner. */
#define DROP_RESIZE_COM(pointer, interface_name, label, index) do { \
    if ((pointer) != NULL) { \
        ULONG remaining; \
        uintptr_t old = (uintptr_t)(pointer); \
        (void)budget(); \
        remaining = interface_name##_Release(pointer); \
        (pointer) = NULL; \
        ++releases_done; ++resize_releases; \
        (void)note("com-release", label, TRUE, S_OK, remaining, releases_done, index, old); \
    } \
} while (0)

static BOOL resize_gpu(gpu_state *gpu)
{
    unsigned int i;
    HRESULT hr = E_PENDING;
    if (!healthy() || !gpu_identity(gpu, "resize-retained-interface-identities-before") ||
        !note("resize-gpu", "resize-old-targets-not-mapped", !gpu->targets[0].mapped && !gpu->targets[1].mapped &&
              !resize_calls && !resize_releases && !releases_done, S_OK,
              gpu->targets[0].mapped, gpu->targets[1].mapped, resize_calls, releases_done) || !budget()) return FALSE;
    ID3D11DeviceContext_ClearState(gpu->context); /* release any context-held old RTV references */
    if (!note("resize-gpu", "resize-unbind-ClearState", TRUE, S_OK, 0, 0, 0, 0) ||
        !gpu_complete(gpu, &hr) ||
        !note("resize-gpu", "resize-actual-completion-before-old-releases", hr == S_OK, hr,
              (uint64_t)(uintptr_t)gpu->query, resize_calls, releases_done, 0)) return FALSE;
    for (i = 0; i < 2; ++i) {
        if (!describe_swap(gpu, i, "resize-before-swap-description", INITIAL_WIDTH, INITIAL_HEIGHT)) return FALSE;
    }
    for (i = 0; i < 2; ++i) {
        target *t = &gpu->targets[i];
        DROP_RESIZE_COM(t->rtv, ID3D11RenderTargetView, "resize-old-RTV-Release", i);
        DROP_RESIZE_COM(t->back, ID3D11Texture2D, "resize-old-backbuffer-Release", i);
        DROP_RESIZE_COM(t->staging, ID3D11Texture2D, "resize-old-staging-Release", i);
    }
    if (!healthy() || !note("resize-gpu", "resize-all-six-old-target-owners-returned",
        resize_releases == 6 && releases_done == 6 &&
        !gpu->targets[0].rtv && !gpu->targets[0].back && !gpu->targets[0].staging &&
        !gpu->targets[1].rtv && !gpu->targets[1].back && !gpu->targets[1].staging,
        S_OK, resize_releases, releases_done, 0, 0)) return FALSE;
    for (i = 0; i < 2; ++i) {
        target *t = &gpu->targets[i];
        uintptr_t before = (uintptr_t)t->swap;
        if (!budget()) return FALSE;
        hr = IDXGISwapChain_ResizeBuffers(t->swap, 2, RESIZED_WIDTH, RESIZED_HEIGHT,
                                         DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        ++resize_calls; /* actual call, including a failed or late result */
        /* GetDesc is observed even on an HRESULT failure; its outcome is not
         * a replacement for ResizeBuffers success. No call is ever retried. */
        {
            BOOL resized = note("resize-gpu", "actual-ResizeBuffers-once-per-swapchain",
                SUCCEEDED(hr) && before == gpu->swap_identity[i] && (uintptr_t)t->swap == before,
                hr, i, before, (uint64_t)(uintptr_t)t->swap, resize_calls);
            BOOL described = describe_swap(gpu, i, "resize-after-swap-description", RESIZED_WIDTH, RESIZED_HEIGHT);
            if (!resized || !described || !acquire_target(gpu, i)) return FALSE;
        }
    }
    return gpu_identity(gpu, "resize-retained-interface-identities-after");
}

static BOOL release_gpu(gpu_state *gpu, HRESULT *drain_hr)
{
    unsigned int i;
    BOOL drained = FALSE;
    if (gpu->context) {
        for (i = 0; i < 2; ++i) {
            target *t = &gpu->targets[i];
            if (t->mapped) {
                (void)budget();
                ID3D11DeviceContext_Unmap(gpu->context, (ID3D11Resource *)t->staging, 0);
                t->mapped = FALSE;
                (void)note("gpu", "cleanup-owned-Unmap", TRUE, S_OK, i, 0, 0, 0);
            }
        }
        (void)budget();
        ID3D11DeviceContext_ClearState(gpu->context);
        (void)note("gpu", "release-ClearState", TRUE, S_OK, 0, 0, 0, 0);
        (void)budget();
        ID3D11DeviceContext_Flush(gpu->context);
        (void)note("gpu", "release-Flush", TRUE, S_OK, 0, 0, 0, 0);
        if (gpu->query) drained = gpu_complete(gpu, drain_hr);
        if (drained) gpu_drained = TRUE;
    }
    for (i = 0; i < 2; ++i) {
        target *t = &gpu->targets[i];
        DROP_COM(t->rtv, ID3D11RenderTargetView, "RTV-Release");
        DROP_COM(t->back, ID3D11Texture2D, "backbuffer-Release");
        DROP_COM(t->staging, ID3D11Texture2D, "staging-Release");
        DROP_COM(t->swap, IDXGISwapChain, "swapchain-Release");
    }
    if (gpu->context) {
        (void)budget();
        ID3D11DeviceContext_Flush(gpu->context);
        (void)note("gpu", "post-resource-release-Flush", TRUE, S_OK, 0, 0, 0, 0);
    }
    DROP_COM(gpu->query, ID3D11Query, "event-query-Release");
    DROP_COM(gpu->factory, IDXGIFactory, "factory-Release");
    DROP_COM(gpu->adapter, IDXGIAdapter, "adapter-Release");
    DROP_COM(gpu->dxgi, IDXGIDevice, "DXGI-device-Release");
    DROP_COM(gpu->context, ID3D11DeviceContext, "context-Release");
    DROP_COM(gpu->device, ID3D11Device, "device-Release");
    local_released = !gpu->device && !gpu->context && !gpu->dxgi && !gpu->adapter &&
        !gpu->factory && !gpu->query;
    for (i = 0; i < 2; ++i) {
        target *t = &gpu->targets[i];
        if (t->swap || t->back || t->staging || t->rtv || t->mapped) local_released = FALSE;
    }
    (void)note("com-release", "all-local-pointers-released-only", local_released, *drain_hr,
               releases_done, drained, local_released, 0);
    return drained && local_released && !failures && !io_bad;
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp)
{
    return DefWindowProcW(window, message, wp, lp);
}

static BOOL destroy_owned(HWND *window, const char *label)
{
    HWND old = *window;
    BOOL destroyed, owned;
    DWORD error, pid = 0, tid;
    if (!old) return TRUE;
    (void)budget();
    tid = GetWindowThreadProcessId(old, &pid);
    error = tid ? 0 : GetLastError();
    owned = tid && pid == self_pid && tid == GetCurrentThreadId();
    (void)note("window", "destroy-slot-actual-self-owner", owned, tid ? S_OK : winhr(error),
               (uint64_t)(uintptr_t)old, pid, self_pid, tid);
    /* A sticky late/failed clock rejects acceptance, but must not suppress a
     * real owner's partial-setup cleanup. Never destroy a mismatched owner. */
    if (!owned) return FALSE;
    (void)budget();
    destroyed = DestroyWindow(old);
    error = destroyed ? 0 : GetLastError();
    if (destroyed) *window = NULL;
    return note("window", label, destroyed, destroyed ? S_OK : winhr(error),
                (uint64_t)(uintptr_t)old, destroyed, error, 0);
}

static BOOL renderer_main(char **argv)
{
    static const wchar_t class_name[] = L"CiderRendererResizeV1";
    gpu_state gpu;
    WNDCLASSEXW wc;
    HINSTANCE instance = NULL; /* borrowed, never FreeLibrary */
    uint32_t read_id = 0, write_id = 0;
    unsigned int phase, i;
    BOOL complete = FALSE, registered = FALSE, result;
    DWORD error, tid, pid = 0;
    HRESULT hr = E_PENDING;
    memset(&gpu, 0, sizeof(gpu)); memset(&wc, 0, sizeof(wc));
    renderer_pid = self_pid;
    {
        BOOL read_parsed = parse_id(argv[4], &read_id);
        BOOL write_parsed = parse_id(argv[5], &write_id);
        /* Retain each valid inherited owner independently, including a
         * partially malformed command. An alias is never double-owned. */
        if (read_parsed) input_pipe = (HANDLE)(uintptr_t)read_id;
        if (write_parsed && write_id != read_id) output_pipe = (HANDLE)(uintptr_t)write_id;
        result = parse_id(argv[2], &root_ids[1]);
        if (!record("arguments", "renderer-canonical-handles", result && read_parsed && write_parsed &&
                    read_id != write_id, S_OK, root_ids[1], read_id, write_id, 0)) goto done;
    }
    roots[1] = window_from_id(root_ids[1]); /* borrowed foreign root, never destroyed here */
    if (!budget()) goto done;
    {
        DWORD kind = GetFileType(input_pipe);
        if (!note("pipe", "renderer-inherited-read-is-pipe", kind == FILE_TYPE_PIPE,
                  kind == FILE_TYPE_UNKNOWN ? winhr(GetLastError()) : S_OK, kind, read_id, 0, 0)) goto done;
        if (!budget()) goto done;
        kind = GetFileType(output_pipe);
        if (!note("pipe", "renderer-inherited-write-is-pipe", kind == FILE_TYPE_PIPE,
                  kind == FILE_TYPE_UNKNOWN ? winhr(GetLastError()) : S_OK, kind, write_id, 0, 0)) goto done;
    }
    if (!budget()) goto done;
    result = IsWindow(roots[1]);
    if (!note("identity", "renderer-Root2-actually-IsWindow", result, S_OK, root_ids[1], 0, 0, 0) ||
        !budget()) goto done;
    tid = GetWindowThreadProcessId(roots[1], &pid);
    host_pid = pid;
    if (!note("identity", "renderer-discovered-actual-host", tid && host_pid && host_pid != self_pid,
              tid ? S_OK : winhr(GetLastError()), host_pid, self_pid, tid, root_ids[1])) goto done;
    if (!budget()) goto done;
    peer_process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, host_pid);
    error = peer_process ? 0 : GetLastError();
    if (!note("identity", "OpenProcess-actual-host", peer_process != NULL,
              peer_process ? S_OK : winhr(error), host_pid, error, 0, 0)) goto done;
    if (!process_birth(GetCurrentProcess(), &self_birth, "renderer-birth") ||
        !process_birth(peer_process, &peer_birth, "host-birth") ||
        !record("identity", "renderer-born-after-host", self_birth >= peer_birth, S_OK,
                self_birth, peer_birth, 0, 0) || !opposite_process(peer_process) || !verify_births()) goto done;
    if (!budget()) goto done;
    instance = GetModuleHandleW(NULL);
    error = instance ? 0 : GetLastError();
    if (!note("window", "borrowed-executable-module", instance != NULL,
              instance ? S_OK : winhr(error), (uint64_t)(uintptr_t)instance, error, 0, 0)) goto done;
    wc.cbSize = sizeof(wc); wc.lpfnWndProc = window_proc; wc.hInstance = instance;
    wc.lpszClassName = class_name;
    /* Borrowed stock brush: define fixture background in Win32, not by theme. */
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    if (!note("window", "class-borrowed-white-background", wc.hbrBackground != NULL,
              wc.hbrBackground ? S_OK : E_FAIL, (uint64_t)(uintptr_t)wc.hbrBackground, WHITE_BRUSH, 0, 0)) goto done;
    if (!budget()) goto done;
    registered = RegisterClassExW(&wc) != 0;
    error = registered ? 0 : GetLastError();
    if (!note("window", "register-owned-class", registered, registered ? S_OK : winhr(error), error, 0, 0, 0)) goto done;
    {
        RECT outer = {0, 0, 440, 240};
        if (!budget()) goto done;
        result = AdjustWindowRectEx(&outer, WS_OVERLAPPEDWINDOW, FALSE, 0);
        error = result ? 0 : GetLastError();
        if (!note("window", "adjust-local-root-for-client-440x240", result,
                  result ? S_OK : winhr(error), (uint64_t)(int64_t)(outer.right - outer.left),
                  (uint64_t)(int64_t)(outer.bottom - outer.top), error, 0) ||
            !record("window", "renderer-root1-owner-before-create", self_pid == renderer_pid &&
                    self_pid != host_pid, S_OK, self_pid, renderer_pid, host_pid, 0) || !budget()) goto done;
        root1_owner = CreateWindowExW(0, class_name, title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                     20, 20, outer.right - outer.left, outer.bottom - outer.top,
                                     NULL, NULL, instance, NULL);
        error = root1_owner ? 0 : GetLastError();
        root = roots[0] = root1_owner; /* owner already retained before canonicalization */
        result = root1_owner && window_id(root1_owner, &root_ids[0]); root_id = root_ids[0];
        if (!note("window", "create-renderer-owned-root1", result,
                  root1_owner ? S_OK : winhr(error), root_ids[0], error, 440, 240)) goto done;
    }
    for (i = 0; i < 2; ++i) {
        if (!record("window", "renderer-child-owner-before-create", self_pid == renderer_pid &&
                    root1_owner != NULL, S_OK, self_pid, renderer_pid, root_ids[0], i) || !budget()) goto done;
        child_owners[i] = CreateWindowExW(0, class_name, L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                        i ? 210 : 10, 10, 180, 100, root1_owner, NULL, instance, NULL);
        error = child_owners[i] ? 0 : GetLastError();
        children[i] = child_owners[i];
        result = child_owners[i] && window_id(child_owners[i], &child_ids[i]);
        if (!note("window", "create-renderer-owned-child", result,
                  child_owners[i] ? S_OK : winhr(error), child_ids[i], i, renderer_pid, error)) goto done;
    }
    if (!record("window", "distinct-canonical-USER-handles", root_ids[0] != root_ids[1] &&
                root_ids[1] != child_ids[0] && root_ids[1] != child_ids[1] && root_id != child_ids[0] &&
                root_id != child_ids[1] && child_ids[0] != child_ids[1], S_OK,
                root_id, root_ids[1], child_ids[0], child_ids[1]) ||
        !pump() || !verify_windows(TRUE) || !verify_births()) goto done;
    {
        pipe_message80 message, reply;
        memset(&message, 0, sizeof(message));
        message.token = make_token(0); message.hresult = S_OK;
        message.flags = BOOTSTRAP; message.sequence = 0;
        if (!send_message(&message) || !receive_message(&reply, 0) ||
            !record("bootstrap", "bootstrap-reply-exact-echo", !memcmp(&reply, &message, sizeof(reply)),
                    (HRESULT)reply.hresult, reply.sequence, reply.flags, sizeof(reply), 0) ||
            !verify_windows(TRUE) || !verify_births() ||
            !record("bootstrap", "bootstrap-complete-before-D3D", TRUE, S_OK,
                    root_ids[0], root_ids[1], renderer_pid, host_pid)) goto done;
    }
    if (!gpu_create(&gpu)) goto done;
    for (phase = 1; phase <= 3; ++phase) {
        pipe_message80 message, reply;
        phase_now = phase;
        if (phase > 1 && !reparent_children(phase)) goto done;
        if (phase == 2 && (!verify_windows(TRUE) || !resize_gpu(&gpu))) goto done;
        if (!verify_windows(TRUE) || !verify_births() ||
            !gpu_identity(&gpu, "phase-owned-device-and-two-swapchains")) goto done;
        for (i = 0; i < 2; ++i)
            if (!describe_swap(&gpu, i, "phase-swap-description", image_width, image_height)) goto done;
        if (!render_frame(&gpu, 0, phase, &hr) || !render_frame(&gpu, 1, phase, &hr)) goto done;
        memset(&message, 0, sizeof(message));
        message.token = make_token(phase); message.hresult = (int32_t)hr;
        message.flags = FRAME_FLAGS; message.sequence = phase;
        if (!send_message(&message) || !receive_message(&reply, phase) ||
            !record("pipe", "reply-exactly-echoes-actual-report", !memcmp(&reply, &message, sizeof(reply)),
                    (HRESULT)reply.hresult, phase, reply.flags, 0, 0) ||
            !verify_windows(TRUE) || !verify_births() ||
            !record("reparent", "phase-echo-complete-before-next-root", TRUE, S_OK,
                    phase, root_id, reply.sequence, 0)) goto done;
        ++phases_done; /* COM resources stayed owned throughout the gate */
    }
    phase_now = 4;
    if (!release_gpu(&gpu, &hr) || !verify_windows(TRUE) || !verify_births() ||
        !destroy_owned(&child_owners[0], "renderer-destroy-owned-child-A") ||
        !destroy_owned(&child_owners[1], "renderer-destroy-owned-child-B") ||
        !pump() || !verify_windows(FALSE) || !verify_births() ||
        !record("release", "renderer-children-destroyed-root1-retained", root1_owner != NULL,
                S_OK, root_ids[0], child_ids[0], child_ids[1], renderer_pid)) goto done;
    {
        pipe_message80 message, reply;
        memset(&message, 0, sizeof(message));
        message.token = make_token(4); message.hresult = (int32_t)hr;
        message.flags = LOCAL_COM_RELEASED; message.sequence = 4;
        if (!send_message(&message) || !receive_message(&reply, 4) ||
            !record("pipe", "release-reply-exact-echo", !memcmp(&reply, &message, sizeof(reply)),
                    (HRESULT)reply.hresult, reply.sequence, reply.flags, 0, 0) ||
            !verify_windows(FALSE) || !verify_births() || !pipe_empty(5) ||
            !record("release", "renderer-final-ACK-before-root1-destroy", root1_owner != NULL,
                    S_OK, reply.sequence, root_ids[0], pipe_sends, pipe_receives)) goto done;
        ++phases_done;
    }
    complete = record("acceptance", "renderer-four-phase-local-contract", phases_done == 4 &&
                      swaps_done == 2 && frames_done == 6 && readbacks_done == 6 && presents_done == 6 &&
                      local_released && gpu_drained && resize_calls == 2 && resize_releases == 6 &&
                      releases_done == 20 && pixels_read == 128000 && pipe_sends == 5 && pipe_receives == 5,
                      hr, phases_done, swaps_done, readbacks_done, presents_done);
done:
    start_cleanup();
    /* Even partially successful setup/Map calls retain their exact ownership. */
    if (!local_released) (void)release_gpu(&gpu, &hr);
    (void)close_owned(&input_pipe, "renderer-read-pipe");
    (void)close_owned(&output_pipe, "renderer-write-pipe");
    (void)destroy_owned(&child_owners[0], "cleanup-renderer-owned-child-A");
    (void)destroy_owned(&child_owners[1], "cleanup-renderer-owned-child-B");
    (void)destroy_owned(&root1_owner, "destroy-renderer-owned-root1");
    if (registered) {
        (void)budget();
        result = UnregisterClassW(class_name, instance);
        error = result ? 0 : GetLastError();
        if (result) registered = FALSE;
        (void)note("window", "unregister-owned-class", result, result ? S_OK : winhr(error), error, registered, 0, 0);
    }
    (void)close_owned(&peer_process, "renderer-host-process-observer");
    return complete && !failures && !io_bad;
}

static BOOL duplicate_standard(DWORD which, HANDLE *copy, const char *label)
{
    HANDLE original;
    BOOL copied;
    DWORD error;
    if (!budget()) return FALSE;
    original = GetStdHandle(which);
    error = original == INVALID_HANDLE_VALUE ? GetLastError() : 0;
    if (!note("inheritance", "actual-standard-handle", original && original != INVALID_HANDLE_VALUE,
              error ? winhr(error) : S_OK, which, (uint64_t)(uintptr_t)original, error, 0)) return FALSE;
    if (!budget()) return FALSE;
    copied = DuplicateHandle(GetCurrentProcess(), original, GetCurrentProcess(), copy,
                              0, TRUE, DUPLICATE_SAME_ACCESS);
    error = copied ? 0 : GetLastError();
    return note("inheritance", label, copied && *copy, copied ? S_OK : winhr(error),
                (uint64_t)(uintptr_t)*copy, error, 0, 0);
}

static BOOL peer_path(const char *ascii, wchar_t *wide, size_t count)
{
    size_t n = strlen(ascii), i;
    DWORD binary_type = 0, error;
    BOOL got, valid = n >= 7 && n <= 200 && n < count &&
        ((ascii[0] >= 'A' && ascii[0] <= 'Z') || (ascii[0] >= 'a' && ascii[0] <= 'z')) &&
        ascii[1] == ':' && ascii[2] == '\\';
    if (!record("arguments", "bounded-absolute-ASCII-peer-exe", valid, S_OK, n, 200, 0, 0)) return FALSE;
    for (i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char)ascii[i];
        if (ch < 32 || ch > 126 || ch == '"' || ch == '/' || (ch == ':' && i != 1))
            return record("arguments", "peer-path-character", FALSE, E_INVALIDARG, i, ch, 0, 0);
        wide[i] = ch;
    }
    wide[n] = 0;
    if (!record("arguments", "peer-exe-suffix", n >= 4 && !_stricmp(ascii + n - 4, ".exe"),
                S_OK, n, 0, 0, 0) || !budget()) return FALSE;
    got = GetBinaryTypeW(wide, &binary_type);
    error = got ? 0 : GetLastError();
    return note("identity", "opposite-PE-binary-type", got && binary_type ==
                (sizeof(void *) == 8 ? SCS_32BIT_BINARY : SCS_64BIT_BINARY),
                got ? S_OK : winhr(error), binary_type, sizeof(void *) * 8, error, 0);
}

static BOOL make_pipe(HANDLE *read_end, HANDLE *write_end, HANDLE *parent_end, const char *label)
{
    SECURITY_ATTRIBUTES security;
    BOOL made, changed;
    DWORD error;
    memset(&security, 0, sizeof(security));
    security.nLength = sizeof(security); security.bInheritHandle = TRUE;
    if (!budget()) return FALSE;
    made = CreatePipe(read_end, write_end, &security, 4096);
    error = made ? 0 : GetLastError();
    if (!note("pipe", label, made && *read_end && *write_end, made ? S_OK : winhr(error),
              (uint64_t)(uintptr_t)*read_end, (uint64_t)(uintptr_t)*write_end, 4096, error)) return FALSE;
    if (!budget()) return FALSE;
    changed = SetHandleInformation(*parent_end, HANDLE_FLAG_INHERIT, 0);
    error = changed ? 0 : GetLastError();
    return note("inheritance", "parent-end-not-inherited", changed,
                changed ? S_OK : winhr(error), (uint64_t)(uintptr_t)*parent_end, error, 0, 0);
}

/* joined_zero can only be set from this actual owned PI wait + exit query.
 * Failure cleanup still records a signaled nonzero child without claiming it
 * satisfied the successful kernel-join contract. */
static BOOL join_owned(PROCESS_INFORMATION *pi, uint64_t limit, BOOL require_zero)
{
    ULONGLONG raw_start = GetTickCount64();
    uint64_t allowance = limit > elapsed_ms ? limit - elapsed_ms : 0;
    unsigned int cleanup_polls = 0;
    if (allowance > GRACE_MS) allowance = GRACE_MS;
    if (elapsed_ms >= TOTAL_MS) allowance = 0;
    else if (allowance > TOTAL_MS - elapsed_ms) allowance = TOTAL_MS - elapsed_ms;
    for (;;) {
        DWORD wait, error, code = STILL_ACTIVE;
        BOOL got;
        if (!cleaning) {
            if (!pump() || !budget()) return FALSE;
        } else {
            /* Even with a sticky failed clock, perform an actual nonblocking
             * PI wait after force. Never infer a join from TerminateProcess. */
            (void)pump();
            (void)budget();
        }
        wait = WaitForSingleObject(pi->hProcess, 0);
        error = wait == WAIT_FAILED ? GetLastError() : 0;
        if (wait == WAIT_OBJECT_0) kernel_signaled = TRUE;
        if (wait == WAIT_OBJECT_0) {
            (void)budget();
            got = GetExitCodeProcess(pi->hProcess, &code);
            error = got ? 0 : GetLastError();
            if (got) joined_exit = code;
            if (got && code == 0) joined_zero = TRUE;
            return note("join", "actual-PI-wait-and-exit", got && (!require_zero || code == 0),
                        got ? S_OK : winhr(error), wait, code, pi->dwProcessId, error);
        }
        if (wait != WAIT_TIMEOUT)
            return record("join", "actual-PI-wait-failed", FALSE, winhr(error), wait, error, 0, 0);
        if (cleaning) {
            ULONGLONG raw_now;
            (void)budget();
            raw_now = GetTickCount64();
            if (elapsed_ms >= limit || elapsed_ms >= TOTAL_MS || raw_now < raw_start ||
                raw_now - raw_start >= allowance || ++cleanup_polls >= 1000) return FALSE;
            /* Independent finite count/tick bound permits a real post-force
             * join even after a QPC failure. The failed clock stays sticky.
             * Still service driver RPCs on every iteration; never Sleep/wait
             * for the whole grace interval without dispatching messages. */
            (void)budget();
            wait = MsgWaitForMultipleObjectsEx(0, NULL, 5, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            error = wait == WAIT_FAILED ? GetLastError() : 0;
            (void)budget();
            if (wait != WAIT_TIMEOUT && wait != WAIT_OBJECT_0)
                return record("join", "cleanup-message-wait", FALSE, winhr(error), wait, error, 0, 0);
            (void)pump();
            continue;
        }
        if (!budget()) return FALSE;
        if (elapsed_ms >= limit) return FALSE;
        if (!pause_pumping(5)) return FALSE;
    }
}

static BOOL host_main(char **argv)
{
    static const wchar_t class_name[] = L"CiderControllerResizeV1";
    wchar_t executable[201], command[512];
    char command_ascii[512];
    WNDCLASSEXW wc;
    PROCESS_INFORMATION pi;
    STARTUPINFOEXW startup;
    HANDLE child_read = NULL, child_write = NULL, stdout_copy = NULL, stderr_copy = NULL;
    HANDLE inherited[4];
    LPPROC_THREAD_ATTRIBUTE_LIST attributes = NULL;
    SIZE_T attribute_bytes = 0;
    HANDLE heap = NULL;
    BOOL attributes_ready = FALSE, registered = FALSE, launched = FALSE, complete = FALSE;
    BOOL result;
    HINSTANCE instance = NULL; /* borrowed GetModuleHandleW result: never FreeLibrary */
    DWORD error, actual_pid;
    unsigned int i, phase;
    int n;
    memset(&pi, 0, sizeof(pi)); memset(&startup, 0, sizeof(startup)); memset(&wc, 0, sizeof(wc));
    host_pid = self_pid;
    if (!peer_path(argv[2], executable, sizeof(executable) / sizeof(executable[0]))) goto done;
    if (!process_birth(GetCurrentProcess(), &self_birth, "host-birth")) goto done;
    if (!budget()) goto done;
    instance = GetModuleHandleW(NULL);
    error = instance ? 0 : GetLastError();
    if (!note("window", "borrowed-executable-module", instance != NULL,
              instance ? S_OK : winhr(error), (uint64_t)(uintptr_t)instance, error, 0, 0)) goto done;
    wc.cbSize = sizeof(wc); wc.lpfnWndProc = window_proc; wc.hInstance = instance;
    wc.lpszClassName = class_name;
    /* Borrowed stock brush: define fixture background in Win32, not by theme. */
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    if (!note("window", "class-borrowed-white-background", wc.hbrBackground != NULL,
              wc.hbrBackground ? S_OK : E_FAIL, (uint64_t)(uintptr_t)wc.hbrBackground, WHITE_BRUSH, 0, 0)) goto done;
    if (!budget()) goto done;
    registered = RegisterClassExW(&wc) != 0;
    error = registered ? 0 : GetLastError();
    if (!note("window", "register-owned-class", registered, registered ? S_OK : winhr(error), error, 0, 0, 0)) goto done;
    {
        RECT outer = {0, 0, 440, 240};
        if (!budget()) goto done;
        result = AdjustWindowRectEx(&outer, WS_OVERLAPPEDWINDOW, FALSE, 0);
        error = result ? 0 : GetLastError();
        if (!note("window", "adjust-host-root-for-client-440x240", result,
                  result ? S_OK : winhr(error), (uint64_t)(int64_t)(outer.right - outer.left),
                  (uint64_t)(int64_t)(outer.bottom - outer.top), error, 0) || !budget()) goto done;
        root2_owner = CreateWindowExW(0, class_name, inactive_title, WS_OVERLAPPEDWINDOW,
                                     20, 20, outer.right - outer.left, outer.bottom - outer.top,
                                     NULL, NULL, instance, NULL);
        error = root2_owner ? 0 : GetLastError();
        root = roots[1] = root2_owner;
        result = root2_owner && window_id(root2_owner, &root_ids[1]); root_id = root_ids[1];
        if (!note("window", "create-host-owned-root2", result,
                  root2_owner ? S_OK : winhr(error), root_ids[1], error, 440, 240)) goto done;
    }
    if (!make_pipe(&child_read, &output_pipe, &output_pipe, "fresh-host-to-renderer-4096") ||
        !make_pipe(&input_pipe, &child_write, &input_pipe, "fresh-renderer-to-host-4096") ||
        !duplicate_standard(STD_OUTPUT_HANDLE, &stdout_copy, "stdout-intended-copy") ||
        !duplicate_standard(STD_ERROR_HANDLE, &stderr_copy, "stderr-intended-copy")) goto done;
    {
        uint32_t read_wire = 0, write_wire = 0;
        result = kernel_wire_id(child_read, &read_wire) && kernel_wire_id(child_write, &write_wire);
        if (!record("inheritance", "pipe-handles-fit-both-bitnesses", result && child_read != child_write,
                    S_OK, read_wire, write_wire, 0, 0)) goto done;
    }
    if (!budget()) goto done;
    result = InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_bytes);
    error = GetLastError();
    if (!note("inheritance", "attribute-list-size-probe", !result && error == ERROR_INSUFFICIENT_BUFFER &&
              attribute_bytes != 0 && attribute_bytes <= 1024 * 1024, S_OK,
              result, error, attribute_bytes, 0)) goto done;
    if (!budget()) goto done;
    heap = GetProcessHeap();
    if (!note("inheritance", "borrowed-process-heap", heap != NULL, heap ? S_OK : winhr(GetLastError()),
              (uint64_t)(uintptr_t)heap, 0, 0, 0) || !budget()) goto done;
    attributes = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(heap, 0, attribute_bytes);
    if (!note("inheritance", "allocate-attribute-list", attributes != NULL,
              attributes ? S_OK : E_OUTOFMEMORY, attribute_bytes, 0, 0, 0) || !budget()) goto done;
    result = InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_bytes);
    error = result ? 0 : GetLastError();
    if (result) attributes_ready = TRUE;
    if (!note("inheritance", "initialize-attribute-list", result, result ? S_OK : winhr(error),
              attribute_bytes, error, 0, 0)) goto done;
    inherited[0] = child_read; inherited[1] = child_write;
    inherited[2] = stdout_copy; inherited[3] = stderr_copy;
    if (!budget()) goto done;
    result = UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                       inherited, sizeof(inherited), NULL, NULL);
    error = result ? 0 : GetLastError();
    if (!note("inheritance", "explicit-four-handle-list", result, result ? S_OK : winhr(error),
              4, (uint64_t)(uintptr_t)child_read, (uint64_t)(uintptr_t)child_write, error)) goto done;
    n = snprintf(command_ascii, sizeof(command_ascii),
                 "\"%s\" --renderer %08" PRIx32 " %s %08" PRIx32 " %08" PRIx32,
                 argv[2], root_ids[1], nonce_text,
                 (uint32_t)(uintptr_t)child_read, (uint32_t)(uintptr_t)child_write);
    if (!record("arguments", "bounded-quoted-ASCII-command", n > 0 && (size_t)n < sizeof(command_ascii) &&
                (size_t)n < sizeof(command) / sizeof(command[0]), S_OK, n > 0 ? (uint64_t)n : 0, 0, 0, 0)) goto done;
    for (i = 0; i <= (unsigned int)n; ++i) command[i] = (wchar_t)(unsigned char)command_ascii[i];
    startup.StartupInfo.cb = sizeof(startup); startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = child_read; startup.StartupInfo.hStdOutput = stdout_copy;
    startup.StartupInfo.hStdError = stderr_copy; startup.lpAttributeList = attributes;
    if (!healthy() || !pump() || !budget()) goto done;
    result = CreateProcessW(executable, command, NULL, NULL, TRUE, EXTENDED_STARTUPINFO_PRESENT,
                            NULL, L"C:\\CiderTests", &startup.StartupInfo, &pi);
    error = result ? 0 : GetLastError();
    if (result) {
        launched = TRUE; renderer_pid = pi.dwProcessId; peer_process = pi.hProcess; /* borrowed alias */
    }
    if (!note("process", "ordinary-CreateProcessW-owned-peer", result && pi.hProcess && pi.hThread &&
              pi.dwProcessId && pi.dwThreadId && pi.dwProcessId != self_pid,
              result ? S_OK : winhr(error), pi.dwProcessId, pi.dwThreadId,
              (uint64_t)(uintptr_t)pi.hProcess, error)) goto done;
    if (!close_owned(&child_read, "host-child-read-copy") ||
        !close_owned(&child_write, "host-child-write-copy") ||
        !close_owned(&stdout_copy, "host-child-stdout-copy") ||
        !close_owned(&stderr_copy, "host-child-stderr-copy")) goto done;
    if (!budget()) goto done;
    actual_pid = GetProcessId(pi.hProcess);
    if (!note("identity", "PI-handle-actual-renderer-PID", actual_pid == renderer_pid,
              actual_pid ? S_OK : winhr(GetLastError()), actual_pid, renderer_pid, 0, 0) ||
        !process_birth(pi.hProcess, &peer_birth, "renderer-birth") ||
        !record("identity", "owned-renderer-birth-order", peer_birth >= self_birth, S_OK,
                self_birth, peer_birth, 0, 0) || !opposite_process(pi.hProcess) || !verify_births()) goto done;
    {
        pipe_message80 bootstrap;
        if (!receive_message(&bootstrap, 0) || !send_message(&bootstrap) ||
            !record("bootstrap", "bootstrap-echo-sent-before-render-phases", TRUE, S_OK,
                    bootstrap.sequence, bootstrap.flags, sizeof(bootstrap), 0)) goto done;
    }
    for (phase = 1; phase <= 4; ++phase) {
        pipe_message80 message;
        phase_now = phase;
        /* This changes only the expected identity. Renderer can still be
         * checking the previous echo/tree; do not inspect the new tree until
         * its complete phase report arrives. Root2 was known before spawn. */
        if (!expect_phase_root(phase)) goto done;
        if (!receive_message(&message, phase) || !verify_windows(phase < 4) || !verify_births()) goto done;
        if (phase < 4) {
            ++peer_frame_reports; peer_frames_reported += 2;
            peer_readbacks_reported += 2; peer_presents_reported += 2;
        } else {
            peer_release_reported = TRUE; /* an authenticated local report, not Native proof */
            if (!record("release", "host-observed-child-absence-root1-live-renderer-alive", TRUE, S_OK,
                        root_ids[0], child_ids[0], child_ids[1], renderer_pid)) goto done;
            /* Borrowed children are already absent. Host never destroys them. */
        }
        if (!ready_and_gate(phase) || !verify_births()) goto done;
        if (!verify_windows(phase < 4)) goto done;
        if (phase == 4 && !pipe_empty(4)) goto done;
        if (!send_message(&message)) goto done; /* exact actual report echo, after real gate */
        if (!record("reparent", "phase-echo-sent-before-next-root", TRUE, S_OK,
                    phase, root_id, message.sequence, 0)) goto done;
        ++phases_done;
    }
    /* Renderer may exit immediately after consuming reply 4; no liveness
     * assertion or pipe peek is allowed here to turn that valid exit into error. */
    if (!join_owned(&pi, WORK_MS, TRUE)) {
        if (!failures) record("join", "renderer-join-deadline", FALSE,
                              HRESULT_FROM_WIN32(ERROR_TIMEOUT), elapsed_ms, 0, 0, 0);
        goto done;
    }
    complete = record("acceptance", "host-four-phase-and-real-zero-exit", phases_done == 4 &&
                      peer_frame_reports == 3 && peer_frames_reported == 6 && peer_readbacks_reported == 6 &&
                      peer_presents_reported == 6 && peer_release_reported && pipe_sends == 5 &&
                      pipe_receives == 5 && joined_zero && !force_attempted,
                      S_OK, phases_done, peer_frame_reports, joined_exit, kernel_signaled);
done:
    start_cleanup();
    /* Close every host copy of a child endpoint before cooperative failure wait. */
    (void)close_owned(&child_read, "cleanup-child-read-copy");
    (void)close_owned(&child_write, "cleanup-child-write-copy");
    (void)close_owned(&stdout_copy, "cleanup-stdout-copy");
    (void)close_owned(&stderr_copy, "cleanup-stderr-copy");
    (void)close_owned(&input_pipe, "host-read-pipe");
    (void)close_owned(&output_pipe, "host-write-pipe");
    (void)close_owned(&phase_file, "host-outstanding-phase-file");
    if (launched && !kernel_signaled) {
        uint64_t grace_limit = elapsed_ms + GRACE_MS;
        uint64_t cleanup_limit = cleanup_start + CLEANUP_MS;
        if (grace_limit > cleanup_limit) grace_limit = cleanup_limit;
        if (!join_owned(&pi, grace_limit, FALSE) && !kernel_signaled && !force_attempted) {
            force_attempted = TRUE;
            (void)budget();
            result = TerminateProcess(pi.hProcess, 1);
            error = result ? 0 : GetLastError();
            if (result) force_succeeded = TRUE;
            (void)note("process", "failure-only-owned-peer-force-once", result,
                       result ? S_OK : winhr(error), pi.dwProcessId, result, error, 0);
            (void)join_owned(&pi, cleanup_limit, FALSE);
        }
    }
    if (launched && !kernel_signaled)
        record("join", "failure-child-not-actually-joined", FALSE, E_FAIL,
               pi.dwProcessId, force_attempted, force_succeeded, 0);
    /* No host DestroyWindow of borrowed Root1 or children, even on failure.
     * Keep our Root2 until an actual PI wait signals the renderer is gone. */
    if (!launched || kernel_signaled) {
        if (complete) {
            BOOL present;
            (void)budget();
            present = IsWindow(window_from_id(root_ids[0]));
            (void)note("window", "joined-renderer-root1-actually-absent", !present,
                       S_OK, root_ids[0], present, renderer_pid, kernel_signaled);
        }
        (void)destroy_owned(&root2_owner, "destroy-host-owned-root2");
    }
    if (registered && !root2_owner) {
        (void)budget();
        result = UnregisterClassW(class_name, instance);
        error = result ? 0 : GetLastError();
        if (result) registered = FALSE;
        (void)note("window", "unregister-owned-class", result, result ? S_OK : winhr(error), error, registered, 0, 0);
    }
    if (attributes_ready) {
        (void)budget();
        DeleteProcThreadAttributeList(attributes);
        attributes_ready = FALSE;
        (void)note("inheritance", "delete-attribute-list", TRUE, S_OK, 0, 0, 0, 0);
    }
    if (attributes) {
        (void)budget();
        result = HeapFree(heap, 0, attributes);
        error = result ? 0 : GetLastError();
        if (result) attributes = NULL;
        (void)note("inheritance", "free-attribute-storage", result, result ? S_OK : winhr(error), error, 0, 0, 0);
    }
    (void)close_owned(&pi.hThread, "PI-thread-handle");
    (void)close_owned(&pi.hProcess, "PI-process-handle");
    peer_process = pi.hProcess; /* preserve failed close ownership; no second alias close */
    return complete && !failures && !io_bad && !force_attempted;
}

int main(int argc, char **argv)
{
    BOOL result = FALSE, got;
    DWORD error;
    self_pid = GetCurrentProcessId();
    output = GetStdHandle(STD_OUTPUT_HANDLE); /* borrowed */
    if (argc > 1 && !strcmp(argv[1], "--renderer")) role = "renderer";
    if (argc == 4 && !strcmp(argv[1], "--host")) (void)parse_nonce(argv[3]);
    else if (argc == 6 && !strcmp(argv[1], "--renderer")) (void)parse_nonce(argv[3]);
    got = QueryPerformanceFrequency(&frequency);
    error = got ? 0 : GetLastError();
    if (!got || frequency.QuadPart <= 0) clock_bad = TRUE;
    if (!record("clock", "QPC-frequency", got && frequency.QuadPart > 0,
                got ? S_OK : winhr(error), (uint64_t)frequency.QuadPart, 0, 0, 0)) goto finish;
    got = QueryPerformanceCounter(&first_qpc);
    error = got ? 0 : GetLastError();
    first_tick = GetTickCount64(); last_tick = first_tick; last_qpc = first_qpc;
    clock_ready = got && first_qpc.QuadPart >= 0;
    if (!clock_ready) clock_bad = TRUE;
    if (!record("clock", "QPC-start", clock_ready, got ? S_OK : winhr(error),
                (uint64_t)first_qpc.QuadPart, first_tick, 0, 0)) goto finish;
    if (!note("arguments", "exact-role-argc-and-32hex-nonce", strlen(nonce_text) == 32 &&
              ((argc == 4 && !strcmp(argv[1], "--host")) ||
              (argc == 6 && !strcmp(argv[1], "--renderer"))), S_OK,
              (uint64_t)(unsigned int)argc, 0, 0, 0)) goto finish;
    if (!strcmp(role, "renderer")) result = renderer_main(argv);
    else result = host_main(argv);
finish:
    if (clock_ready) (void)budget();
    (void)record("summary", "final", result && !failures && !io_bad && !clock_bad &&
                 !timeout_bad && !force_attempted, S_OK, failures, checks, elapsed_ms, 0);
    return result && !failures && !io_bad && !clock_bad && !timeout_bad && !force_attempted ? 0 : 1;
}
