/* Owned offscreen D3D11 shared-texture regression. No HWND or swapchain.
 * Two hardware devices open one resource, transfer keyed ownership where
 * requested, and read back all 16x16 RGBA pixels. Not a rendering/game/FPS test.
 */
#define COBJMACROS
#define INITGUID
#define _WIN32_WINNT 0x0602
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned checks, failures, modes;

static int check(const char *name, int ok, unsigned long value)
{
    ++checks;
    if (!ok) ++failures;
    printf("{\"type\":\"check\",\"name\":\"%s\",\"ok\":%s,\"value\":%lu}\n",
            name, ok ? "true" : "false", value);
    fflush(stdout);
    return ok;
}

static DWORD WINAPI watchdog(void *parameter)
{
    (void)parameter;
    Sleep(15000);
    fputs("owned-shared-texture-watchdog-expired\n", stderr);
    fflush(stderr);
    ExitProcess(124);
    return 0;
}

#define DROP(pointer) do { if (pointer) IUnknown_Release((IUnknown *)(pointer)); } while (0)

static void shared_mode(int keyed)
{
    ID3D11Device *producer = NULL, *consumer = NULL;
    ID3D11DeviceContext *producer_context = NULL, *consumer_context = NULL;
    ID3D11Texture2D *texture = NULL, *opened = NULL, *staging = NULL;
    ID3D11RenderTargetView *target = NULL;
    ID3D11Query *finished = NULL;
    IDXGIResource *resource = NULL;
    IDXGIKeyedMutex *producer_mutex = NULL, *consumer_mutex = NULL;
    D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_11_0, actual;
    D3D11_TEXTURE2D_DESC desc = {0};
    D3D11_MAPPED_SUBRESOURCE mapped = {0};
    D3D11_QUERY_DESC query = {D3D11_QUERY_EVENT, 0};
    HANDLE shared = NULL;
    const float clear[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    const int expected[4] = {64, 128, 191, 255};
    unsigned int x, y, channel, mismatches = 0;
    int producer_owned = 0, consumer_owned = 0, is_mapped = 0;
    BOOL gpu_finished = FALSE;
    ULONGLONG deadline;
    HRESULT hr;

    printf("{\"type\":\"mode\",\"keyed\":%s}\n", keyed ? "true" : "false");
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &requested, 1,
            D3D11_SDK_VERSION, &producer, &actual, &producer_context);
    if (!check("producer.hardware-device", hr == S_OK, (unsigned long)hr)) goto done;
    if (!check("producer.feature-level", actual == requested, actual)) goto done;
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &requested, 1,
            D3D11_SDK_VERSION, &consumer, &actual, &consumer_context);
    if (!check("consumer.hardware-device", hr == S_OK, (unsigned long)hr)) goto done;
    if (!check("consumer.feature-level", actual == requested, actual)) goto done;
    if (!check("separate-device-objects", producer != consumer, 0)) goto done;

    desc.Width = desc.Height = 16;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = keyed ? D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX : D3D11_RESOURCE_MISC_SHARED;
    hr = ID3D11Device_CreateTexture2D(producer, &desc, NULL, &texture);
    if (!check("shared.texture", hr == S_OK, (unsigned long)hr)) goto done;
    hr = ID3D11Texture2D_QueryInterface(texture, &IID_IDXGIResource, (void **)&resource);
    if (!check("shared.dxgi-resource", hr == S_OK, (unsigned long)hr)) goto done;
    hr = IDXGIResource_GetSharedHandle(resource, &shared);
    if (!check("shared.legacy-handle", hr == S_OK && shared != NULL, (unsigned long)hr)) goto done;
    /* This is DXGI's legacy shared handle: do not pass it to CloseHandle. */
    hr = ID3D11Device_OpenSharedResource(consumer, shared, &IID_ID3D11Texture2D, (void **)&opened);
    if (!check("consumer.open-shared-resource", hr == S_OK, (unsigned long)hr)) goto done;
    hr = ID3D11Device_CreateRenderTargetView(producer, (ID3D11Resource *)texture, NULL, &target);
    if (!check("producer.render-target", hr == S_OK, (unsigned long)hr)) goto done;
    if (keyed)
    {
        hr = ID3D11Texture2D_QueryInterface(texture, &IID_IDXGIKeyedMutex, (void **)&producer_mutex);
        if (!check("producer.keyed-interface", hr == S_OK, (unsigned long)hr)) goto done;
        hr = ID3D11Texture2D_QueryInterface(opened, &IID_IDXGIKeyedMutex, (void **)&consumer_mutex);
        if (!check("consumer.keyed-interface", hr == S_OK, (unsigned long)hr)) goto done;
        hr = IDXGIKeyedMutex_ReleaseSync(consumer_mutex, 0);
        if (!check("consumer.unowned-release-rejected", hr == E_FAIL, (unsigned long)hr)) goto done;
        hr = IDXGIKeyedMutex_AcquireSync(consumer_mutex, 7, 10);
        if (hr == S_OK) consumer_owned = 1;
        if (!check("consumer.initial-wrong-key-times-out", hr == WAIT_TIMEOUT, (unsigned long)hr)) goto done;
        hr = IDXGIKeyedMutex_AcquireSync(producer_mutex, 0, 500);
        if (!check("producer.acquire-key0", hr == S_OK, (unsigned long)hr)) goto done;
        producer_owned = 1;
        hr = IDXGIKeyedMutex_AcquireSync(producer_mutex, 0, 0);
        if (!check("producer.recursive-acquire-rejected", hr == E_FAIL, (unsigned long)hr)) goto done;
        hr = IDXGIKeyedMutex_AcquireSync(consumer_mutex, 0, 10);
        if (hr == S_OK) consumer_owned = 1;
        if (!check("consumer.owned-resource-times-out", hr == WAIT_TIMEOUT, (unsigned long)hr)) goto done;
    }
    ID3D11DeviceContext_ClearRenderTargetView(producer_context, target, clear);
    hr = ID3D11Device_CreateQuery(producer, &query, &finished);
    if (!check("producer.gpu-event", hr == S_OK, (unsigned long)hr)) goto done;
    ID3D11DeviceContext_End(producer_context, (ID3D11Asynchronous *)finished);
    ID3D11DeviceContext_Flush(producer_context);
    deadline = GetTickCount64() + 3000;
    do
    {
        hr = ID3D11DeviceContext_GetData(producer_context, (ID3D11Asynchronous *)finished,
                &gpu_finished, sizeof(gpu_finished), 0);
        if (hr != S_FALSE) break;
        Sleep(1);
    } while (GetTickCount64() < deadline);
    if (!check("producer.gpu-complete", hr == S_OK && gpu_finished, (unsigned long)hr)) goto done;
    if (keyed)
    {
        hr = IDXGIKeyedMutex_ReleaseSync(producer_mutex, 1);
        if (!check("producer.release-key1", hr == S_OK, (unsigned long)hr)) goto done;
        producer_owned = 0;
        hr = IDXGIKeyedMutex_AcquireSync(consumer_mutex, 0, 10);
        if (hr == S_OK) consumer_owned = 1;
        if (!check("consumer.stale-key-times-out", hr == WAIT_TIMEOUT, (unsigned long)hr)) goto done;
        hr = IDXGIKeyedMutex_AcquireSync(consumer_mutex, 1, 500);
        /* WAIT_TIMEOUT is nonnegative but is not successful ownership. */
        if (!check("consumer.acquire-key1", hr == S_OK, (unsigned long)hr)) goto done;
        consumer_owned = 1;
    }
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = ID3D11Device_CreateTexture2D(consumer, &desc, NULL, &staging);
    if (!check("consumer.staging-texture", hr == S_OK, (unsigned long)hr)) goto done;
    ID3D11DeviceContext_CopyResource(consumer_context, (ID3D11Resource *)staging, (ID3D11Resource *)opened);
    hr = ID3D11DeviceContext_Map(consumer_context, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &mapped);
    if (!check("consumer.readback-map", hr == S_OK, (unsigned long)hr)) goto done;
    is_mapped = 1;
    if (!check("consumer.row-layout", mapped.pData && mapped.RowPitch >= 64, mapped.RowPitch)) goto done;
    for (y = 0; y < 16; ++y)
        for (x = 0; x < 16; ++x)
            for (channel = 0; channel < 4; ++channel)
            {
                const BYTE *data = (const BYTE *)mapped.pData + y * mapped.RowPitch + x * 4;
                if (abs((int)data[channel] - expected[channel]) > 1) ++mismatches;
            }
    check("consumer.all-rgba-pixels", mismatches == 0, mismatches);
    ID3D11DeviceContext_Unmap(consumer_context, (ID3D11Resource *)staging, 0);
    is_mapped = 0;
    if (keyed)
    {
        hr = IDXGIKeyedMutex_ReleaseSync(consumer_mutex, 2);
        if (!check("consumer.release-key2", hr == S_OK, (unsigned long)hr)) goto done;
        consumer_owned = 0;
        hr = IDXGIKeyedMutex_AcquireSync(producer_mutex, 2, 500);
        if (!check("producer.reacquire-key2", hr == S_OK, (unsigned long)hr)) goto done;
        producer_owned = 1;
        hr = IDXGIKeyedMutex_ReleaseSync(producer_mutex, 0);
        if (!check("producer.final-release", hr == S_OK, (unsigned long)hr)) goto done;
        producer_owned = 0;
    }
    ++modes;
done:
    if (is_mapped) ID3D11DeviceContext_Unmap(consumer_context, (ID3D11Resource *)staging, 0);
    if (consumer_owned) IDXGIKeyedMutex_ReleaseSync(consumer_mutex, 2);
    if (producer_owned) IDXGIKeyedMutex_ReleaseSync(producer_mutex, 0);
    DROP(staging); DROP(target); DROP(finished); DROP(consumer_mutex); DROP(producer_mutex);
    DROP(opened); DROP(resource); DROP(texture);
    DROP(consumer_context); DROP(producer_context); DROP(consumer); DROP(producer);
}

int main(void)
{
    HANDLE timer = CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    if (!timer) return 125;
    CloseHandle(timer);
    printf("{\"type\":\"begin\",\"probe\":\"d3d11-shared-offscreen\",\"pid\":%lu,\"pointer_bits\":%u}\n",
            GetCurrentProcessId(), (unsigned int)(8 * sizeof(void *)));
    shared_mode(0);
    shared_mode(1);
    check("both-sharing-modes", modes == 2, modes);
    printf("{\"type\":\"complete\",\"ok\":%s,\"checks\":%u,\"failures\":%u,\"modes\":%u,\"pixels_per_mode\":256}\n",
            failures ? "false" : "true", checks, failures, modes);
    fflush(stdout);
    return failures ? 1 : 0;
}
