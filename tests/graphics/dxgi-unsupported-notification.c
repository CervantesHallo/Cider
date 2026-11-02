#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dxgi1_4.h>
#include <stdio.h>

static unsigned checks, failures;
static void check(int ok, const char *name)
{
    ++checks;
    if (!ok) ++failures;
    printf("{\"check\":\"%s\",\"ok\":%s}\n", name, ok ? "true" : "false");
}

int main(void)
{
    IDXGIFactory1 *factory = NULL;
    IDXGIAdapter1 *base = NULL;
    IDXGIAdapter3 *adapter = NULL;
    HANDLE event = NULL;
    DWORD cookie = 0x13572468, flags = 0;
    HRESULT hr;
    hr = CreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&factory);
    check(SUCCEEDED(hr) && factory, "factory");
    if (!factory) goto done;
    hr = IDXGIFactory1_EnumAdapters1(factory, 0, &base);
    check(SUCCEEDED(hr) && base, "adapter");
    if (!base) goto done;
    hr = IDXGIAdapter1_QueryInterface(base, &IID_IDXGIAdapter3, (void **)&adapter);
    check(SUCCEEDED(hr) && adapter, "adapter3");
    if (!adapter) goto done;
    event = CreateEventW(NULL, TRUE, FALSE, NULL);
    check(event != NULL, "event");
    if (!event) goto done;
    hr = IDXGIAdapter3_RegisterHardwareContentProtectionTeardownStatusEvent(adapter, event, &cookie);
    /* This asserts the candidate's explicit failure, not a Windows reference result. */
    check(hr == E_NOTIMPL, "explicit-not-implemented");
    check(cookie == 0x13572468, "no-cookie-issued");
    check(WaitForSingleObject(event, 0) == WAIT_TIMEOUT, "event-not-signalled");
    check(GetHandleInformation(event, &flags), "event-still-owned-by-caller");
    IDXGIAdapter3_UnregisterHardwareContentProtectionTeardownStatus(adapter, cookie);
    check(GetHandleInformation(event, &flags), "unregister-keeps-caller-handle");
    check(IDXGIAdapter3_RegisterHardwareContentProtectionTeardownStatusEvent(adapter, NULL, NULL)
          == E_NOTIMPL, "unsupported-null-call-no-dereference");
done:
    if (event) check(CloseHandle(event), "caller-closes-event");
    if (adapter) IDXGIAdapter3_Release(adapter);
    if (base) IDXGIAdapter1_Release(base);
    if (factory) IDXGIFactory1_Release(factory);
    printf("{\"complete\":true,\"checks\":%u,\"failures\":%u}\n", checks, failures);
    return failures || checks != 11 || fflush(stdout) || ferror(stdout) ? 1 : 0;
}
