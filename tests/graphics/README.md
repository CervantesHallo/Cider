# 限定图形回归

`d3d11-shared-offscreen.c` 不创建窗口或 swapchain。它用两个硬件 D3D11 设备共享 16×16 纹理，检查普通共享和 keyed mutex、GPU 完成、全部 RGBA 像素，以及错误 key/递归获取/未持有释放/所有权交回。硬件不可用、超时或接口缺失均为失败，不跳过。

使用现有 MinGW 对两种 PE 编译，例如：

```sh
x86_64-w64-mingw32-gcc -O2 -g -Wall -Wextra -Werror \
  tests/graphics/d3d11-shared-offscreen.c -o shared-x64.exe -ld3d11 -ldxgi
i686-w64-mingw32-gcc -O2 -g -Wall -Wextra -Werror \
  tests/graphics/d3d11-shared-offscreen.c -o shared-x86.exe -ld3d11 -ldxgi
```

只在自有实验引擎和瓶子中通过 `ciderctl run --wait` 执行，记录实际组件/EXE 指纹、完整日志及清理。GPU event 在交接前等待完成，因此不计 FPS 或无同步性能。legacy shared handle 不 CloseHandle；AcquireSync 要精确判断 S_OK 和 WAIT_TIMEOUT。

该探针通过不代表 MF、跨进程窗口、CEF、游戏或 Windows 原生对照通过。关联证据及明确失败见研究 93。
