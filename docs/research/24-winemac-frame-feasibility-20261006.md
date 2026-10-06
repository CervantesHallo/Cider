# L5：winemac 标题栏与跨进程几何可行性（2026-10-06）

后续独立 GDI 实验、候选补丁及尚缺的验收见 [第一批实现证据](25-feasibility-implementation-20261006.md)。下文保留静态研究阶段的结论，不能替代动态验证。

结论：L5 的视觉现象不能证明应删除原生标题栏。静态源码支持三类候选；启动器稳态矩形尚缺，以下修复均为提案。本次只读源码与官方资料，未运行/关闭 Wine、启动器，未操作瓶子、游戏或反作弊。背景见 `docs/plan/13-roadmap.md:65–68`；创建期三框相等不能替代稳态证据。

行号基于本机 `/Users/cervantes/Library/Caches/Cider/engine-build/cider-cx26.3-r1-x86_64/src/`：W=`dlls/winemac.drv/window.c`，C=`dlls/winemac.drv/cocoa_window.m`，U=`dlls/win32u/window.c`，H=`dlls/winemac.drv/macdrv_cocoa.h`。P=`/Users/cervantes/Cider/engine/patches/highball/0007-winemac-cross-process-child-swapchains.patch`。

## 坐标链与候选

[Microsoft WM_NCCALCSIZE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-nccalcsize) 明确：TRUE 时不改矩形、返回 0，可让 client 覆盖整窗；WS_CAPTION 保留本身不违法。[官方示例](https://learn.microsoft.com/en-us/windows/win32/dwm/customframe) 要求用 SWP_FRAMECHANGED 触发重算。U:3724–3748 将回调后的 rgrc[0] 写入 client；U:5992–6006 的创建消息则是 FALSE。

令根框为 window=W₀、client=C₀、visible=V₀。U:2068–2077 先取非空 present，再检查 window==client，否则按样式扣默认非客户区；W:121 已有 window==visible 时不加宿主装饰的出口。W:812–822 把 visible 交给 Cocoa；C:1175、2123 把它当 contentRect，再向外生成 NSWindow frame。故“标题栏压住”也可能是内容原点落在 content 外被裁。

普通 surface：子 client → 根 client 坐标 → 加 C₀.origin−V₀.origin（W:1207–1213）。远程路径的源 layer bounds 是子 client 尺寸（W:1291–1297），宿主却取子 window 屏幕框，映射根 client 后加同一偏移（W:1777–1784；P:775–788）。公式：
`F = Map(child矩形→root client) + C₀.origin−V₀.origin`。
C:4412/4435、H:151–159 仅在入口将 Retina 像素除二；C:563–566 已翻转 view，无须再翻 y。C:793–795 设置宿主 frame 并裁剪；C:510–530 的边缘吸附还会截断越界右/底边。

| 候选 | 源码证据与证伪条件 |
|---|---|
| ① 部分自绘非客户区（优先） | 若 C₀.top==W₀.top，但侧/底仍有内缩，U:2069 的整框相等失败，样式计算仍扣 caption，可能令 C₀.top−V₀.top<0。纯 GDI 同样丢顶行即可排除 Highball 是必要原因；若 client 保留完整默认顶边、V₀ 包含 client，此解释不成立。 |
| ② Highball 几何不等价 | 远程用 window，普通用 client；子窗口有边框时位置/尺寸可错。无边框且子 window==client 时，此差异不能解释 L5。另 root==hwnd 传空 child，宿主铺满 content（W:1296、1898；C:780–784），未显式保持 client 偏移。 |
| ③ style-mask 未初始化 | W:2071 写的是 `*style_mask = ex_style = 0`，未清 `*ex_style_mask`；U:2065 的该局部量也未初始化，随后参与按位运算。属于确定缺陷，非 Highball 引入；ex_style==0 时不能解释本次裁切。[Wine 11.11 原码](https://raw.githubusercontent.com/wine-mirror/wine/wine-11.11/dlls/winemac.drv/window.c) 同样存在（1494）。 |

present 不是普通 Present 的目标框：U:6135–6145 只读本进程状态；`dlls/win32u/d3dkmt.c:551–562` 写入它，`dlls/wined3d/swapchain.c:2312/2357` 在全屏设置/恢复时设置/清空。须记录根进程值；非空时整框相等出口会被提前跳过。

[上游原始提交 1a63b0d7c431](https://github.com/wine-mirror/wine/commit/1a63b0d7c431.patch) 的 window.c hunk 明确拒绝跨进程子窗口，Cocoa host 铺满 content；它证明 CAContext 通路存在，不能证明 Highball 子几何正确（P:12–17、771–788）。

## 决定性复现与验收（待执行）

先在独立临时环境做 helper：800×600、保留 WS_OVERLAPPEDWINDOW；TRUE NCCALCSIZE 分别①完全不改、②左右/底内缩8而顶为0、③DefWindowProc；创建后 SWP_FRAMECHANGED。画 client 第0行红线及每8像素刻度、顶部点击计数，分别用 GDI、同进程 DXMT、第二进程 WS_CHILD DXMT；子位置(11,13)、尺寸301×181，切换有/无 WS_BORDER。

从首帧后连续三次、间隔1秒取同一 HWND/PID 的 window/client/visible/present、样式、DPI、retina_on、context ID、源 bounds、host frame/content bounds；client 须映射到屏幕、统一根 MDT_RAW_DPI 再比较。现有 +macdrv,+win,+d3dkmt 日志应补齐缺失字段；不能只看创建 trace。启动器取样留给主代理另行安排，勿把 L6 的旧实例当重启。

若①在 GDI 重现，修根装饰契约；若 GDI/同进程正常而远程错，修②。验收要求顶行完整、点击与绘制误差≤1设备像素；移动、奇偶尺寸缩放、最大化/还原、最小化恢复、1×/2×均通过。

## 最小修复边界

先独立修③为 `*style_mask = *ex_style_mask = 0`。若①证实，同时修 U:get_visible_rect 与 W:get_cocoa_window_features：依据本次 NCCALCSIZE 后矩形一致决定宿主 caption，扣除量不得越过 client；保留缩放/命中测试，避免旧缓存和固定标题栏高度。若②证实，只将远程框统一到映射后的 client，并处理空 child 的 client 偏移及更新；不借删根标题栏掩盖误差。

仅改标题栏开关仍不够：C:75–83 的 borderless 分支不带宿主 Resizable 标志；W:2146–2167 在 frame/features 更新后才放置 host。实现需证明装饰切换后的 content 原点与边界正确，且自绘边缘仍可拖动缩放；不能用截图变好代替这一坐标契约。

回归覆盖普通有标题窗、部分/完全自绘窗、菜单/tool/layered/shaped 窗、CEF 嵌套兄弟层的裁剪/Z序、隐藏/0×0/重建、全屏 present 清空。收益是通用坐标保真；代价是装饰或远程几何回归，分别以独立补丁撤回，不按应用名分支。
