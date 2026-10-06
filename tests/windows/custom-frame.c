/* Independent Win32 fixture for L5. No launcher/game dependencies.
 * Keeps WS_CAPTION while replacing WM_NCCALCSIZE, as permitted by Win32.
 * Build with x86_64-w64-mingw32-gcc custom-frame.c -o custom-frame.exe -lgdi32
 * Run in an isolated 64-bit bottle. It closes itself after 45 seconds.
 */
#include <windows.h>
#include <stdio.h>

static const char *names[] = { "L5 default frame", "L5 full client", "L5 partial frame" };
static int windows_left = 3;

static void snapshot(HWND hwnd, int mode, int tick)
{
    RECT window, client;
    GetWindowRect(hwnd, &window);
    GetClientRect(hwnd, &client);
    MapWindowPoints(hwnd, NULL, (POINT *)&client, 2);
    printf("L5 mode=%d tick=%d hwnd=%p style=%08lx ex=%08lx window=(%ld,%ld)-(%ld,%ld) client=(%ld,%ld)-(%ld,%ld)\n",
           mode, tick, hwnd, (unsigned long)GetWindowLongW(hwnd, GWL_STYLE),
           (unsigned long)GetWindowLongW(hwnd, GWL_EXSTYLE),
           window.left, window.top, window.right, window.bottom,
           client.left, client.top, client.right, client.bottom);
    fflush(stdout);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    int mode = (int)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg)
    {
    case WM_NCCREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCT *)lparam)->lpCreateParams);
        break;
    case WM_NCCALCSIZE:
        if (wparam && mode)
        {
            RECT *r = &((NCCALCSIZE_PARAMS *)lparam)->rgrc[0];
            if (mode == 2) { r->left += 8; r->right -= 8; r->bottom -= 8; }
            return 0;
        }
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        RECT client;
        HDC dc = BeginPaint(hwnd, &paint);
        GetClientRect(hwnd, &client);
        FillRect(dc, &client, (HBRUSH)GetStockObject(WHITE_BRUSH));
        HBRUSH red = CreateSolidBrush(RGB(240, 30, 30));
        RECT band = { 0, 0, client.right, 8 };
        FillRect(dc, &band, red);
        DeleteObject(red);
        SetBkMode(dc, TRANSPARENT);
        TextOutA(dc, 16, 10, "CLIENT TOP: red band must be visible", 36);
        for (int y = 32; y < client.bottom; y += 16)
        {
            char label[32];
            int len = snprintf(label, sizeof(label), "client y = %d", y);
            TextOutA(dc, 16, y, label, len);
            MoveToEx(dc, 0, y, NULL); LineTo(dc, 8, y);
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_LBUTTONDOWN:
        printf("L5 click mode=%d client=(%d,%d)\n", mode, (short)LOWORD(lparam), (short)HIWORD(lparam));
        fflush(stdout);
        return 0;
    case WM_TIMER:
        snapshot(hwnd, mode, (int)GetTickCount());
        return 0;
    case WM_DESTROY:
        if (!--windows_left) PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

int main(void)
{
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = "CiderCustomFrameFixture";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassA(&wc)) return 1;
    HWND windows[3];
    for (int mode = 0; mode < 3; mode++)
    {
        windows[mode] = CreateWindowA(wc.lpszClassName, names[mode], WS_OVERLAPPEDWINDOW,
                                      40 + mode * 360, 100, 320, 280, NULL, NULL, wc.hInstance, (void *)(LONG_PTR)mode);
        if (!windows[mode]) return 2;
        SetWindowPos(windows[mode], NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowWindow(windows[mode], SW_SHOW);
        UpdateWindow(windows[mode]);
        SetTimer(windows[mode], 1, 1000, NULL);
    }
    DWORD start = GetTickCount();
    MSG msg;
    while (GetTickCount() - start < 45000)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) return 0;
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        Sleep(20);
    }
    for (int mode = 0; mode < 3; mode++) if (IsWindow(windows[mode])) DestroyWindow(windows[mode]);
    return 0;
}
