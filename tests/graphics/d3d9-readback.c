/* Bounded D3D9 HAL clear/readback check for a newly ported wined3d module.
 * Hidden owned window; not a game, shader suite, backend-fallback or FPS proof.
 * Run only in the renderer-smoke suite, which drains the owned runtime. */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
static int check(const char *name,int ok,long actual,long expected)
{
    ++checks;failures+=!ok;
    printf("{\"type\":\"check\",\"name\":\"%s\",\"ok\":%s,\"actual\":%ld,\"expected\":%ld}\n",name,ok?"true":"false",actual,expected);
    if(fflush(stdout)||ferror(stdout)){TerminateProcess(GetCurrentProcess(),120);ExitProcess(120);}
    return ok;
}
int main(void)
{
    const UINT width=64,height=64;const DWORD color=0x002589c7;
    WNDCLASSW wc={0};ATOM atom=0;HWND window=NULL;HINSTANCE instance=GetModuleHandleW(NULL);
    IDirect3D9 *d3d=NULL;IDirect3DDevice9 *device=NULL;IDirect3DSurface9 *target=NULL,*copy=NULL;
    D3DPRESENT_PARAMETERS parameters={0};D3DSURFACE_DESC desc;D3DLOCKED_RECT locked;
    D3DADAPTER_IDENTIFIER9 identifier;HRESULT hr;unsigned x,y,matched=0;BOOL is_locked=FALSE;
    int64_t absolute_pitch;
    printf("{\"type\":\"begin\",\"mode\":\"correctness\",\"subject\":\"d3d9-readback\",\"pointer_bits\":%u,\"pid\":%lu}\n",(unsigned)(8*sizeof(void*)),GetCurrentProcessId());
    if(fflush(stdout)||ferror(stdout))return 2;
    wc.lpfnWndProc=DefWindowProcW;wc.hInstance=instance;wc.lpszClassName=L"Cider.D3D9Probe";
    atom=RegisterClassW(&wc);
    if(!check("register_class",atom!=0,atom!=0,1))goto done;
    window=CreateWindowW(wc.lpszClassName,L"Cider renderer probe",WS_OVERLAPPEDWINDOW,0,0,128,128,NULL,NULL,instance,NULL);
    if(!check("create_window",window!=NULL,window!=NULL,1))goto done;
    d3d=Direct3DCreate9(D3D_SDK_VERSION);
    if(!check("d3d9_create",d3d!=NULL,d3d!=NULL,1))goto done;
    hr=IDirect3D9_GetAdapterIdentifier(d3d,D3DADAPTER_DEFAULT,0,&identifier);
    if(!check("adapter_identifier",SUCCEEDED(hr),hr,S_OK))goto done;
    printf("{\"type\":\"adapter\",\"vendor_id\":%lu,\"device_id\":%lu,\"default_choice_not_backend_detection\":true}\n",identifier.VendorId,identifier.DeviceId);
    parameters.BackBufferWidth=width;parameters.BackBufferHeight=height;
    parameters.BackBufferFormat=D3DFMT_X8R8G8B8;parameters.BackBufferCount=1;
    parameters.SwapEffect=D3DSWAPEFFECT_COPY;parameters.Windowed=TRUE;parameters.hDeviceWindow=window;
    hr=IDirect3D9_CreateDevice(d3d,D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&parameters,&device);
    if(!check("create_hal_device",SUCCEEDED(hr)&&device!=NULL,hr,S_OK))goto done;
    hr=IDirect3DDevice9_Clear(device,0,NULL,D3DCLEAR_TARGET,0xff000000|color,1.0f,0);
    if(!check("clear",SUCCEEDED(hr),hr,S_OK))goto done;
    hr=IDirect3DDevice9_GetRenderTarget(device,0,&target);
    if(!check("get_render_target",SUCCEEDED(hr)&&target!=NULL,hr,S_OK))goto done;
    hr=IDirect3DSurface9_GetDesc(target,&desc);
    if(!check("surface_layout",SUCCEEDED(hr)&&desc.Width==width&&desc.Height==height&&
        (desc.Format==D3DFMT_X8R8G8B8||desc.Format==D3DFMT_A8R8G8B8),hr,S_OK))goto done;
    hr=IDirect3DDevice9_CreateOffscreenPlainSurface(device,width,height,desc.Format,D3DPOOL_SYSTEMMEM,&copy,NULL);
    if(!check("create_system_copy",SUCCEEDED(hr)&&copy!=NULL,hr,S_OK))goto done;
    hr=IDirect3DDevice9_GetRenderTargetData(device,target,copy);
    if(!check("render_target_readback",SUCCEEDED(hr),hr,S_OK))goto done;
    hr=IDirect3DSurface9_LockRect(copy,&locked,NULL,D3DLOCK_READONLY);
    if(!check("lock_copy",SUCCEEDED(hr),hr,S_OK))goto done;
    is_locked=TRUE;
    absolute_pitch=locked.Pitch<0?-(int64_t)locked.Pitch:(int64_t)locked.Pitch;
    if(!check("row_layout",locked.pBits!=NULL&&absolute_pitch>=4*(int64_t)width&&
        absolute_pitch<=PTRDIFF_MAX/(height-1),locked.Pitch,4*width))goto done;
    for(y=0;y<height;++y)for(x=0;x<width;++x)
    {
        uint32_t pixel;const unsigned char *row=(const unsigned char*)locked.pBits+(ptrdiff_t)y*locked.Pitch;
        memcpy(&pixel,row+x*4,sizeof(pixel));if((pixel&0x00ffffff)==color)++matched;
    }
    check("exact_rgb_pixels",matched==width*height,matched,width*height);
    hr=IDirect3DSurface9_UnlockRect(copy);if(SUCCEEDED(hr))is_locked=FALSE;
    check("unlock_copy",SUCCEEDED(hr),hr,S_OK);
 done:
    if(is_locked)IDirect3DSurface9_UnlockRect(copy);
    if(copy)IDirect3DSurface9_Release(copy);
    if(target)IDirect3DSurface9_Release(target);
    if(device)IDirect3DDevice9_Release(device);
    if(d3d)IDirect3D9_Release(d3d);
    if(window)check("destroy_window",DestroyWindow(window)!=FALSE,0,0);
    if(atom)check("unregister_class",UnregisterClassW(wc.lpszClassName,instance)!=FALSE,0,0);
    printf("{\"type\":\"complete\",\"mode\":\"correctness\",\"ok\":%s,\"checks\":%u,\"failures\":%u,\"output_error\":%s}\n",failures?"false":"true",checks,failures,ferror(stdout)?"true":"false");
    return failures||fflush(stdout)||ferror(stdout)?1:0;
}
