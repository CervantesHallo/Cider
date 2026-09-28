/* Minimal Direct3D 11 program for engine smoke tests (docs/plan/04, milestone M-A "DXMT sample"): opens a
 * window, creates a device and swap chain, compiles shaders at run time, draws a triangle for N frames and
 * prints the feature level and frame rate. Exit code 0 when every Present succeeded.
 * Build: x86_64-w64-mingw32-gcc -O2 -o d3d11-triangle.exe d3d11-triangle.c -ld3d11 -ld3dcompiler_47 -ldxgi -luuid -mwindows */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>

static const char shader[] =
    "struct V { float4 pos : SV_Position; float3 col : COLOR; };\n"
    "V vs(uint id : SV_VertexID) {\n"
    "    const float2 p[3] = { float2(0, 0.7), float2(0.7, -0.6), float2(-0.7, -0.6) };\n"
    "    const float3 c[3] = { float3(1, 0.45, 0.2), float3(0.3, 0.9, 0.6), float3(0.4, 0.5, 1) };\n"
    "    V v; v.pos = float4(p[id], 0, 1); v.col = c[id]; return v; }\n"
    "float4 ps(V v) : SV_Target { return float4(v.col, 1); }\n";

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int main(int argc, char **argv)
{
    int frames = argc > 1 ? atoi(argv[1]) : 300;
    WNDCLASSA wc = { 0 };
    DXGI_SWAP_CHAIN_DESC sd = { 0 };
    ID3D11Device *device; ID3D11DeviceContext *ctx; IDXGISwapChain *swap;
    ID3D11Texture2D *back; ID3D11RenderTargetView *rtv;
    ID3DBlob *vsb, *psb, *err = NULL;
    ID3D11VertexShader *vs; ID3D11PixelShader *ps;
    D3D_FEATURE_LEVEL level;
    LARGE_INTEGER freq, t0, t1;
    const float clear[4] = { 0.10f, 0.09f, 0.08f, 1 };
    D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0, 1 };
    HRESULT hr;
    int i, failed = 0;
    HWND hwnd;

    wc.lpfnWndProc = wndproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "cider-d3d11";
    RegisterClassA(&wc);
    hwnd = CreateWindowA("cider-d3d11", "Cider D3D11 test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                         100, 100, 816, 639, NULL, NULL, wc.hInstance, NULL);

    sd.BufferCount = 2; sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION,
                                       &sd, &swap, &device, &level, &ctx);
    printf("D3D11CreateDeviceAndSwapChain: %#lx, feature level %#x\n", hr, level);
    if (FAILED(hr)) return 2;

    IDXGISwapChain_GetBuffer(swap, 0, &IID_ID3D11Texture2D, (void **)&back);
    ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)back, NULL, &rtv);
    if (FAILED(D3DCompile(shader, sizeof(shader) - 1, NULL, NULL, NULL, "vs", "vs_5_0", 0, 0, &vsb, &err))
        || FAILED(D3DCompile(shader, sizeof(shader) - 1, NULL, NULL, NULL, "ps", "ps_5_0", 0, 0, &psb, &err)))
    {
        printf("shader compile failed: %s\n", err ? (char *)ID3D10Blob_GetBufferPointer(err) : "?");
        return 3;
    }
    ID3D11Device_CreateVertexShader(device, ID3D10Blob_GetBufferPointer(vsb), ID3D10Blob_GetBufferSize(vsb), NULL, &vs);
    ID3D11Device_CreatePixelShader(device, ID3D10Blob_GetBufferPointer(psb), ID3D10Blob_GetBufferSize(psb), NULL, &ps);

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    for (i = 0; i < frames; i++)
    {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL);
        ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp);
        ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear);
        ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11DeviceContext_VSSetShader(ctx, vs, NULL, 0);
        ID3D11DeviceContext_PSSetShader(ctx, ps, NULL, 0);
        ID3D11DeviceContext_Draw(ctx, 3, 0);
        if (FAILED(hr = IDXGISwapChain_Present(swap, 1, 0))) { printf("Present failed: %#lx\n", hr); failed++; }
    }
    QueryPerformanceCounter(&t1);
    printf("%d frames, %.1f fps, %d failed presents\n", frames,
           frames / ((double)(t1.QuadPart - t0.QuadPart) / freq.QuadPart), failed);
    return failed ? 4 : 0;
}
