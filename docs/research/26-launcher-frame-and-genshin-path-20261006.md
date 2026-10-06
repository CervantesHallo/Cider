# 启动器顶部裁切与原神安装诊断（2026-10-06）

接续 [research/25](25-feasibility-implementation-20261006.md)。本轮修复安装到本机米哈游瓶子；它是本地 r2 候选，尚未发布 GitHub Release，下载索引仍为 r1。没有改变游戏文件或反作弊，没有解除启动门禁。

## 1 真实启动器的顶部裁切

环境：Apple M3 / 8 GB、macOS 26.7、米哈游启动器 1.18.0.380、`bottle-ca8f`、zh_CN.UTF-8、瓶子级 msync。只开启有界会话的 `trace+macdrv`，未读取整个历史 30.76 GB 日志。

| 证据 | r1（20261006T143601-launcher） | r2（20261006T144215-launcher） |
|---|---|---|
| 主窗口 style | 0x96ce0000 | 0x96ce0000 |
| window | (112,82)–(1400,806) | (112,82)–(1400,806) |
| client | (116,82)–(1396,802) | (116,82)–(1396,802) |
| visible / 传给 Cocoa 的 content frame | (116,112)–(1396,802) | (116,82)–(1396,802) |
| 跨进程 DXMT layer | 1280×720 | 1280×720，根内位置 (0,0) |

真实应用明确具有部分自绘非客户区：client.top == window.top，而左右和底部仍保留边框。原默认 caption 裁切令 client.top − visible.top = −30。0004 修正可见框；0005 根据实际客户区撤去重复的 Cocoa caption，保留 Windows 样式与宿主缩放标志。随后 `20261006T144554-launcher` 的原生 `macdrv_window_frame_changed` 也读回 (116,82)–(1396,802)，宿主 content frame 与 Wine visible 一致。

Microsoft 的 [WM_NCCALCSIZE 契约](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-nccalcsize)允许程序自行决定客户区；不能仅按 WS_CAPTION 从客户区扣掉默认标题栏。

### 补丁与审查

- 保留 0003 的 style/ex-style mask 初始化修复。
- 保留 0004 的客户区与窗口内部可见框合并。
- 新增 `0005-winemac-sync-custom-caption-geometry.patch`：非空客户区顶边到达窗口顶边时撤去重复的宿主 caption；borderless 分支保留 resizable；先设置 Cocoa 装饰，再同步 content frame；原生 frame event 中若四边 visible/window 内缩变化，则重新同步。内缩不变仍不回送 native frame event，避免无条件回送造成反馈循环。
- 静态对抗性审查检查：普通 caption 客户区不触发去 caption；shaped/no-activate 等原有标志继续沿用；只改变坐标映射及宿主装饰；Highball 子层在宿主 frame 更新后定位。没有按应用名硬编码或按固定 30 px 平移画面。
- 之前 P2 的动态 NC 原点同步缺口已在代码中补齐；动态用户缩放/点击与 1×/2× 的完整验收尚未完成，不能将其记为已经实测通过。

本机引擎：`cider-cx26.3-r2-x86_64`，CX26.3 + Highball + Cider 0001–0006。仅本地增量构建 native 模块，macOS15.4 SDK；其余文件来自已安装 r1 的 APFS 克隆。来源及补丁/模块 SHA-256 保存在引擎的 `local-build.json`。这是诚实记录的增量候选，不是从零全量构建或完整发布验收。

切换前已保存快照 `20261006T144050`，引擎历史记录 r1 → r2；原 r1 引擎仍可回退。

### 窗口自动化的具体限制

原 `EngineHost` 链接的是 `bin/wine` bootstrap，实际进程又执行 `lib/wine/x86_64-unix/wine`，离开 `.app`。本轮改为依据 cpu_backend 选择真实 Unix loader，附相对 `ntdll.so` 链接；0006 只在宿主 loader 与本架构真实 loader 的 device/inode 相同时保留 WINELOADER，且 bundle loader 不转移到 winetemp。

实机主/辅助 PID 的 proc_pidpath 已位于 `CiderWineHost.app/Contents/MacOS/wine`，自动化 inventory 能列出运行中的 `org.cider.winehost`。但是按 bundle/path 读取 AX 或截图仍返回 `timeoutReached`；多引擎相同 bundle ID 也有歧义。故 L4 只记部分完成。未声称取得启动器截图、顶部点击或完整视觉验收；后续须按 ADR-007 收敛应用身份与自动化寻址。

## 2 原神“找不到文件”诊断

原神目录：`C:\Program Files\miHoYo Launcher\games\Genshin Impact Game`。注册表 `Software\miHoYo\HYP\1_1\hk4e_cn` 的 GameInstallPath 与实际目录一致，config.ini 为 7.1.0 / 国服渠道 1_1。

| 本地安装清单 | 项数 | 缺失 | 大小不符 |
|---|---:|---:|---:|
| pkg_version | 2743 | 0 | 0 |
| beyond_pkg_version | 4858 | 0 | 0 |
| Audio_Chinese_pkg_version | 176 | 0 | 0 |
| 合计 | 7777 | 0 | 0 |

`YuanShen.exe` 为 445,006,744 字节，MD5 与本地清单一致。没有对全部资产做哈希校验，也没有将本地清单当成官方远程签名证明。Wine 的目录查询能列出该文件，独立只读 cmd 批处理报告 `CIDER_DIAG_GAME_EXE_FOUND`、`CIDER_DIAG_METADATA_FOUND`，会话 `20261006T145006-cmd`；确认关键 Windows 路径可访问。

注意诊断命令的引号：把含引号的整条 IF 命令作为一个 argv 传给 Wine/cmd 时，本轮第一次查询错误地报告 MISSING；分参数 DIR 和 cmd 批处理复查均 FOUND。不能用这个命令构造问题推断游戏丢失。

启动器日志中的 `app.replace.req.dat` 找不到，属于启动器热替换请求文件，不是 YuanShen.exe；未据此修改安装路径或创建空游戏文件。

### 已确认的启动障碍

Cider 当前在两个位置拒绝原神：WineRunner 的宿主 preflight，以及引擎 0001 的 NtCreateUserProcess 策略。会话环境仍含 `CIDER_PREFLIGHT_DENY` 的 yuanshen.exe 项；直接通过 ciderctl 请求原神立即收到 Cider 的内核兼容未验证提示，没有 spawn 游戏。

引擎返回 STATUS_ACCESS_DISABLED_BY_POLICY_DEFAULT，对应 Win32 1260（[Microsoft 错误码](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--1000-1299-)）。这是策略拒绝，不能把它记成 ERROR_FILE_NOT_FOUND。用户当时的原错误弹窗/实际返回码尚未复现，不能断言启动器把 1260 翻译成了“找不到游戏文件”。可确定的是：当前安装关键路径可访问，但现有 Cider 门禁会阻止游戏本地启动。重新下载不会解除门禁。

本轮将启动器 profile 的“已下载但无法启动”说明显示在应用详情；同时区分安装状态与本地运行支持。没有解除门禁，也没有伪造驱动/API 成功。

## 3 下一步按计划

1. 收尾 HY1：顶部点击、动态缩放、最大化/恢复与 1×/2× 的验收；L4 解决宿主窗口的稳定寻址；之后才把 r2 升为一般发布。
2. 进入 HY2/K1：取得 Windows 对照基线，优先 UserMode/KernelMode 权限语义，以及 KMDF 请求队列、取消、完成、生命周期；对每个可实现契约逐项实现并记录不支持范围。矩阵、下载完成及通用 API 修复均不代表游戏可玩。
3. HY3 把版本/环境/验证证据接入宿主与引擎的同一启动许可逻辑。目前 WineRunner 仍用无 verdict 的预检，引擎仍用固定 deny 名单；不能仅更改 JSON Verdict 就宣称解禁闭环已经存在。
4. 依赖组合和闭环通过后才进行 HY4 原神本地登录与可操作场景；崩铁/绝区零再各自验收。Epic 不恢复。

本轮没有触发 GitHub Actions，没有新建子代理，没有添加或运行测试套件。执行了本地模块/App/CLI 构建、实际启动器坐标采样、安装文件与只读 Windows 路径诊断。最终 App ad-hoc 签名验证通过；在 Cider 的实际详情页已读取到新 profile 的“已下载但无法启动”说明和 r2 引擎信息。最终通过 Cider 正常启动米哈游启动器，不再启用 trace 日志。

0003–0006 配方按顺序应用所得源码与本轮实际编译源码逐字节一致；原 r1 缓存源码和构建模块已恢复。候选引擎的三份模块及补丁 SHA-256 与 local-build.json 记录相符。

## 本地候选模块哈希

```text
ntdll.so a9e5d97fd8596865df93829e93bef843da0bafd137bf0999bc26e7254c5aa4fd
win32u.so 6fae1324da47d0209a1ea356c0831956ec76666587cc38c6538a47917df1b0c2
winemac.so 56ed26f5ca05e11f410cc3e7d429375232e7f24b7f61ba2dae905c6e43523627
```
