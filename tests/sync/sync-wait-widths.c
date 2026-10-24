/* Original N matrix: WaitAny 2/8/64 and WaitAll 2/8, mixed object states.
 * Late signal is a real peer action, not proof that the waiter kernel-parked.
 * No waiting handle is closed; every owned peer is joined before stack exit. */
#define _WIN32_WINNT 0x0602
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

static unsigned checks, failures, width;
static const char *mode;
static void check(const char *name, int ok, int64_t actual, int64_t expected)
{
    ++checks; failures += !ok;
    printf("{\"type\":\"check\",\"mode\":\"%s\",\"width\":%u,\"name\":\"%s\",\"ok\":%s,\"actual\":%" PRId64 ",\"expected\":%" PRId64 "}\n",
           mode, width, name, ok ? "true" : "false", actual, expected);
    if (fflush(stdout) || ferror(stdout)) ExitProcess(120);
}
static void fatal(const char *name)
{
    check(name, 0, GetLastError(), 0);
    TerminateProcess(GetCurrentProcess(), 121); ExitProcess(121);
}
static void close_one(HANDLE handle) { if (!CloseHandle(handle)) fatal("close"); }
/* Repeating manual, auto, semaphore. All handles are distinct. */
static HANDLE make(unsigned index, int ready)
{
    HANDLE h = index % 3 == 2 ? CreateSemaphoreW(NULL, ready, 1, NULL)
                             : CreateEventW(NULL, index % 3 == 0, ready, NULL);
    if (!h) fatal("create");
    return h;
}
static void signal_one(HANDLE h, unsigned index)
{
    LONG previous = -1;
    if (index % 3 == 2) {
        if (!ReleaseSemaphore(h, 1, &previous)) fatal("release");
        check("release_previous_zero", previous == 0, previous, 0);
    } else if (!SetEvent(h)) fatal("set");
}
struct peer { HANDLE gate, target; };
static DWORD WINAPI late_signal(void *opaque)
{
    struct peer *p = opaque;
    if (WaitForSingleObject(p->gate, 5000) != WAIT_OBJECT_0) return 10;
    /* All selected late targets are events (1/7/63), not semaphores. */
    return SetEvent(p->target) ? 0 : 11;
}
static void any_width(unsigned count)
{
    HANDLE objects[MAXIMUM_WAIT_OBJECTS], thread;
    struct peer peer;
    DWORD status, code = 999;
    unsigned i;
    width = count; mode = "any";
    for (i = 0; i < count; ++i) objects[i] = make(i, 0);
    status = WaitForMultipleObjects(count, objects, FALSE, 0);
    check("initial_timeout", status == WAIT_TIMEOUT, status, WAIT_TIMEOUT);
    for (i = 0; i < count; ++i) signal_one(objects[i], i);
    for (i = 0; i < count; ++i) {
        status = WaitForMultipleObjects(count, objects, FALSE, 0);
        check("lowest_ready_index", status == WAIT_OBJECT_0 + i, status, WAIT_OBJECT_0 + i);
        status = WaitForSingleObject(objects[i], 0);
        check("selected_object_state", status == (i % 3 == 0 ? WAIT_OBJECT_0 : WAIT_TIMEOUT),
              status, i % 3 == 0 ? WAIT_OBJECT_0 : WAIT_TIMEOUT);
        if (i % 3 == 0 && !ResetEvent(objects[i])) fatal("reset_manual");
    }
    status = WaitForMultipleObjects(count, objects, FALSE, 0);
    check("drained_timeout", status == WAIT_TIMEOUT, status, WAIT_TIMEOUT);
    peer.gate = CreateEventW(NULL, FALSE, FALSE, NULL);
    peer.target = objects[count - 1];
    if (!peer.gate) fatal("gate");
    thread = CreateThread(NULL, 0, late_signal, &peer, 0, NULL);
    if (!thread) fatal("peer_create");
    if (!SetEvent(peer.gate)) fatal("peer_start");
    status = WaitForMultipleObjects(count, objects, FALSE, 5000);
    check("late_peer_index", status == WAIT_OBJECT_0 + count - 1, status, WAIT_OBJECT_0 + count - 1);
    if (WaitForSingleObject(thread, 5000) != WAIT_OBJECT_0) fatal("peer_join");
    if (!GetExitCodeThread(thread, &code)) fatal("peer_exit_api");
    check("peer_exit", code == 0, code, 0);
    status = WaitForSingleObject(objects[count - 1], 0);
    check("late_object_state", status == ((count - 1) % 3 == 0 ? WAIT_OBJECT_0 : WAIT_TIMEOUT),
          status, (count - 1) % 3 == 0 ? WAIT_OBJECT_0 : WAIT_TIMEOUT);
    close_one(thread); close_one(peer.gate);
    for (i = 0; i < count; ++i) close_one(objects[i]);
}
static void all_width(unsigned count)
{
    HANDLE objects[8]; DWORD status; unsigned i;
    width = count; mode = "all";
    for (i = 0; i < count; ++i) objects[i] = make(i, i + 1 != count);
    status = WaitForMultipleObjects(count, objects, TRUE, 0);
    check("blocked_timeout", status == WAIT_TIMEOUT, status, WAIT_TIMEOUT);
    for (i = 0; i + 1 < count; ++i) {
        status = WaitForSingleObject(objects[i], 0);
        check("blocked_preserves_ready", status == WAIT_OBJECT_0, status, WAIT_OBJECT_0);
        if (i % 3 != 0) signal_one(objects[i], i);
    }
    status = WaitForSingleObject(objects[count - 1], 0);
    check("sentinel_still_empty", status == WAIT_TIMEOUT, status, WAIT_TIMEOUT);
    signal_one(objects[count - 1], count - 1);
    status = WaitForMultipleObjects(count, objects, TRUE, 5000);
    /* Documented WaitAll success is a range, not a required index zero. */
    check("all_ready_range", status < WAIT_OBJECT_0 + count, status, WAIT_OBJECT_0);
    for (i = 0; i < count; ++i) {
        status = WaitForSingleObject(objects[i], 0);
        check("final_object_state", status == (i % 3 == 0 ? WAIT_OBJECT_0 : WAIT_TIMEOUT),
              status, i % 3 == 0 ? WAIT_OBJECT_0 : WAIT_TIMEOUT);
    }
    status = WaitForMultipleObjects(count, objects, TRUE, 0);
    check("cannot_consume_twice", status == WAIT_TIMEOUT, status, WAIT_TIMEOUT);
    for (i = 0; i < count; ++i) close_one(objects[i]);
}
int main(void)
{
    printf("{\"type\":\"begin\",\"mode\":\"wait-widths-correctness\",\"pointer_bits\":%u,\"pid\":%lu}\n",
           (unsigned)(8 * sizeof(void *)), GetCurrentProcessId());
    if (fflush(stdout) || ferror(stdout)) return 2;
    any_width(2); any_width(8); any_width(64); all_width(2); all_width(8);
    printf("{\"type\":\"complete\",\"mode\":\"wait-widths-correctness\",\"ok\":%s,\"checks\":%u,\"failures\":%u,\"output_error\":%s}\n",
           failures ? "false" : "true", checks, failures, ferror(stdout) ? "true" : "false");
    return failures || fflush(stdout) || ferror(stdout) ? 1 : 0;
}
