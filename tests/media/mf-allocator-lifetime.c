/* Owned allocator teardown probe. No decoder, SourceReader, HWND or game.
 * The manager must be the last device owner while cached shared textures are
 * released. Do not add an extra device pin to hide the destructor ordering.
 */
#define COBJMACROS
#define _WIN32_WINNT 0x0602
#include <windows.h>
#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <stdio.h>

static unsigned int checks, failures, cases;
static LONG final_release_phase;
static LONG fault_reporting;
static unsigned int current_keyed;

static int check(const char *name, int ok, unsigned long value)
{
    ++checks;
    if (!ok) ++failures;
    printf("allocator-check keyed=%u name=%s ok=%u value=%lu\n", current_keyed, name, !!ok, value);
    fflush(stdout);
    return ok;
}

static LONG CALLBACK observe(EXCEPTION_POINTERS *exception)
{
    void *frames[20];
    HMODULE module;
    USHORT count;
    unsigned int i;

    if (exception->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedExchange(&fault_reporting, 1))
    {
        TerminateProcess(GetCurrentProcess(), 118);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    printf("allocator-native-fault keyed=%u phase=%ld code=%08lx address=%p\n", current_keyed,
            final_release_phase, exception->ExceptionRecord->ExceptionCode,
            exception->ExceptionRecord->ExceptionAddress);
    count = RtlCaptureStackBackTrace(0, 20, frames, NULL);
    for (i = 0; i < count; ++i)
    {
        module = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (const char *)frames[i], &module);
        printf("allocator-fault-frame index=%u address=%p module=%p offset=%Ix\n", i,
                frames[i], module, (UINT_PTR)frames[i] - (UINT_PTR)module);
    }
    fflush(stdout);
    /* Diagnostic termination is always failure, with no attempted DLL teardown
     * through an invalid object. It never supplies a successful API result. */
    TerminateProcess(GetCurrentProcess(), final_release_phase ? 117 : 118);
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI watchdog(void *unused)
{
    (void)unused;
    Sleep(15000);
    /* Never wait for CRT logging/unwind locks on the termination path. */
    TerminateProcess(GetCurrentProcess(), 124);
    return 0;
}

#define HR(name, call) do { hr = (call); if (!check(name, hr == S_OK, (unsigned long)hr)) goto done; } while (0)
#define DROP(p) do { if (p) IUnknown_Release((IUnknown *)(p)); } while (0)

static void allocator_case(unsigned int keyed)
{
    IMFVideoSampleAllocatorEx *allocator = NULL;
    IMFVideoSampleAllocatorCallback *callback = NULL;
    IMFDXGIDeviceManager *manager = NULL;
    ID3D11Device *device = NULL;
    IMFMediaType *type = NULL;
    IMFAttributes *attributes = NULL;
    D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_11_0, actual;
    UINT token;
    LONG free_samples = -1;
    ULONG device_refs, manager_refs, allocator_refs;
    HRESULT hr;

    current_keyed = keyed;
    HR("device", D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &requested, 1,
            D3D11_SDK_VERSION, &device, &actual, NULL));
    if (!check("feature-level", actual == requested, actual)) goto done;
    HR("manager", MFCreateDXGIDeviceManager(&token, &manager));
    HR("reset-device", IMFDXGIDeviceManager_ResetDevice(manager, (IUnknown *)device, token));
    HR("allocator", MFCreateVideoSampleAllocatorEx(&IID_IMFVideoSampleAllocatorEx, (void **)&allocator));
    HR("set-manager", IMFVideoSampleAllocatorEx_SetDirectXManager(allocator, (IUnknown *)manager));
    HR("type", MFCreateMediaType(&type));
    HR("major", IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video));
    HR("subtype", IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32));
    HR("size", IMFMediaType_SetUINT64(type, &MF_MT_FRAME_SIZE, ((UINT64)16 << 32) | 16));
    HR("stride", IMFMediaType_SetUINT32(type, &MF_MT_DEFAULT_STRIDE, 64));
    HR("attributes", MFCreateAttributes(&attributes, 3));
    HR("shared", IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_SHARED, keyed));
    HR("without-mutex", IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_SHARED_WITHOUT_MUTEX, !keyed));
    HR("bind", IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_BINDFLAGS,
            D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE));
    HR("initialize-two", IMFVideoSampleAllocatorEx_InitializeSampleAllocatorEx(allocator, 2, 2, attributes, type));
    HR("free-count-interface", IMFVideoSampleAllocatorEx_QueryInterface(allocator,
            &IID_IMFVideoSampleAllocatorCallback, (void **)&callback));
    HR("free-count", IMFVideoSampleAllocatorCallback_GetFreeSampleCount(callback, &free_samples));
    if (!check("two-owned-free-samples", free_samples == 2, free_samples)) goto done;
    IMFVideoSampleAllocatorCallback_Release(callback);
    callback = NULL;
    device_refs = ID3D11Device_Release(device);
    device = NULL;
    manager_refs = IMFDXGIDeviceManager_Release(manager);
    manager = NULL;
    /* Bind this test to the actual last-owner state, rather than assuming a
     * hidden extra reference cannot keep an incorrectly ordered path alive. */
    if (!check("last-device-owner", device_refs == 1, device_refs)) goto done;
    if (!check("last-manager-owner", manager_refs == 1, manager_refs)) goto done;
    printf("allocator-before-final-release keyed=%u device_refs=%lu manager_refs=%lu\n",
            keyed, device_refs, manager_refs);
    fflush(stdout);
    InterlockedExchange(&final_release_phase, 1);
    allocator_refs = IMFVideoSampleAllocatorEx_Release(allocator);
    allocator = NULL;
    InterlockedExchange(&final_release_phase, 0);
    if (check("final-allocator-release", allocator_refs == 0, allocator_refs)) ++cases;
done:
    DROP(callback);
    DROP(allocator);
    DROP(attributes);
    DROP(type);
    DROP(manager);
    DROP(device);
}

int main(void)
{
    HANDLE timer;
    HRESULT hr;

    timer = CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    if (!timer) return 121;
    CloseHandle(timer);
    if (!AddVectoredExceptionHandler(1, observe)) return 122;
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) return 123;
    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    if (FAILED(hr)) return 125;
    allocator_case(0);
    allocator_case(1);
    check("both-sharing-cases", cases == 2, cases);
    hr = MFShutdown();
    check("shutdown", hr == S_OK, (unsigned long)hr);
    CoUninitialize();
    printf("allocator-complete bits=%u checks=%u failures=%u cases=%u\n",
            (unsigned int)(8 * sizeof(void *)), checks, failures, cases);
    fflush(stdout);
    return failures ? 1 : 0;
}
