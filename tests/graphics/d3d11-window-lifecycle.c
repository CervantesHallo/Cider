/* Engine R v1 targeted window lifecycle exercise; run only in an owned lab bottle.
 * This is resource/Present/readback evidence, not a screen or performance oracle. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks, failures, windows_done, swaps_done, presents;
static void check(const char *label, BOOL passed)
{
    ++checks;
    if (!passed) { ++failures; printf("window-lifecycle-fail check=%u label=%s\n", checks, label); }
}
static void pump(void)
{
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}
static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return DefWindowProcA(hwnd, msg, wp, lp);
}
static BOOL clear_readback_present(ID3D11Device *device, ID3D11DeviceContext *context,
                                   IDXGISwapChain *swap, UINT width, UINT height, unsigned int phase)
{
    const BYTE expected[3][4] = {{255, 0, 255, 255}, {255, 255, 0, 255}, {0, 255, 255, 255}};
    float color[4];
    ID3D11Texture2D *back = NULL, *staging = NULL;
    ID3D11RenderTargetView *view = NULL;
    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr;
    BOOL ok = FALSE;
    unsigned int x, y;
    for (x = 0; x < 4; ++x) color[x] = expected[phase][x] / 255.0f;
    hr = IDXGISwapChain_GetBuffer(swap, 0, &IID_ID3D11Texture2D, (void **)&back);
    check("backbuffer", SUCCEEDED(hr) && back);
    if (FAILED(hr) || !back) goto done;
    ID3D11Texture2D_GetDesc(back, &desc);
    ok = desc.Width == width && desc.Height == height && desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM &&
         desc.SampleDesc.Count == 1 && desc.SampleDesc.Quality == 0 && desc.ArraySize == 1 && desc.MipLevels == 1;
    check("phase-buffer-layout", ok);
    if (!ok) goto done;
    ok = FALSE;
    hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)back, NULL, &view);
    check("render-target", SUCCEEDED(hr) && view);
    if (FAILED(hr) || !view) goto done;
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
    hr = ID3D11Device_CreateTexture2D(device, &desc, NULL, &staging);
    check("staging", SUCCEEDED(hr) && staging);
    if (FAILED(hr) || !staging) goto done;
    ID3D11DeviceContext_ClearRenderTargetView(context, view, color);
    ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)staging, (ID3D11Resource *)back);
    hr = ID3D11DeviceContext_Map(context, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &mapped);
    check("readback-map", SUCCEEDED(hr));
    if (FAILED(hr)) goto done;
    ok = mapped.pData && mapped.RowPitch >= desc.Width * 4;
    if (ok)
        for (y = 0; y < desc.Height; ++y)
            for (x = 0; x < desc.Width; ++x)
            {
                const BYTE *pixel = (const BYTE *)mapped.pData + y * mapped.RowPitch + 4 * x;
                if (memcmp(pixel, expected[phase], 4)) ok = FALSE;
            }
    ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)staging, 0);
    check("all-readback-pixels", ok);
    if (!ok) goto done;
    hr = IDXGISwapChain_Present(swap, 0, 0);
    check("present", hr == S_OK);
    ok = hr == S_OK;
    if (ok) ++presents;
done:
    if (staging) ID3D11Texture2D_Release(staging);
    if (view) ID3D11RenderTargetView_Release(view);
    if (back) ID3D11Texture2D_Release(back);
    return ok;
}
int main(void)
{
    WNDCLASSA wc = {0};
    ID3D11Device *device = NULL;
    ID3D11DeviceContext *context = NULL;
    IDXGIDevice *dxgi_device = NULL;
    IDXGIAdapter *adapter = NULL;
    IDXGIFactory *factory = NULL;
    D3D_FEATURE_LEVEL level;
    HRESULT hr;
    unsigned int w, s;
    wc.lpfnWndProc = window_proc; wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "Cider-window-lifecycle";
    check("register-class", RegisterClassA(&wc) != 0);
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
                          D3D11_SDK_VERSION, &device, &level, &context);
    check("device", SUCCEEDED(hr) && device && context && level >= D3D_FEATURE_LEVEL_11_0);
    if (FAILED(hr) || !device || !context) goto done;
    hr = ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void **)&dxgi_device);
    check("dxgi-device", SUCCEEDED(hr) && dxgi_device);
    if (FAILED(hr) || !dxgi_device) goto done;
    hr = IDXGIDevice_GetAdapter(dxgi_device, &adapter);
    check("adapter", SUCCEEDED(hr) && adapter);
    if (FAILED(hr) || !adapter) goto done;
    hr = IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory, (void **)&factory);
    check("factory", SUCCEEDED(hr) && factory);
    if (FAILED(hr) || !factory) goto done;
    for (w = 0; w < 4; ++w)
    {
        HWND hwnd = CreateWindowA(wc.lpszClassName, "Cider owned lifecycle exercise",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE, 160, 160, 400, 300,
                                  NULL, NULL, wc.hInstance, NULL);
        check("create-window", hwnd != NULL);
        if (!hwnd) break;
        pump();
        for (s = 0; s < 14; ++s)
        {
            DXGI_SWAP_CHAIN_DESC desc = {0};
            IDXGISwapChain *swap = NULL;
            BOOL rendered;
            desc.BufferCount = 2; desc.BufferDesc.Width = 96; desc.BufferDesc.Height = 64;
            desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.OutputWindow = hwnd;
            desc.SampleDesc.Count = 1; desc.Windowed = TRUE; desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            hr = IDXGIFactory_CreateSwapChain(factory, (IUnknown *)device, &desc, &swap);
            check("create-swapchain", SUCCEEDED(hr) && swap);
            if (FAILED(hr) || !swap) break;
            pump();
            rendered = clear_readback_present(device, context, swap, 96, 64, 0);
            ID3D11DeviceContext_ClearState(context);
            ID3D11DeviceContext_Flush(context);
            hr = IDXGISwapChain_ResizeBuffers(swap, 2, 128, 80, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
            check("resize-buffers", hr == S_OK);
            if (hr == S_OK && rendered) rendered = clear_readback_present(device, context, swap, 128, 80, 1);
            check("move-window", SetWindowPos(hwnd, NULL, 160 + 3 * s, 160 + 2 * s,
                                                400 + s, 300 + s, SWP_NOACTIVATE | SWP_NOZORDER));
            ShowWindow(hwnd, SW_HIDE); pump();
            ShowWindow(hwnd, SW_SHOWNOACTIVATE); pump();
            check("shown-again", IsWindowVisible(hwnd));
            if (rendered) clear_readback_present(device, context, swap, 128, 80, 2);
            ID3D11DeviceContext_ClearState(context);
            ID3D11DeviceContext_Flush(context);
            if ((w & 1) && s == 13)
            {
                check("destroy-before-renderer-release", DestroyWindow(hwnd));
                hwnd = NULL; pump();
            }
            IDXGISwapChain_Release(swap);
            /* D3D11 destruction is deferred; flush after the final Release
             * before replacing a flip-model chain on the same HWND. */
            ID3D11DeviceContext_ClearState(context);
            ID3D11DeviceContext_Flush(context);
            pump();
            ++swaps_done;
        }
        if (hwnd) { check("destroy-after-renderer-release", DestroyWindow(hwnd)); pump(); }
        ++windows_done;
    }
done:
    if (factory) IDXGIFactory_Release(factory);
    if (adapter) IDXGIAdapter_Release(adapter);
    if (dxgi_device) IDXGIDevice_Release(dxgi_device);
    if (context) { ID3D11DeviceContext_ClearState(context); ID3D11DeviceContext_Flush(context); ID3D11DeviceContext_Release(context); }
    if (device) ID3D11Device_Release(device);
    check("unregister-class", UnregisterClassA(wc.lpszClassName, wc.hInstance));
    check("complete-work", windows_done == 4 && swaps_done == 56 && presents == 168);
    printf("{\"type\":\"window-lifecycle\",\"bits\":%u,\"checks\":%u,\"failures\":%u,\"windows\":%u,\"swapchains\":%u,\"presents\":%u}\n",
           (unsigned int)(8 * sizeof(void *)), checks, failures, windows_done, swaps_done, presents);
    return failures ? 1 : 0;
}
