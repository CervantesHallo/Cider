#include <windows.h>
#include <winternl.h>
#include <stdint.h>
#include <stdio.h>
#include "coordinator-fixture.h"
typedef NTSTATUS (WINAPI *probe_fn)(struct coordinator_fixture *);
_Static_assert(sizeof(struct coordinator_fixture) == 80, "fixed PE/native fixture frame");
static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return DefWindowProcA(hwnd, msg, wp, lp);
}
int main(void)
{
    WNDCLASSA wc = {0};
    HWND root = NULL, child = NULL;
    HMODULE module = NULL;
    probe_fn probe;
    struct coordinator_fixture frame = {0};
    NTSTATUS status = (NTSTATUS)0xc0000001;
    BOOL success = FALSE, cleanup = TRUE;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "Cider-owned-root-coordinator";
    if (!RegisterClassA(&wc)) return 2;
    root = CreateWindowA(wc.lpszClassName, "Cider owned coordinator exercise", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                         160, 160, 440, 330, NULL, NULL, wc.hInstance, NULL);
    if (!root) goto done;
    child = CreateWindowA(wc.lpszClassName, "", WS_CHILD | WS_VISIBLE | WS_BORDER,
                          40, 30, 100, 80, root, NULL, wc.hInstance, NULL);
    if (!child) goto done;
    if (!(module = LoadLibraryW(L"winemac.drv"))) goto done;
    if (!(probe = (probe_fn)(void *)GetProcAddress(module, "cider_remote_probe"))) goto done;
    frame.magic = COORDINATOR_FIXTURE_MAGIC; frame.version = 1;
    frame.root = (uint32_t)(uintptr_t)root; frame.child = (uint32_t)(uintptr_t)child;
    status = probe(&frame);
    success = !status && frame.checks == 32 && !frame.failures && frame.snapshots == 9 &&
              frame.allocation_faults == 1 && frame.exhaustion_faults == 1 && frame.cleanup == 1 &&
              frame.epoch >= (UINT64_C(1) << 33) && frame.tickets[0] >= (UINT64_C(1) << 33) &&
              frame.tickets[1] >= (UINT64_C(1) << 33) && frame.tickets[2] >= (UINT64_C(1) << 33) &&
              frame.tickets[0] != frame.tickets[1] && frame.tickets[1] != frame.tickets[2];
done:
    if (child) cleanup = DestroyWindow(child) && cleanup;
    if (root) cleanup = DestroyWindow(root) && cleanup;
    if (module) FreeLibrary(module);
    cleanup = UnregisterClassA(wc.lpszClassName, wc.hInstance) && cleanup;
    printf("{\"type\":\"remote-coordinator\",\"bits\":%u,\"status\":%lu,\"checks\":%lu,\"failures\":%lu,\"epoch\":%llu,\"ticket_a\":%llu,\"ticket_b\":%llu,\"ticket_c\":%llu,\"snapshots\":%lu,\"allocation_faults\":%lu,\"exhaustion_faults\":%lu,\"cleanup\":%lu,\"windows_cleanup\":%u}\n",
           (unsigned int)(8 * sizeof(void *)), (unsigned long)status, (unsigned long)frame.checks,
           (unsigned long)frame.failures, (unsigned long long)frame.epoch, (unsigned long long)frame.tickets[0],
           (unsigned long long)frame.tickets[1], (unsigned long long)frame.tickets[2], (unsigned long)frame.snapshots, (unsigned long)frame.allocation_faults,
           (unsigned long)frame.exhaustion_faults, (unsigned long)frame.cleanup, (unsigned int)cleanup);
    return success && cleanup ? 0 : 1;
}
