# 多媒体、输入、中文（CJK）与桌面集成 —— Cider 调研报告 09

> 调研日期 2026-09-26 · 置信度说明：**[已证实]** = 直接读到一手来源（Wine master/11.18 源码、MR、tag 内的 ANNOUNCE、Apple 官方文档 JSON、上游仓库发布记录）；**[本机实测]** = 在开发机（macOS 26.5 / 25F71）上用 `swift`、`ls`、`locale` 读到的结果；**[二手]** = 搜索摘要或媒体报道；**[推断]** = 基于已证实事实做的工程推理。所有版本号和日期都附了出处，核实不了的会写明“未核实”。图形后端、CPU 翻译和 Wine 上游概况分别见报告 04/05、06、03，CrossOver 源码差异见 02。按用户偏好，本报告不做法律分析，编解码器部分只记录工程事实。
>
> **修订说明（2026-09-26）**：本版已并入独立事实核查的结论。被推翻或部分成立的段落已原地更正，并标注“（核查更正）”；逐条结论见文末“事实核查记录”。核查新增的参考来源编号为 [85]–[97]。

## 摘要

- **音频链路是完整的，但在 Mac 上仍有实质缺口 [已证实]。** WASAPI（mmdevapi 通用层）、DirectSound、winmm、XAudio2（Wine 内置 FAudio 26.09，走 WASAPI）最终都汇到 `winecoreaudio.drv`：每个流一个 AUHAL（`kAudioUnitSubType_HALOutput`）。Wine 11.16（提交日期 2026-08-16）开始把请求的 period 写入 `kAudioDevicePropertyBufferFrameSize`，解决了 IAudioClient3 在 480 帧与 Mac 默认 512 帧之间不匹配导致的爆音。下面两项截至今天仍是**开放 MR**：多声道 downmix（!11982，GTA V Enhanced 的 7.1.4 对白丢失）和“跟随系统默认设备”（!11370）。Spatial Audio 只支持静态声道对象，动态对象和 HRTF 都返回 `E_NOTIMPL`。
- **视频仍然离不开 GStreamer [已证实]。** 在 master 上（wine-11.18 tag 于 2026-09-18），MF 默认使用 GStreamer 的 byte stream handler。FFmpeg 后端 `winedmo` **只做解复用**，链接的是**宿主**的 FFmpeg（libavformat/libavcodec/libavutil），要把 `HKCU\Software\Wine\MediaFoundation\DisableGstByteStreamHandler`（REG_DWORD）设为非零才会启用。现代音视频解码器（AAC、H.264/HEVC 的 `video_decoder`、WMA、WMV/ASF 的 `wm_reader`）都在 `winegstreamer` 里。（核查更正）说“**全部**解码器都在 winegstreamer”言过其实：`mp3dmod`、`l3codeca.acm`（MP3，基于 mpg123）、`iccvid`（Cinepak）、`msvidc32`、`imaadp32.acm` 是 Wine 自带、不经 GStreamer 的实现；`ir50_32`（Indeo 5）则导入 winegstreamer [87]。Wine 11.12 内置的 FFmpeg 8.1.1 只编译 libavutil、libswresample、libswscale，没有编译任何 libavcodec 源文件，`config_components.h` 里所有 `CONFIG_*` 都是 0 [16][85][86][88]。结论不变：Cider 要解码过场动画和 xWMA，必须打包 GStreamer。上游没有 VideoToolbox 硬解，wined3d 的视频解码后端只有 Vulkan Video（11.0）和 VA-API（11.16）。
- **GStreamer 打包 [已证实]。** 官方版本为 1.28.7（2026-09-07），universal 包，最低支持 macOS 10.13。Gcenx 的 Wine 11.18 包要求全局安装 GStreamer.framework 1.28.5。（核查更正）`applemedia` 的 `vtdec` 文档页上的静态 caps 只有 H.264、HEVC、MPEG-2、JPEG 和 ProRes，但 **GStreamer 1.28 系列（1.28.0，2026-01-27）加入了 VideoToolbox 的 VP9 和 AV1 硬件解码**。这两种格式作为 VideoToolbox 的“补充解码器”（supplemental decoders）在运行时注册，所以静态 caps 里看不到。因此 VP9/AV1 **不必只走软件解码** [96]。cerbero 的 FFmpeg 配方（8.1.2，LGPL，`nonfree` 与 `videotoolbox` 均关闭）是 gst-libav 的软件解码来源。
- **手柄 [已证实]。** 上游 `winebus.sys` **只认 SDL2**（configure 检测 `libSDL2`），不支持 SDL3。macOS 的 IOHID 后端只打开 Generic Desktop 下的 Joystick 和 GamePad 设备，这样可以避开“输入监控”（Input Monitoring）授权（Bug 50153）。DS4、DualSense、Switch Pro（057e:2009）和 **Joy-Con R**（057e:2007）默认走 hidraw，也就是 macOS 上的 IOHID，所以只支持 XInput 的游戏看不到它们。（核查更正）Joy-Con L 的判断在源码里写成了 `pid == 0x057e`，不是 Joy-Con L 的 PID（通常认为是 0x2006，未核实），所以 master 上 **Joy-Con L 默认并不走 hidraw** [25][89]。另外 [推断，高置信，基于源码]：在默认注册表设置下，**Xbox 和通用手柄实际上必须依赖 SDL2**。libSDL2 缺失或加载失败时，这些手柄会整体消失，不会自动回退到 IOHID。SDL3 3.4.16 和 sdl2-compat 2.32.72 都在 2026-09-02 发布，SDL2 的最后一个版本是 2.32.10（2025-09-01）。
- **鼠标 [已证实]。** 相对移动来自 `NSEvent deltaX`，这个值经过了加速和合并。GCMouse 原始输入（!11799，2026-08-31）和“用透明光标代替 `[NSCursor hide]`”（!11880，规避 macOS 26 隐藏光标后帧率被锁到刷新率的问题）都还是开放 MR。ClipCursor 默认靠私有 API `-[NSWindow setMouseConfinementRect:]` 实现；另一条 CGEventTap 路径需要辅助功能（Accessibility）授权。
- **中文 IME [已证实]。** 链路是 `NSTextInputClient` → `IM_SET_TEXT` → win32u → imm32。候选窗由 macOS 原生绘制，位置取 `SetIMECompositionRect`，数据来源是 caret 或 `ImmSetCompositionWindow`；`ImmSetCandidateWindow` 不会被转发。2025-10 到 2026-09 这段时间里，IME 路径改动密集：11.7 引入 `ImeToAsciiEx` 驱动调用，11.18 修复了中文 HKL 下 `WM_KEYDOWN` 的 scan code 丢失（Bug 59737，这个 bug 会让 WASD 失灵）。!12164（2026-09-25）仍然开放。
- **字体是中文体验的最大隐患 [已证实 + 本机实测]。** 典型的 macOS 构建没有链接 fontconfig（未定义 `SONAME_LIBFONTCONFIG`）。这时 Wine 通过 CoreText（`load_mac_fonts`）加载**全部**系统字体，但**没有 fontconfig 的动态回退**（`fontconfig_enum_family_fallbacks` 直接返回 FALSE）。（核查更正）这是**构建时**的选择，不是 macOS 本身的属性：configure 并不排除 Darwin。如果构建时找到了 fontconfig，Wine 会**改用** `load_fontconfig_fonts()`，**不再**走 CoreText 枚举。那样能得到动态回退，但可能看不到 AssetsV2 里的 PingFang [推断]。默认 FontLink 指向 `SIMSUN.TTC`、`MSYH.TTC` 这类前缀里不存在的 Windows 字体，结果就是“豆腐块”。不过 Wine 内部的默认链接按“替换后的字体族名”解析，所以名为 `SimSun` 的 Replacements 别名就能满足它们。本机上的 PingFang 位于 `/System/Library/AssetsV2/com_apple_MobileAsset_Font8/<hash>.asset/AssetData/PingFang.ttc`，Hiragino Sans GB、STHeiti、Songti 和 Apple SD Gothic Neo 都在系统目录里。
- **区域设置 [已证实]。** macOS 上 Wine 的 Unix 代码页固定为 UTF-8，并做 NFC 规范化。系统区域（决定 ACP）按 `LC_ALL`/`LC_CTYPE`/`LANG` → `CFLocaleCopyCurrent` 的顺序取；用户 UI 语言按 `LC_MESSAGES` → `AppleLanguages` 首项取。因此 Cider **必须主动控制子进程的 `LANG`/`LC_*`**，按应用设置 `LC_ALL=zh_CN.UTF-8` 就能得到 ACP 936。有一个坑（核查补充）：如果 `LC_CTYPE` 只是字符集名 `UTF-8`（有些 macOS 终端会这样导出），Wine 会跳过 CFLocale 回退。之后在 locale.nls 里查不到这个名字，系统区域就落回 en-US（ACP 1252），与 macOS 地区设置无关 [推断，高置信，基于源码]。
- **显示 [已证实 + 本机实测]。** `RetinaMode` 对整个前缀生效，不能按应用设置。`EmulateModeset` 由 win32u 通用层读取，Mac 也适用；11.17 开始有显示模式模拟（GL 缩放和 gamma）。10.20 起按 `maximumPotentialExtendedDynamicRangeColorComponentValue` 上报 HDR 能力，DXMT 已经把 PQ 和 scRGB 映射到 EDR。开发机是 14" M3 MacBook Pro：刷新率 120Hz，可变范围 24–120Hz，EDR 潜在值 16.0，刘海高 32pt。它正好可以测 ProMotion、HDR 和刘海三项。
- **桌面集成 [已证实]。** Mac 与 Wine 之间，文本和文件的剪贴板可以互通。图片方向有缺口：Mac 端给的 `public.png`/`public.tiff` 不会合成为 `CF_DIB`。拖放只支持从 Mac 拖进 Wine。托盘图标会映射成 `NSStatusItem`，但气泡提示不支持（Bug 34645），Toast 通知也没有实现。`winemenubuilder` 只生成 XDG 条目。`winebrowser` 会回退到 `/usr/bin/open`。打印走 CUPS（`dlopen libcups`）。
- **网络 [已证实]。** Winsock 已成熟，并处理了 macOS 的差异（`SO_REUSEPORT`、`IP_RECVDSTADDR`、`TCP_CONNECTION_INFO`）。macOS 15 起的**本地网络隐私**会拦截局域网 TCP/UDP、广播和组播。从终端运行的命令行工具自动放行，从 .app 启动的进程不会，这是“开发时能联机、打包后局域网失效”的典型原因。`hnetcfg` 实现了真实的 UPnP 端口映射。

## 详细调研

### 1. 音频

**1.1 调用链 [已证实]**

| Windows API | Wine 组件 | 最终后端 |
|---|---|---|
| WASAPI（IAudioClient/2/3、IAudioRenderClient、Capture） | `mmdevapi`（格式校验、建议格式、定时线程都在通用层）[6] | `winecoreaudio.drv` unixlib → AUHAL [1] |
| DirectSound / DirectSound3D | `dsound` → mmdevapi | 同上 |
| winmm waveOut / MIDI | `winmm` → mmdevapi；MIDI 由 `coremidi.c` 处理 | CoreAudio / CoreMIDI |
| XAudio2 2.0–2.9、X3DAudio、XACT3 | `xaudio2_*` 链接 `libs/faudio`（PE 静态库，`-DFAUDIO_WIN32_PLATFORM -DHAVE_WMADEC`）[8] | FAudio 的 win32 平台层调用 `IAudioClient`，最后到 WASAPI |
| xWMA（XAudio2） | FAudio 通过 `CLSID_CWMADecMediaObject` 创建 MFT [8] | `winegstreamer` 的 `wma_decoder` → GStreamer（gst-libav） |
| 游戏自带的 OpenAL Soft、FMOD、Wwise | 原生 DLL | 调用 WASAPI 或 DirectSound |

结论 [推断，高置信]：**GStreamer 不只是视频依赖**，使用 xWMA 的 XAudio2 游戏同样离不开它。

**1.2 winecoreaudio 的设计 [已证实][1][2]**
- 每个流调用一次 `AudioComponentInstanceNew`，创建 `kAudioUnitType_Output/kAudioUnitSubType_HALOutput` 单元。渲染端的 `ca_render_cb` 从环形 `local_buffer` 拷贝数据，不够时补静音。采集端的 `ca_capture_cb` 调用 `AudioUnitRender`，输入单元自己不能做 SRC，所以再用 `AudioConverterFillComplexBuffer` 重采样。流创建后立即 `AudioOutputUnitStart` 并持续运行（源码注释说启动可能很慢）。
- Mix format 取设备的 `kAudioDevicePropertyNominalSampleRate`，格式为 32-bit float。声道布局由 `convert_channel_layout` 映射到最接近的 Windows 布局（stereo、quad、5.1、7.1）。
- 延迟的计算方式是 `kAudioDevicePropertyLatency` + `kAudioStreamPropertyLatency` + 一个 period。
- **2025–2026 年的变更**：
  - 2025-02：移除 10.12 以前的兼容代码。
  - 2025-12-03：输出单元设置 `AudioChannelLayout`（Brendan Shanks）。
  - 2026-05-15：mmdevapi 的主循环线程和定时线程改为 Wine system thread。
  - 2026-07-07：USB 设备上报 `PKEY_Device_InstanceId`（Aric Stewart）。
  - **2026-08-16**：Zhiyi Zhang 提交 “Set the requested period frame size”，并让驱动返回 CoreAudio 实际的 period。根据 MR 描述，Godot 4.1 的游戏用 IAudioClient3 请求 480 帧，而 Mac 默认是 512 帧，`ca_render_cb` 取不到足够数据，于是爆音（!11681，进入 Wine 11.16）[3][12]。核查确认：MR 创建于 2026-08-16，2026-08-17 合入，共 3 个提交（07c7fcc8 设置请求的 period、8e09602c 取采样率的辅助函数、da0b0847 返回实际的设备 period），wine-11.16 tag 日期为 2026-08-21 [90][93]。
- **仍开放的 MR（2026-09-26）**：
  - !11982（2026-09-15）：HALOutput 只能做声道映射，不能 downmix。GTA V Enhanced 输出 12 声道（7.1.4），对白在 FRONT_CENTER 上，接到双声道设备时就听不到了 [4]。
  - !11370（2026-07-12）：暴露虚拟的 “Default Output/Input” 端点，让流能跟随 macOS 默认设备切换，例如接上 AirPods 或 HDMI 时 [5]。
  - （核查更正）截至 2026-09-24/26，两个 MR 的状态仍为 opened [94][95]。!11982 的作者 Zhiyi Zhang 是 CodeWeavers 系开发者 [推断]；!11370 的作者 Rhodri Richards 的所属**未核实**。因此只能说 CrossOver 可能在把部分音频修复上游化，不能断定两个 MR 都来自 CodeWeavers。

**1.3 WASAPI 模式与 Spatial Audio [已证实][6][7]**
- 共享模式完整。独占模式只做了“缓冲区按 period 对齐”，“EXCLUSIVE + EVENTCALLBACK”仍会打出 FIXME，本质上还是经 HAL 的共享路径。IAudioClient3 的 `GetSharedModeEnginePeriod`、`InitializeSharedAudioStream` 已实现；`GetBufferSizeLimits` 是 stub。loopback 只有 stub（依赖后端的 `get_loopback_capture_device`），跨进程会话不支持。
- `ISpatialAudioClient`：静态对象（`AudioObjectType_FrontLeft` 等）被混到声道床上。`GetMaxDynamicObjectCount`、`SetPosition`、`SetVolume`、`ISpatialAudioObjectRenderStreamForHrtf` 都返回 `E_NOTIMPL` 或 FIXME。所以 Windows Sonic 和 Atmos 的动态对象、HRTF 都不可用；耳机上的空间感只能靠 macOS 系统级的“空间化立体声”[推断]。

**1.4 XAudio2：FAudio 还是原生 [已证实]**
- Wine 内置 FAudio 按月导入：26.02、26.06（11.11），**26.09 于 2026-09-04 导入（11.17）**[8][12]。上游 FAudio 26.07 重写了重采样，26.08 增加了 AArch64 NEON 混音，26.09 修复了 WMA 解码器创建 [9]。
- CrossOver 从 18.5.0 起使用 FAudio 重新实现 XAudio2 [81]。原生 `xaudio2_7`（winetricks `xact`）只应作为按游戏的回退方案。

**1.5 麦克风与后台运行 [已证实][75][35]**
- 需要同时具备两样东西：`NSMicrophoneUsageDescription`，以及启用 Hardened Runtime 时的 `com.apple.security.device.audio-input` 权利。TCC 会把授权记到“负责进程”（通常是拉起 Wine 的 Cider.app）名下 [推断，高置信]。
- winemac 默认 `EnableAppNap=false`，进程转到前台时会调用 `beginActivityWithOptions:NSActivityUserInitiatedAllowingIdleSystemSleep`，避免后台音频被 App Nap 节流。
- Game Mode（macOS 14+，仅 Apple Silicon）会把蓝牙采样率翻倍，从而降低手柄和 AirPods 的延迟，游戏进入全屏时自动开启 [76]。系统如何把一个非 bundle 的 Wine 进程识别为“游戏”，**未核实**。

### 2. 视频播放

**2.1 上游现状 [已证实]**
- **后端选择**：`mfsrcsnk/media_source.c` 的 `use_gst_byte_stream_handler()` 读取 `HKCU\Software\Wine\MediaFoundation\DisableGstByteStreamHandler`（REG_DWORD），**默认值是 TRUE，即用 GStreamer**；当 winedmo 不支持某个容器时也会回退到 GStreamer [13]。
- **winedmo 的能力**：只导出 `winedmo_demuxer_*` 这组函数，unix 端链接宿主的 `libavformat`/`libavcodec`/`libavutil`（configure 通过 pkg-config 检测）[14][27]。
- **解码器**：`winegstreamer` 里有 `aac_decoder.c`、`video_decoder.c`（H.264 等）、`wma_decoder.c`、`wm_reader.c`（WMV/ASF）、`quartz_parser.c`（DirectShow）[15]。（核查更正）现代格式的解码器都集中在这里，但并不是**所有**解码器都依赖 GStreamer：`mp3dmod` 和 `l3codeca.acm`（MP3，基于 mpg123）、`iccvid`（Cinepak）、`msvidc32`（MS Video 1）、`imaadp32.acm`（IMA ADPCM）是 Wine 自带的实现；`ir50_32`（Indeo 5）的 Makefile 写着 `IMPORTS = winegstreamer`，仍然依赖 GStreamer [87]。
- **内置 FFmpeg**：Wine 11.12 的 release note 原文是 “Bundled libswresample and libswscale from FFmpeg” 和 “Import ffmpeg from upstream release 8.1.1”（`libs/ffmpeg/VERSION` 为 8.1.1）。核查确认 `libs/ffmpeg/Makefile.in` 只编译 libavutil、libswresample、libswscale，没有任何 libavcodec 源文件；`config_components.h` 里所有 `CONFIG_*`（decoder、demuxer、bsf 等）都是 0 [12][16][85][86][88]。这份 FFmpeg 用于 `resampledmo` 这类 DSP 组件，11.13 又加了 ARM64EC 汇编。
- **硬件解码**：11.0 通过 Vulkan Video 实现 D3D11 H.264 硬解，但必须用 wined3d 的 Vulkan 渲染器；11.16 增加了 wined3d VA 解码后端（Linux）[11][12]。GitLab 上搜 “videotoolbox” 没有任何 MR。MoltenVK 不提供 Vulkan Video [推断]。DXMT 和 D3DMetal 的 `ID3D11VideoDevice` 实现状况**未核实**。

**2.2 在 macOS 上打包 GStreamer**
- 官方版本 1.28.7，发布于 2026-09-07，是安全修复版 [17][18]。有 runtime、devel、debug 三种安装包，universal（x86_64+arm64），最低 macOS 10.13 [17]。Gcenx 的 Wine 11.18 包（2026-09-25）要求把 GStreamer.framework **1.28.5 装给所有用户**，另外预置了 “win32u: Enable host Vulkan portability enumeration” 补丁 [24]。
- 私有打包的做法：放进 `App.app/Contents/Frameworks`，用 `osxrelocator.py` 把 `/Library/Frameworks/GStreamer.framework/` 改写为 `@executable_path/../Frameworks/...`，然后设置 `GST_PLUGIN_SYSTEM_PATH`、`GST_PLUGIN_SCANNER`、`GIO_EXTRA_MODULES` [19]。
- 体积：runtime 包大约 93MB 到 190MB 量级 [二手，搜索摘要，未核实具体数值]。
- 历史坑：Bug 54831 中，`gst_init_check()` 在较新的 macOS 上崩溃，Wine 在 Sonoma 上也会崩，2023 年已修复 [82]。
- cerbero 的包划分 [20][21]：

| cerbero 包 | 内容 | Cider 取舍 |
|---|---|---|
| `gstreamer-1.0-core`/`playback`/`system` | 核心，以及 base/good/bad 的 sys 插件；macOS 的 `system` 包还带 moltenvk | 需要（applemedia 在 bad 的 sys 插件里） |
| `gstreamer-1.0-libav` | `ffmpeg:libs` + gst-libav；FFmpeg 8.1.2，LGPLv2.1+，`nonfree`、`hwaccels`、`videotoolbox`、`audiotoolbox` 都关闭 | 需要（WMV、WMA、ADPCM、MPEG-1/2、Indeo 等只能靠它） |
| `codecs-restricted` | ugly/bad 的受限插件、vo-aacenc、vvdec | 按需 |
| `codecs-gpl-restricted` | x264、x265 等 GPL 编码器 | 不需要（Wine 只解码） |

**2.3 VideoToolbox（vtdec）[已证实][22][23][96]**
- 文档页列出的**静态** sink caps 包括：`video/x-h264, stream-format=avc, alignment=au`；`video/x-h265, stream-format={hev1,hvc1}`；`video/mpeg, mpegversion=2`；`image/jpeg`；ProRes。src caps 是 NV12/AYUV64/ARGB64/RGBA64，或 `memory:GLMemory`。另外有只走硬件的 `vtdec_hw`。
- （核查更正）原稿说“VP9 和 AV1 不在 caps 里，只能走软件解码”，这是错的。**GStreamer 1.28 系列（1.28.0，2026-01-27；当前 1.28.7，2026-09-07）给 applemedia 加入了 VideoToolbox 的 VP9 和 AV1 硬件解码**，同一版本还加了 10-bit HEVC 编码。这两种格式作为 VideoToolbox 的“补充解码器”（supplemental decoders）在运行时注册，所以文档页的静态 caps 里看不到。后续点版本还调整过这部分：VP9 只在第一个 vtdec 元素上启用，补充解码器改由 vtutil 辅助函数注册，支持情况存进全局变量 [96]。M3 的 AV1 硬解有 Apple 文档记载，本次未复核 [推断]。因此在 GStreamer 1.28.x 上，AV1 和 VP9 可以经 vtdec 硬解；不支持的机型或旧系统上仍回退到 gst-libav 或 dav1d 软解。winegstreamer 搭建的管线会不会实际选中 vtdec，**需要本机实测**。
- winegstreamer 输出到系统内存，vtdec 解码后还有一次拷贝，但这对过场动画来说足够 [推断]。

**2.4 编解码器（只记工程事实）**：上游 Wine 自身不带任何 H.264、HEVC 或 AAC 解码器 [16]。gst-libav 的默认 LGPL 构建带这些格式的软件解码 [21]，vtdec 用的是系统自带的解码器。Cider 的默认策略可以是“H.264/HEVC 优先 vtdec，其余交给 gst-libav”。（核查更正）GStreamer 1.28+ 上 VP9 和 AV1 也可以优先交给 vtdec，gst-libav 或 dav1d 只作软件回退 [96]。

**2.5 Bink 与常见过场动画故障**

| 类型 | 典型游戏或引擎 | 在 Wine/Mac 上的表现 | 处理办法 |
|---|---|---|---|
| Bink 1/2、Smacker | 大量 3A 与独立游戏 | 由游戏自带的 `binkw32`/`bink2w64` 用 CPU 解码，与 Wine 多媒体栈无关，通常正常 [推断] | 出问题多半是音频输出（Miles/DirectSound）或纹理上传，属于图形层 |
| WMV/ASF（DirectShow、WMReader） | 2000–2012 年的游戏 | 依赖 `wm_reader` 和 gst-libav 的 wmv/wma 解码器；ASF Reader 的 seek 在 11.0 实现 [11] | GStreamer 插件要装全 |
| MP4/H.264（MF Source Reader、Media Engine） | UE4/5 的 WmfMedia、Unity VideoPlayer | 回归比较多，11.18 还在修：#60307 Bard's Tale IV 的 MP4 黑屏卡死，#60326 In Sound Mind 启动视频崩溃 [12] | 跟进上游；可按游戏试 `DisableGstByteStreamHandler=1`；最后手段是跳过视频 |
| MPEG-1/2（quartz） | 老 DirectShow 游戏 | mpg123 与 GStreamer 都能处理 | — |
| CRI USM/Sofdec、Theora、VP8/9（自带解码器） | 日系、独立游戏 | 由游戏自带的原生库解码 | — |

Proton 的 media-converter 用转码缓存绕开专利格式，它依赖 Steam 分发，没法移植（见报告 03）。

### 3. 输入

**3.1 winebus.sys 在 macOS 上的结构 [已证实][25][26][27]**
- 有三条总线：SDL（`sdl_driver_init`）、UDEV（仅 Linux）和 IOHID（`iohid_driver_init`）。configure 只检测 `libSDL2`（`WINE_CHECK_SONAME(SDL2,SDL_Init,...[[libSDL2-2.0*]])`），源码按 `SONAME_LIBSDL2` 做 dlopen，**不支持 SDL3**。
- **IOHID 只接手柄**：`handle_DeviceMatchingCallback` 遇到不是 Generic Desktop/Joystick 或 GamePad 的设备直接跳过。源码注释写明，打开键盘、鼠标或 Touch Bar 会触发“输入监控”授权弹窗（对应 Bug 50153：Catalina 和 Big Sur 上 `IOHIDManagerOpen` 返回 `kIOReturnNotPermitted`，2021 年修复）[26][28]。
- **仲裁规则**：对每个设备计算 `is_hidraw_enabled()`，结果为真就保留 hidraw（IOHID）版本并丢弃 SDL 版本，反之亦然。默认偏好 hidraw 的设备包括 DS4、DualSense、Switch Pro（057e:2009）、Joy-Con R（057e:2007），以及 Thrustmaster、Simucube、Fanatec、VKB、VPC、Winwing 等模拟外设 [25]。（核查更正）原稿笼统写成“Joy-Con”，不准确：`main.c` 里 Joy-Con L 的判断写成了 `pid == 0x057e`（和 VID 一样），不是 Joy-Con L 的 PID（通常认为是 0x2006，未核实），所以 Joy-Con L 默认仍走 SDL [25][89]。
- （核查补充）[推断，高置信，基于源码，未做运行时测试] 对 Xbox 和通用手柄，`is_hidraw_enabled()` 只有在 `options.disable_sdl && options.disable_input` 同时成立时才返回 TRUE。默认设置下（`Enable SDL`=1、`DisableInput`=0），这类手柄的 IOHID 实例会以 “ignoring hidraw device” 被丢弃，只保留 SDL 实例（`bus_iohid.c` 给所有 IOHID 设备都设 `.is_hidraw = TRUE`）。如果 libSDL2 缺失或运行时加载失败，`sdl_driver_init()` 返回错误，`options.disable_input` 不会被置位，这些手柄就会**整体消失**，不会回退到 IOHID。不带 SDL 的构建必须同时设置 `Enable SDL`=0 和 `DisableInput`=1 才能改走 IOHID [25][26][27]。所以对 macOS 上的 Wine 来说，SDL2 不是可选增强，而是 Xbox 和通用手柄的必需依赖。
- 注册表位置是 `HKLM\System\CurrentControlSet\Services\WineBus`（注册表不区分大小写），可用的值有：`Enable SDL`（默认 1）、`DisableHidraw`（0）、`DisableInput`（0）、`Map Controllers`（1，把 SDL 手柄映射成 XInput 兼容的 HID）、`Split Controllers`（0），以及 `EnableHidraw` [25]。（核查更正）`EnableHidraw` 不是按设备的开关，而是 REG_MULTI_SZ 列表，每项是 `VVVV:PPPP` 字符串，**只能强制启用** hidraw，写 0 没有作用。按设备的双向覆盖在子键 `WineBus\Devices\<VID>/<PID>`（或只写 `<VID>`）下，用 DWORD 值 `Hidraw`：0 强制不走 hidraw，1 强制走 hidraw。`is_hidraw_enabled()` 最先检查这些子键，所以它们能覆盖内置的 DS4/DualSense/Switch 列表。全局 `DisableHidraw=1` 也能生效，但 `iohid_driver_init` 会因此直接返回，整个 IOHID 总线都不工作，需要 hidraw 的飞行摇杆和方向盘也会一起失效（11.17 的 ANNOUNCE 也写明这个开关影响 IOHID 后端）[25][91]。
- 上层 API 包括 `xinput1_1…1_4`/`xinputuap`、`dinput`/`dinput8`（11.0 起支持 action map）、`windows.gaming.input`（11.0 起 joy.cpl 有对应页面）和 `WM_INPUT`，它们都消费 winebus 提供的 HID 设备 [11]。

**3.2 SDL 在 macOS 上的行为 [已证实][29][30][31]**
- SDL3 的 MFi 后端（`SDL_mfijoystick.m`）遇到 HIDAPI 已经接管的 Xbox、PS4、PS5、Switch Pro、Joy-Con、8BitDo、Steam Controller 时会主动让出。它会把绑定了系统手势的按钮（Home、Share）设为 `GCSystemGestureStateDisabled`，并在 macOS 11.3+ 上设置 `GCController.shouldMonitorBackgroundEvents = YES`。后一点对 Cider 很关键：winebus 跑在后台的 `winedevice.exe` 进程里，它不在前台也要能收到事件。
- CrossOver 的 **CW HACK 19629** 在 `bus_sdl.c` 中加了 `SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE`/`PS5_RUMBLE = "1"`，只在 `__APPLE__` 下生效，作用是让 DS4 和 DualSense 在蓝牙下也能震动 [31]。
- 版本（GitHub releases）：SDL2 最后一个版本 2.32.10（2025-09-01）；SDL 3.4.16（2026-09-02）；sdl2-compat 2.32.72（2026-09-02）[30][92]。核查确认了这三个日期。

**3.3 各类手柄在 Cider 默认配置下的路径 [推断，基于 3.1 和 3.2]**

| 手柄 | winebus 路径 | 仅支持 XInput 的游戏 | 原生支持 PS/Switch 的游戏 |
|---|---|---|---|
| Xbox One/Series（蓝牙/USB） | SDL（HIDAPI 或 GameController）→ Map Controllers；libSDL2 加载失败时手柄整体消失（核查补充） | 可用 | — |
| DS4/DualSense（蓝牙/USB） | IOHID raw | **不可用**（Windows 上直连也是这样） | 可用（按键、灯条、触觉） |
| Switch Pro（057e:2009）、Joy-Con R（057e:2007） | IOHID raw（11.15 起偏好 hidraw）[89] | **不可用** | 少数游戏可用 |
| Joy-Con L（核查更正） | SDL（源码把 PID 写成 0x057e，没进入 hidraw 列表）[25] | 可用 [推断] | 和 Joy-Con R 走不同路径，拆开使用时行为不一致 |
| 通用 HID 手柄 | SDL（默认设置下必须依赖 SDL，见 3.1） | 可用（需要映射库） | — |

所以 Cider 必须提供一个开关，把 PS 和 Switch 手柄当作 Xbox 手柄。（核查更正）正确做法是按设备写 `WineBus\Devices\<VID>/<PID>` 下的 DWORD `Hidraw=0`。原稿写的 “`EnableHidraw=0`” 无效，因为 `EnableHidraw` 只能强制启用。全局 `DisableHidraw=1` 也能达到目的，但会关掉整个 IOHID 总线，飞行摇杆和方向盘会一起失效，只适合作为最后手段 [25][91]。CrossOver 在控制面板里就有 “Disable hidraw”（报告 01）。Joy-Con L 的 PID 笔误值得提交上游修正；在那之前，Cider 可以写 `Devices\057E/2006` 下的 `Hidraw=1` 覆盖，让左右 Joy-Con 行为一致（前提是 0x2006 确实是 Joy-Con L 的 PID，待核实）。

**3.4 鼠标 [已证实][32][33][34][35]**
- **光标限制**：默认用私有 API `-[NSWindow setMouseConfinementRect:]`（macOS 10.13+），只能限制在单个窗口内。设置 `UseConfinementCursorClipping=n` 后改用 CGEventTap 加 `CGAssociateMouseAndMouseCursorPosition(false)`，但从 Catalina 起需要辅助功能授权 [34]。11.10 修复了 ClipCursor 的 reset 参数处理 [12]。
- **相对移动**：光标被限制或隐藏时，winemac 把 `NSEvent deltaX/deltaY` 作为 `MOUSE_MOVED_RELATIVE` 发出 [35]。这个值带系统加速，并且和 WindowServer 的事件合并在一起。
- **!11799**（Elvin Hayatov，2026-08-31，开放）：应用注册原始输入鼠标时启用 `GCMouse`，拿到未加速的 1000Hz 增量；`WINE_DISABLE_GCMOUSE=1` 可以回退。MR 用 Overwatch、Marvel Rivals 和 KCD2 测试 [32]。
- **!11880**（2026-09-05，开放）：在 macOS 26 上，隐藏光标后一移动鼠标，画面呈现就会被同步到刷新率。Overwatch 从 250–300 FPS 掉到 120 FPS。改用透明光标可以规避 [33]。
- GCMouse 本身从 macOS 11 起可用 [77]；它是否需要“输入监控”授权，**未核实**。

**3.5 键盘与修饰键 [已证实][36][37][35][38]**
- 默认映射：左、右 Command 分别映射为 `VK_LMENU`/`VK_RMENU`（即 Alt）；Option **不映射**，留给系统组合重音字符；Control 映射为 Ctrl；没有 Win 键。
- 选项（`HKCU\Software\Wine\Mac Driver`，也可以放在 `AppDefaults\<exe>\Mac Driver` 下按应用设置）：`LeftCommandIsCtrl`、`RightCommandIsCtrl`、`LeftOptionIsAlt`、`RightOptionIsAlt`，默认都是 false [37]。
- 为了不和 Windows 的 Alt 组合键冲突，Wine 菜单里的“退出”是 **Cmd+Opt+Q**，“隐藏其他”是 Cmd+Opt+H。捕获显示器时，Cmd-Tab 由 winemac 自己处理 [35]。
- **CrossOver Hack 10912 “Mac Edit menu”**：增加注册表值 `EditMenu`（`key`/`message`/禁用，默认 by-key）。用户在 Mac 编辑菜单里选择或按 Cmd+C/X/V/A/Z 时，它会合成 Ctrl+C 等按键，或者发送 `WM_COPY` 这类消息。上游没有这个功能 [38]，它是“Cmd=Alt”默认映射下办公软件可用性的关键。
- 仍开放的 bug：53243（Ctrl+数字键输出了数字）、44382（右 Shift 被识别成左 Shift）[80]。

**3.6 TCC 汇总**：输入监控（打开键盘/鼠标类 HID，winebus 已避开）、辅助功能（CGEventTap）、麦克风/摄像头（usage description 加权利）、本地网络（见 7）、蓝牙（如果 SDL 用 CoreBluetooth 访问 BLE 手柄，**未核实**）。

### 4. 中文 / CJK

**4.1 IME 架构与近期变更 [已证实][39][40][41][42][43][44]**
- Cocoa 侧：`WineContentView` 实现 `NSTextInputClient`。`setMarkedText:` 对应预编辑文本，`insertText:` 通过 `completeText:` 提交，二者都经 `IM_SET_TEXT` 事件（含 himc、text、complete、cursor_begin/end）送出。`firstRectForCharacterRange:` 返回受互斥锁保护的 `ime_composition_rect`，用来定位 macOS 候选窗。
- Win32 侧：win32u 发出 `WINE_IME_POST_UPDATE`，imm32 生成 `WM_IME_STARTCOMPOSITION`/`COMPOSITION`/`ENDCOMPOSITION`。应用不自己绘制时，由 imm32 的默认 IME UI 窗口显示预编辑文本：`CFS_DEFAULT` 放在窗口左下，`CFS_POINT`/`CFS_RECT` 按应用给定的位置。
- 时间线：
  - 2023–2024：Rémi Bernon 把 IME 迁到 imm32 和 win32u；Tim Clem 增加了手写/面板类 IME 检测，并排除听写 [41]。
  - 2024-10：通过 `SetIMECompositionRect` 跟踪位置。
  - 2025-10/11：`macdrv_ime_process_key` 改为同步，移除 `QUERY_IME_CHAR_RECT`。
  - **11.7（2026-04）**：`ImeToAsciiEx` 成为用户驱动调用（Marc-Aurel Zent，!9992）。
  - 11.10：根据 HKL 设置 IME 的打开状态。
  - **11.18**：“imm32: Fix malformed messages in ImeToAsciiEx()”。这修复了 Bug 59737：在中文 HKL `00000804` 下，`VK_PROCESSKEY` 之后的真实 `WM_KEYDOWN` 的 scan code 变成 0，依赖 scan code 的游戏（WASD、退格）会失灵，法语等非英语区域也会受影响 [43][12]。
  - **!12164（2026-09-25，开放）**：从 Cocoa 主线程直接投递 IME 更新，并把 IME 代码集中到一个文件 [42]。
- **候选窗定位的规则** [44]：`NtUserSetCaretPos`/`CreateCaret` → `set_ime_composition_rect`；`ImmSetCompositionWindow` → `NtUserCallTwoParam_SetIMECompositionRect`；`ImmSetCandidateWindow` **不会**转发。所以用系统 caret 的传统控件定位准确；自绘文本框（游戏、部分 Qt/Electron 应用）只有调用了 `ImmSetCompositionWindow` 才能定位，否则候选窗会跑偏 [推断]。
- **游戏内候选列表**：macOS 的 IME 不通过 `NSTextInputClient` 把候选词交给客户端，所以 `ImmGetCandidateList` 拿不到系统拼音的候选词。自己绘制候选框的游戏（常见于国产网游）只会看到 macOS 原生候选窗 [推断，高置信]。拼音、双拼、搜狗、微信输入法、鼠须管都是 IMK 输入法，走同一套协议 [推断]。
- 依赖 TSF（`msctf`）的应用，比如 Chromium/CEF 内核的启动器，IME 行为**未核实**。

**4.2 字体 [已证实 + 本机实测][45][46][47][48]**
- 加载方式：`load_mac_fonts()` 调用 `CTFontCollectionCreateFromAvailableFonts` 并去重，把系统可用的所有字体（跳过 pfa/pfb）按 URL 注册为外部字体。所以放在 AssetsV2 里的 PingFang 也能被 Wine 看到。（核查更正）这取决于构建方式，不是 macOS 固有的行为。`freetype_load_fonts()` 里写的是 `#ifdef SONAME_LIBFONTCONFIG load_fontconfig_fonts(); #elif defined(__APPLE__) load_mac_fonts();`，而 configure.ac 的 fontconfig 检测并不排除 Darwin。如果构建时找到了 fontconfig，Wine 会**改用** fontconfig 枚举，**不再**调用 `load_mac_fonts()`，那时 AssetsV2 里的 PingFang 可能就看不到了 [推断] [27][45]。
- **典型的 Mac 构建（不链接 fontconfig）没有动态回退**：`fontconfig_enum_family_fallbacks` 在没有 `SONAME_LIBFONTCONFIG` 时返回 FALSE。`font.c` 的 `enum_fallbacks` 只有 3 个硬编码的默认回退（都不是 CJK），然后才调用 `font_funcs->enum_family_fallbacks` [45][46]。缺字时只能依赖 FontLink、`FontSubstitutes` 和 `HKCU\Software\Wine\Fonts\Replacements`。
- 默认链接指向 Windows 字体：`font_links_defaults_list` 中简体中文以 “SimSun” 作为 MS Shell Dlg 的替代；`system_link_tahoma_sc` 列出的是 `SIMSUN.TTC,SimSun`、`MSYH.TTC,Microsoft YaHei UI` 等文件，干净的前缀里都没有，结果是 Wine 自己的对话框和应用 UI 出现“豆腐块”[推断，高置信]。（核查补充）这些链接有两种解析方式。Wine 内部的默认链接（`populate_system_links`）先经 `get_gdi_font_subst` 做替换，再用 `find_family_from_name` 按字体族名查找，所以名为 `SimSun` 的 Replacements 别名就能满足它们。注册表里基于文件名的 `SystemLink` 条目则要靠 `find_face_from_filename()` 匹配文件名才生效 [46]。
- 社区做法：winetricks（`20260125-next`）的 `fakechinese` 先装思源黑体（Source Han Sans），再把 SimSun、NSimSun、SimHei、Microsoft YaHei(UI)、KaiTi、FangSong、DengXian 以及繁体的 MingLiU、PMingLiU、Microsoft JhengHei 等写入 `Replacements`，分别指向 “Source Han Sans SC/TC”[47]。
- 本机字体（macOS 26.5）：

| 字体 | 位置 | 适合替代 |
|---|---|---|
| PingFang SC/TC/HK | `/System/Library/AssetsV2/com_apple_MobileAsset_Font8/<hash>.asset/AssetData/PingFang.ttc`（路径含哈希，会变） | Microsoft YaHei(UI)、DengXian、MS Shell Dlg |
| Songti SC/TC | `/System/Library/Fonts/Supplemental/Songti.ttc` | SimSun、NSimSun、MingLiU |
| STHeiti Light/Medium、Hiragino Sans GB | `/System/Library/Fonts/` | SimHei 或备选 |
| ヒラギノ角ゴシック W0–W9、ヒラギノ明朝 ProN | `/System/Library/Fonts/` | MS Gothic、Meiryo、Yu Gothic、MS Mincho |
| AppleSDGothicNeo.ttc | `/System/Library/Fonts/` | Gulim、Malgun Gothic |
| Arial Unicode、CJKSymbolsFallback | 系统目录 | 符号兜底 |

- 自带字体的体积：Noto CJK 的 Sans2.004（2022-01-27）中，`NotoSansCJK.ttc.zip` 为 95.2MB，`18_NotoSansSC.zip`（全字重）为 50.1MB [48]。结论：**默认不打包**，用 Replacements 指向系统字体；仅在需要度量兼容时，才作为可选下载提供 Noto 或思源的单字重子集。

**4.3 区域设置与代码页 [已证实 + 本机实测][49]**
- `ntdll/unix/env.c`：`__APPLE__` 下 Unix 代码页固定为 `CP_UTF8`，并加载 `normnfc.nls` 把文件系统返回的 NFD 名称还原为 NFC。
- `init_locale()` 的逻辑：先执行 `setlocale(LC_ALL,"")`。系统区域取自 `LC_CTYPE`，没有时用 `CFLocaleCopyCurrent()` 的“语言-地区”。用户区域取自 `LC_MESSAGES`，没有时用 `CFLocaleCopyPreferredLanguages()` 的第一项，带 script，例如 `zh-Hans-CN`。ACP 和 OEMCP 由系统区域决定，zh-CN 对应 936/936，zh-TW 对应 950，ja-JP 对应 932。
- （核查补充）独立核查确认了上面的逻辑，并指出两个细节 [推断，高置信，基于源码][49]：
  - 值为 `C` 时，`unix_to_win_locale` 会改读 `getenv("LC_ALL")`。
  - 如果 `LC_CTYPE` 只是字符集名 `UTF-8`（部分 macOS 终端配置会导出这个值），`unix_to_win_locale` 会返回 TRUE，名称就是 “UTF-8”。这样 CFLocale 回退被跳过，locale.nls 里又查不到这个名字，系统区域落回 en-US（ACP 1252），与 macOS 的地区设置无关。
- 本机 `locale -a` 能看到 `zh_CN.UTF-8`、`zh_CN.GBK`、`zh_CN.GB18030`、`zh_TW.UTF-8`、`ja_JP.UTF-8`、`ko_KR.UTF-8` 等。
- **乱码（mojibake）的成因与对策**：

| 现象 | 根因 | 对策 |
|---|---|---|
| 非 Unicode 程序、NSIS/Inno 安装包的中文变乱码 | 系统区域不是中文，ACP≠936 | 按应用设置 `LC_ALL=zh_CN.UTF-8`，相当于 Locale Emulator |
| 从终端启动时一切正常，从 App 启动出问题（或反过来） | 终端带着 `LANG=en_US.UTF-8`，GUI 启动时没有 `LANG` | Cider 显式设置 `LANG`、`LC_*`，不继承外部环境 |
| 从某些终端启动时，Mac 地区明明是中国，ACP 却是 1252（核查补充） | 终端导出了只有字符集名的 `LC_CTYPE=UTF-8` | 同上；尤其不能让继承的裸 `LC_CTYPE` 漏进子进程 |
| 界面是中文，但显示成方块 | 缺少 CJK 字体链接，不是编码问题 | 见 4.2 |
| 日文游戏乱码 | 需要 ACP 932 | `LC_ALL=ja_JP.UTF-8` |
| 控制台程序乱码 | OEMCP 或 `chcp` 设置不对 | 系统区域设成中文，OEMCP 就是 936 |
| winhlp32 显示中文帮助乱码 | Bug 30325，仍开放 | 用外部查看器 |

**4.4 Wine 自身的中文界面**：用 python 粗略统计 master 的 po 文件，`zh_CN.po` 在 4469 条中已翻译约 97.3%，zh_TW 约 94.4%，ja 约 98.3%，ko 约 97.3% [50]。winemac 菜单中的 “Wine / 窗口 / 进入全屏” 等字符串也来自这些 po 文件。关键前提仍是字体（见 4.2）。

### 5. 显示

**5.1 Retina 与 DPI [已证实][37][51][54]**
- `RetinaMode`（默认 false）**只从前缀全局键读取，不读 AppDefaults**。源码注释写的是 “DPI and monitor sizes should be consistent for all processes in the prefix”。关闭时 1 个 Windows 像素等于 1 pt，由 macOS 放大 2 倍，因此发糊；打开后应用能看到物理像素。
- CrossOver 的 “High Resolution Mode” 在文档里的描述是“关闭像素倍增，并向应用报告 192 DPI”[54]。
- Wine 10.0 起会自动缩放不感知 DPI 的窗口，并提供按应用的兼容标志（AppCompatFlags 中的 `HIGHDPIAWARE`/`DPIUNAWARE`，win32u 会读取）[10][55]。
- 相关修复：2025-05 “OpenGL backbuffer 按窗口 DPI 设置尺寸”（!7979）；2025-12-17 修正 RetinaMode 下 `macdrv_get_monitors` 返回一半尺寸、导致 Steam 只能在左上象限移动的问题（!9781）[51]。仍开放的 Bug 42973：分辨率改变后 RetinaMode 会被关闭 [80]。

**5.2 全屏、分辨率与刘海 [已证实][35][39][55][56][57][58][59]**
- Win32 全屏窗口不进入 macOS 原生全屏 Space，而是通过窗口层级（活动时用 `NSStatusWindowLevel + 1`）覆盖整个屏幕。用户可以点绿色按钮，或在 Wine 窗口菜单里按 Cmd+Ctrl+Opt+F 进入原生全屏。winemac 能检测台前调度（Stage Manager）。
- 改分辨率要先 `CGCaptureAllDisplays()`，再 `CGDisplaySetDisplayMode`。设置 `CaptureDisplaysForFullscreen=y` 时，只要有全屏窗口就会捕获显示器。Wine 11.0 支持独占全屏 [11]。
- Apple Silicon 内屏只有少数几种模式。本机共 132 个模式，刷新率集合为 {47.95, 48, 50, 59.94, 60, 120}。Bug 56478（低于 HD 的全屏分辨率导致程序出错）仍开放 [80]。CrossOver Hack 18576 在 Apple Silicon 上**不检查 `kDisplayModeSafeFlag`**，从而开放更多模式 [57]。
- **模式模拟**：`EmulateModeset` 从 `HKCU\Software\Wine\X11 Driver`（也可以放在 AppDefaults 下）读取，但读取发生在 **win32u 通用代码**里，对 winemac 同样生效 [55]。11.17 加入了 “Initial support for display mode emulation”：OpenGL 按模拟分辨率缩放，并模拟 gamma ramp [12]。
- 刘海：本机 `NSScreen.safeAreaInsets.top = 32pt`。`NSPrefersDisplaySafeAreaCompatibilityMode`（macOS 12+）写在 Info.plist 里，决定是否启用“避开摄像头区域”的兼容模式；没有这个键时，Finder 的“显示简介”里会出现一个复选框 [58][59]。CrossOver Hack 20512 专门为 Skyrim SE 启动器处理了 safe area [57]。

**5.3 多显示器**：显示器通过 CGDisplay 枚举，11.14 起用 `CGDisplayRegisterReconfigurationCallback` 监听配置变化（报告 03）。仍开放的 Bug 60266：全屏窗口上方的窗口被画到了它后面 [80]。

**5.4 ProMotion / VRR [本机实测 + 已证实][56][53][60]**
- 本机为 120Hz，`minimumRefreshInterval` 为 1/120，`maximumRefreshInterval` 为 1/24，`CGDisplayModeGetRefreshRate` 返回 120。winemac 在该值为 0 时回退为 60Hz [56]。
- 11.17 移除了基于 CVDisplayLink 的 `WineDisplayLink`（!11850）[53]。可变刷新率的帧节奏应在 DXMT、D3DMetal 或 MoltenVK 的呈现路径里，通过 `CAMetalDisplayLink`（macOS 14+）实现 [60]。社区 winecx 分支正在做这件事（报告 02）。

**5.5 HDR：EDR 与 Windows HDR [已证实][52][61][62][63]**
- Wine 10.20 起，`gdi_monitor.hdr_enabled` 由 `NSScreen.maximumPotentialExtendedDynamicRangeColorComponentValue > 1` 决定，同时提供 `DisplayConfigGetDeviceInfo(ADVANCED_COLOR_INFO)` 半桩，Alan Wake 2、Control、Silent Hill 2、S.T.A.L.K.E.R. 2 会检查这一项（!9556）[52]。本机的 EDR 潜在值为 16.0，当前值为 1.0。
- DXMT 的 `d3d11_swapchain.cpp` 把 `DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020` 映射到 `WMTColorSpaceHDR_PQ`，把 FP16 格式映射到 scRGB；`SetHDRMetaData(HDR10)` 会透传；是否支持 EDR 由 layer 的 EDR 值判断 [62]。
- 两边的语义不一致：Windows 的 HDR 是系统级开关，EDR 则是“随时都有余量”。只要显示器支持，Wine 就上报 HDR 已启用，游戏可能默认开启 HDR，导致 SDR 内容过暗或色调映射异常 [推断]。

### 6. 桌面集成

| 功能 | 上游 Wine（Mac） | CrossOver 的补充 | 缺口与 Cider 方案 |
|---|---|---|---|
| 剪贴板·文本 | `CF_UNICODETEXT` ↔ `public.utf16-plain-text`/`public.utf8-plain-text`；RTF ↔ `public.rtf`；HTML Format ↔ `public.html` [64] | clipboard.c 与上游相同 | — |
| 剪贴板·图片 | `CF_DIB` ↔ `com.microsoft.bmp`；`CF_TIFF` ↔ `public.tiff`；“PNG” ↔ `public.png`；“JFIF”、“GIF” [64] | 无 | **Mac 截图的 PNG/TIFF 不会合成 `CF_DIB`**，大多数 Windows 程序粘贴不了图片；反方向只提供 BMP。可以打补丁，用 ImageIO 做 PNG/TIFF ↔ DIB/DIBV5 转换 [推断] |
| 剪贴板·文件 | `CF_HDROP` ↔ `NSFilenamesPboardType`（已废弃但可用）[64] | — | 补充 `public.file-url` |
| 拖放 | Mac → Wine 经 win32u 的拖放接口（!6717，2024-10），`QUERY_DRAG_DROP_*` → `WINE_DRAG_DROP_*` [65][40] | — | 没有 Wine → Mac 的拖出（未见 NSDraggingSource）[推断] |
| 启动器与文件关联 | `winemenubuilder` 只写 XDG 的 `.desktop`/mime [66] | 在 `~/Applications/CrossOver` 生成启动器，出现在 Launchpad（报告 01） | 生成 .app：包含 icns、`CFBundleDocumentTypes`、`CFBundleURLTypes`；默认处理程序用 `NSWorkspace setDefaultApplication…`（`LSSetDefaultHandlerForURLScheme` 已于 macOS 12 废弃）[78] |
| Dock | 应用名取 `[NSBundle mainBundle]` 的 `CFBundleName`，图标取自 exe [35] | Hack 22144（按应用命名的链接）、22310（AUMID 分组和联动退出）、24141（不创建 Dock 图标）、25964（Tahoe 遮罩图标）[83] | 每个启动器放一个真正的 bundle |
| 托盘 → 菜单栏 | `NSStatusItem`（`cocoa_status_item.m`）；10.0 起可用 `NoTrayItemsDisplay=1` 关闭 [10] | — | 气泡提示不支持（Bug 34645，`systray.c` 不处理 `NIF_INFO`）；Windows 菜单没有集成到 Mac 菜单栏（Bug 40642）[68] |
| 通知 | `windows.ui` 源码中没有 Toast 相关实现 [69] | 未知 | 通过 Cider.app 的 `UNUserNotificationCenter` 转发（它需要 bundle）[79] |
| 打印 | `winspool` 通过 dlopen 使用 libcups（`cupsGetDests`、`cupsGetPPD3`）；Mac 默认纸张由 `PMSessionDefaultPageFormat` 取得；`wineps.drv` 输出 PostScript [70] | CX 23.6 修复了 Sonoma 上的打印（报告 01） | 测试无驱动的 AirPrint/IPP Everywhere 打印机 |
| 链接与 URL | `winebrowser` 先试 `xdg-open`，再用 `/usr/bin/open`，所以会打开 Mac 默认浏览器 [67] | — | 自定义协议（OAuth 回跳，如 `com.epicgames.launcher://`）需要启动器声明 `CFBundleURLTypes`，再转发给正在运行的 bottle |

### 7. 网络

- **Winsock [已证实][73]**：`ws2_32` 调用 AFD ioctl，由 wineserver 的 `sock.c` 实现。macOS 专属处理有：UDP 设置 `SO_REUSEADDR` 时一并设置 `SO_REUSEPORT`（允许多个实例绑定同一个广播端口）；`IP_PKTINFO` 用 `IP_RECVDSTADDR` 模拟；`sndbuf=0` 特殊处理；`TCP_INFO` 用 `TCP_CONNECTION_INFO` 代替。IPv6 支持 `IPV6_V6ONLY`；11.0 实现了 ICMPv6 ping [11]。
- **本地网络隐私（macOS 15+）[已证实][71][84][97]**（独立核查已确认本节要点；TN3179 的修订日期包括 2024-10-31、2025-07-18、2026-02-17）：

| 操作 | 是否需要授权 |
|---|---|
| 发起 TCP 连接到局域网地址、发 UDP 单播、connect UDP | 需要 |
| 监听并接受 TCP、接收 UDP 单播 | 不需要 |
| 收发 UDP 组播或广播（包括 255.255.255.255 与 224/4） | 需要 |
| 解析 `.local`、Bonjour 注册/浏览/解析 | 需要 |

  要点：
  - launchd 守护进程、root 进程，以及**从终端或 SSH 运行的命令行工具及其子进程**会自动放行。
  - 系统通过“负责代码”（responsible code）确定授权对象，由 App 拉起的辅助进程归到这个 App 名下。
  - 身份依据是代码签名和主可执行文件的 UUID；UUID 缺失或与其他程序重复都会导致异常。
  - macOS 上**无法把授权重置为“未决定”**（FB14944392）。
  - macOS 15.5 起可用 `AllowedEthernetLocalNetworkAddresses`/`AllowedWiFiLocalNetworkAddresses` 豁免网段。
  - 在 macOS 上组播**不需要**额外的 entitlement。
- **防火墙 [已证实][72]**：有 CA 签名的下载应用可以自动加入允许列表。所有 bottle 共用一个 wine 可执行文件，所以只会有一条规则 [推断]。
- **UPnP [已证实][74]**：`hnetcfg` 的 `IUPnPNAT` 是真实实现（SSDP 发现网关，再通过 WinHTTP SOAP 调用 WANIPConnection）；`dpnhupnp`（DirectPlay NAT Helper）是 stub；`INetFwPolicy` 系列多是 FIXME 并返回 S_OK。SSDP 组播同样受本地网络隐私限制 [推断]。
- **局域网联机**：广播发现依赖本地网络授权。macOS 没有 IPX，老游戏只能借助第三方 ipxwrapper 把 IPX 封装进 UDP [推断，未核实]。Wine 11.0 的蓝牙栈只支持 Linux BlueZ [11]。

## 对 Cider 的启示与建议

**P0（M0–M2，决定架构）**

1. **所有 Wine 进程都在一个签名 .app 内运行。** 例如 `Cider.app/Contents/Resources/Runtime.app/Contents/MacOS/wine`，或者每个启动器各自一个 bundle。原因是 winemac 读取的是 `[NSBundle mainBundle]`，而 TCC 和本地网络隐私按负责代码与签名识别身份。
   - Info.plist 至少包含：`NSMicrophoneUsageDescription`、`NSCameraUsageDescription`、`NSLocalNetworkUsageDescription`、`NSBonjourServices`（按需）、`CFBundleName`。在启动器 bundle 中按游戏设置 `NSPrefersDisplaySafeAreaCompatibilityMode`；`LSApplicationCategoryType=public.app-category.games` 对 Game Mode 是否有作用**待验证**。
   - 权利：`com.apple.security.device.audio-input`、`com.apple.security.device.camera`。
   - 每个可执行文件都要有唯一的 LC_UUID，并用 Developer ID 签名。
   - 首次运行时主动连一次局域网地址，**在向导里触发本地网络授权弹窗**，不要等到游戏里才弹。
2. **Wine 子进程的环境变量完全由 Cider 生成。** 按 bottle 或应用设置 `LANG`/`LC_ALL`/`LC_MESSAGES`。UI 上提供“区域：跟随系统 / 简体中文(936) / 繁体中文(950) / 日文(932) / 韩文(949)”，相当于内置 Locale Emulator。（核查补充）不要让继承来的 `LC_CTYPE=UTF-8` 这类裸字符集值漏进子进程，否则系统区域会落回 en-US（ACP 1252）。选“跟随系统”时，也应由 Cider 根据 macOS 地区算出完整的 `xx_YY.UTF-8` 再传给 Wine（见 4.3）。
3. **中文字体默认配置。** 建前缀时写入 `HKCU\Software\Wine\Fonts\Replacements`：
   - SimSun、NSimSun → Songti SC
   - Microsoft YaHei(UI)、DengXian、SimHei → PingFang SC
   - MingLiU、PMingLiU、Microsoft JhengHei → PingFang TC 或 Songti TC
   - MS Gothic、Meiryo、Yu Gothic → Hiragino Sans
   - Gulim、Malgun Gothic → Apple SD Gothic Neo

   同时给 Tahoma、MS Shell Dlg、Segoe UI 写入 `FontLink\SystemLink`，指向这些 Mac 字体文件。`file,face` 的写法在 Mac 字体路径下是否生效**需要实测**。不打包大型 CJK 字体。

   （核查更正）以 Replacements 为主，SystemLink 只作补充。Wine 内部的默认链接按“替换后的字体族名”解析，只要写好 `SimSun`、`Microsoft YaHei UI` 等别名，内置指向 SIMSUN.TTC、MSYH.TTC 的链接就能生效。注册表 `SystemLink` 的 `file,face` 条目要靠 `find_face_from_filename()` 匹配文件名（如 `PingFang.ttc`），能否匹配仍需实测。另外，Cider 的 Wine 构建**不要链接 fontconfig**：链接后 Wine 会改用 fontconfig 枚举，放弃 CoreText，PingFang 这类 AssetsV2 字体可能就看不到了 [推断][45][46]。
4. **GStreamer 自建，版本跟随 1.28.x。** 用 cerbero 做裁剪构建：core、base、good（isomp4、matroska、audioparsers、wavparse）、bad（applemedia/vtdec、videoparsersbad）、libav（LGPL）。不包含 GPL 插件和编码器。放进 Frameworks 后做重定位，并设置 `GST_PLUGIN_SYSTEM_PATH` 和 `GST_PLUGIN_SCANNER`。第一阶段 Wine 是 x86_64，GStreamer 至少要有 x86_64 切片，universal 最省事。建立一个“过场动画冒烟测试集”（WMV、MP4-H.264、HEVC、xWMA），并关注 #60307、#60326 这类 MF 回归。（核查更正）在 1.28.x 上，applemedia 还能用 VideoToolbox 硬解 VP9 和 AV1，所以测试集要加上 VP9 和 AV1 样片，并确认 winegstreamer 的管线实际选中了 vtdec；gst-libav 或 dav1d 只作软件回退 [96]。
5. **音频基线不低于 Wine 11.16。** 先吸收 !11982（downmix）和 !11370（跟随默认设备）作为补丁。测试矩阵包括：内置扬声器、AirPods（空间音频开/关）、HDMI 5.1、USB 声卡、蓝牙耳麦的采集。

（核查更正）建议 6 的第一条（随包附带可用的 libSDL2 运行时）提升为 P0。原因见 3.1：默认设置下 Xbox 和通用手柄只走 SDL2，SDL 缺失时这些手柄会整体消失。

**P1（M2–M6，体验对齐 CrossOver）**

6. **输入。**
   - **（核查更正：此条提升为 P0）** 随包附带 **sdl2-compat 2.32.x + SDL3 3.4.x**，作为 `libSDL2-2.0.0.dylib`，上游 winebus 无需改动；同时移植 CW HACK 19629 的震动 hint。默认设置下 Xbox 和通用手柄只走 SDL，libSDL2 缺失或加载失败（包括签名、路径问题）会让它们整体消失，所以启动时要检查 SDL 是否加载成功，失败时在 UI 报错。
   - UI 提供“PlayStation/Switch 手柄按 Xbox 手柄处理”开关。（核查更正）它对应按设备写 `HKLM\System\CurrentControlSet\Services\WineBus\Devices\<VID>/<PID>` 下的 DWORD `Hidraw=0`，不是 `EnableHidraw=0`（后者只能强制启用）。全局 `DisableHidraw=1` 会连带关闭飞行摇杆和方向盘，只作最后手段。Joy-Con L 用同一机制补上 hidraw 偏好，同时把 PID 笔误提交给上游。
   - 键位预设：游戏预设为 Cmd=Alt、Option=Alt；办公预设为 Cmd=Ctrl。移植 Hack 10912 的 `EditMenu`。
   - 把 !11799（GCMouse）和 !11880（透明光标）作为可选补丁，在 Overwatch、Marvel Rivals 上用 120Hz 的 M3 MBP 做回归测试。
7. **IME。** 跟进 !12164。建立测试集：记事本、Office、Unity/UE 输入框、一款国产网游、Steam/CEF 聊天框。输入法覆盖系统拼音、双拼、搜狗、微信输入法、鼠须管，每项检查三点：候选窗位置、预编辑显示、按住 WASD 时有没有丢键。
8. **显示。**
   - bottle 级的“高分辨率模式”同时设置 `RetinaMode=y` 和 `LogPixels=192`，对应 CrossOver 的做法；不要做成按应用的开关，因为源码只从前缀全局键读取。
   - 游戏类 bottle 默认开启 `EmulateModeset=y`，并参考 Hack 18576。
   - HDR 提供按游戏的“对游戏隐藏 HDR”开关，因为 Wine 在支持的显示器上总是上报 HDR。
   - VRR 和帧节奏在 DXMT 的呈现路径里用 `CAMetalDisplayLink` 实现。
9. **桌面集成。**
   - 启动器生成器：从 exe 提取图标生成 icns，写入文档类型和 URL 类型，由一个很小的 Mach-O 入口把 `open` 事件转发成对应 bottle 里的 `wine start`。
   - 剪贴板图片补丁：PNG/TIFF ↔ DIB。
   - 气泡和 Toast 转发到系统通知。
   - 每个应用独立的 Dock 名称和图标（Hack 22144、25964）。

**P2（M6+）**

10. 硬解：给 wined3d 写一个 VideoToolbox 解码后端，参考 11.16 VA 后端的结构；或者在 DXMT 里实现 `ID3D11VideoDevice`。备选是给 winegstreamer 增加 `vtdec` 的零拷贝输出。
11. 从 Wine 拖出到 Finder（`NSDraggingSource`）、映射 Windows 菜单栏、动态空间音频对象（映射到 PHASE 或 AVAudioEnvironmentNode）、改进 TSF。
12. 回馈上游：剪贴板图片、字体默认值和本地网络文档都适合提交给上游，可以减少 Cider 自己维护的补丁队列。

## 风险

- **多个关键修复还没合入**（!11799、!11880、!11982、!11370、!12164），Cider 需要长期 rebase 这些补丁，MR 的最终形态也可能变化。
- **私有 API**：`setMouseConfinementRect:`（光标限制）和 `CALayerHost`/`CAContext`（报告 03）可能在 macOS 27 失效。
- **TCC 与本地网络隐私不透明**：签名或 UUID 一变，授权就可能失效，而且无法重置；开发时从终端运行会掩盖问题。
- **GStreamer 的体积与安全更新**：1.28.7 就是安全修复版，Cider 必须跟随发布；重定位后的插件扫描器和 GIO 模块容易出错；Rosetta 阶段需要 x86_64 版本。
- **字体**：PingFang 位于带哈希的 AssetsV2 路径，系统更新后路径会变，所以只能按字体族名引用，不能写死路径。系统字体与 Windows 字体的度量不同，可能导致文字截断。
- **HDR 误报、刘海、分辨率切换**会带来按游戏的配置负担，需要一个兼容性数据库来承载。
- **SDL2 是手柄的单点依赖**（核查补充）：默认设置下 Xbox 和通用手柄只走 SDL2。dylib 缺失、签名不对或加载失败都会让它们整体消失，不会回退到 IOHID。
- **编解码器**：gst-libav 默认带有专利格式的软件解码。按用户偏好这里不展开分析，只把它记为发布渠道需要确认的工程项。

## 未解问题

1. 系统如何为非 bundle 的 Wine 进程判定 Game Mode 资格？`LSApplicationCategoryType` 放在启动器 bundle 里是否有效？
2. GCMouse 或 GCKeyboard 是否需要输入监控授权？MR 的讨论需要登录才能看到，未读到。
3. macOS 的“按住显示重音字符”（ApplePressAndHold）会不会干扰 Wine 窗口里的长按按键？
4. 用 Mac 字体文件名写 `FontLink\SystemLink`（例如 `PingFang.ttc,PingFang SC`）能否被 Wine 的 `find_font_link` 正确解析？（核查补充）源码显示，注册表里的文件条目靠 `find_face_from_filename()` 按文件名匹配；内部默认链接按族名解析，可以用 Replacements 满足。文件名能否匹配 AssetsV2 路径下的 `PingFang.ttc`，仍需实测。
5. DXMT 和 D3DMetal 对 `ID3D11VideoDevice`（DXVA 解码）的支持程度；D3DMetal 是否支持 HDR。
6. CrossOver 如何提供 GStreamer？它的 FOSS 源码清单里没有 GStreamer（报告 02），CX 23.5 的 changelog 只写了 “GStreamer support”。
7. 在 M3 上，GStreamer 能否通过 `vtdec` 或其他元素用上 AV1 硬解（vtdec 的 caps 里没有 AV1）？（核查已解答）能。GStreamer 1.28 起，vtdec 以运行时注册的“补充解码器”支持 VP9 和 AV1 硬解，所以静态 caps 里看不到 [96]。剩下要实测的是 winegstreamer 的管线会不会实际选中 vtdec。
8. 基于 Chromium/CEF 的启动器（Steam、Epic、EA）在 Wine/Mac 上走 IMM32 还是 TSF？中文输入是否可用？
9. SDL3 的 HIDAPI 在 macOS 上访问蓝牙手柄时，是否会触发 CoreBluetooth 授权？

## 参考来源

1. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winecoreaudio.drv/coreaudio.c — AUHAL、period 与延迟计算、声道布局
2. https://api.github.com/repos/wine-mirror/wine/commits?path=dlls/winecoreaudio.drv — winecoreaudio 2025–2026 提交记录
3. https://gitlab.winehq.org/wine/wine/-/merge_requests/11681 — period frame size 修复（Buckshot Roulette 爆音）
4. https://gitlab.winehq.org/wine/wine/-/merge_requests/11982 — 多声道 downmix（开放）
5. https://gitlab.winehq.org/wine/wine/-/merge_requests/11370 — 虚拟默认端点与设备跟随（开放）
6. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/mmdevapi/client.c — 独占模式、IAudioClient3、loopback
7. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/mmdevapi/spatialaudio.c — ISpatialAudioClient 实现范围
8. https://raw.githubusercontent.com/wine-mirror/wine/master/libs/faudio/Makefile.in （以及 `src/FAudio_platform_win32*.c`）— FAudio 的 PE 构建、WASAPI 与 WMA MFT 调用
9. https://github.com/FNA-XNA/FAudio/releases — FAudio 26.06–26.09 变更
10. https://raw.githubusercontent.com/wine-mirror/wine/wine-10.0/ANNOUNCE.md — HiDPI、FFmpeg 后端（opt-in）、托盘策略
11. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.0/ANNOUNCE.md — 独占全屏、Vulkan Video 硬解、HID、ICMPv6、蓝牙
12. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.18/ANNOUNCE.md （以及 wine-10.20、11.7、11.10、11.11、11.12、11.13、11.15、11.16、11.17 的 ANNOUNCE.md）— 各开发版中与本主题相关的提交
13. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/mfsrcsnk/media_source.c — `DisableGstByteStreamHandler` 与后端选择
14. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winedmo/winedmo.spec — winedmo 只导出 demuxer 函数
15. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winegstreamer/Makefile.in — winegstreamer 的解码器与解析器清单
16. https://raw.githubusercontent.com/wine-mirror/wine/master/libs/ffmpeg/config_components.h — 内置 FFmpeg 的 codec 组件全部关闭
17. https://gstreamer.freedesktop.org/download/ — GStreamer 1.28.7 的 macOS 包
18. https://linuxiac.com/gstreamer-1-28-7-released-with-security-and-playback-fixes/ — 1.28.7 发布（2026-09-07，二手）
19. https://gstreamer.freedesktop.org/documentation/deploying/mac-osx.html — 私有打包、osxrelocator、环境变量
20. https://gitlab.freedesktop.org/gstreamer/cerbero/-/tree/main/packages — cerbero 包划分（codecs-restricted、libav、gpl-restricted、system）
21. https://gitlab.freedesktop.org/gstreamer/cerbero/-/raw/main/recipes/ffmpeg.recipe — FFmpeg 8.1.2 的 LGPL 构建选项
22. https://gstreamer.freedesktop.org/documentation/applemedia/vtdec.html — vtdec 的 caps
23. https://gstreamer.freedesktop.org/documentation/applemedia/index.html — applemedia 插件元素列表
24. https://github.com/Gcenx/macOS_Wine_builds/releases — Wine 11.18 包要求 GStreamer 1.28.5
25. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winebus.sys/main.c — 总线选项与 hidraw 仲裁
26. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winebus.sys/bus_iohid.c — IOHID 只接手柄（避开输入监控）
27. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winebus.sys/bus_sdl.c 与 https://raw.githubusercontent.com/wine-mirror/wine/master/configure.ac — 只支持 SDL2、FFmpeg/GStreamer 检测
28. https://bugs.winehq.org/show_bug.cgi?id=50153 — HID 需要输入监控授权（2021 年修复）
29. https://raw.githubusercontent.com/libsdl-org/SDL/main/src/joystick/apple/SDL_mfijoystick.m — MFi 与 HIDAPI 分工、后台事件、系统手势
30. https://github.com/libsdl-org/SDL/releases 与 https://github.com/libsdl-org/sdl2-compat/releases — SDL 3.4.16、sdl2-compat 2.32.72、SDL2 2.32.10
31. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/winebus.sys/bus_sdl.c — CW HACK 19629（PS 手柄蓝牙震动）
32. https://gitlab.winehq.org/wine/wine/-/merge_requests/11799 — GCMouse 原始输入（开放）
33. https://gitlab.winehq.org/wine/wine/-/merge_requests/11880 — 透明光标规避 macOS 26 帧率锁定（开放）
34. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/cocoa_cursorclipping.m — 两种光标限制实现与辅助功能授权
35. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/cocoa_app.m — 相对鼠标、Cmd-Tab、App Nap、菜单、显示器捕获
36. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/keyboard.c — Mac 键码到 VK 的映射
37. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/macdrv_main.c — Mac Driver 注册表选项与 RetinaMode 的读取范围
38. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/winemac.drv/keyboard.c （以及 macdrv_main.c）— CrossOver Hack 10912 “EditMenu”
39. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/cocoa_window.m — NSTextInputClient、全屏、Retina、CALayerHost
40. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/event.c — IM_SET_TEXT、拖放查询事件
41. https://gitlab.winehq.org/wine/wine/-/merge_requests/9992 （以及 !9260、!4923、!5660、!6647）— winemac 的 IME 演进
42. https://gitlab.winehq.org/wine/wine/-/merge_requests/12164 — 从 Cocoa 主线程投递 IME 更新（开放）
43. https://bugs.winehq.org/show_bug.cgi?id=59737 — 中文 HKL 下 scan code 丢失（经 !11965 在 11.18 修复）
44. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/win32u/input.c （以及 dlls/imm32/imm.c、dlls/imm32/ime.c）— 候选窗位置来源与默认 IME UI
45. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/win32u/freetype.c — `load_mac_fonts`，没有 fontconfig 回退
46. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/win32u/font.c — CJK 默认 FontLink 与 Replacements
47. https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks — `fakechinese` 的字体替换
48. https://github.com/notofonts/noto-cjk/releases — Noto CJK 各打包的体积
49. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ntdll/unix/env.c — macOS 的区域与代码页推导
50. https://raw.githubusercontent.com/wine-mirror/wine/master/po/zh_CN.po （以及 zh_TW.po、ja.po、ko.po）— 翻译完成度（本报告自行统计）
51. https://gitlab.winehq.org/wine/wine/-/merge_requests/9781 — RetinaMode 下显示器尺寸修复
52. https://gitlab.winehq.org/wine/wine/-/merge_requests/9556 — HDR 能力上报与 ADVANCED_COLOR_INFO
53. https://gitlab.winehq.org/wine/wine/-/merge_requests/11850 — 移除 WineDisplayLink
54. https://support.codeweavers.com/en_US/advanced-settings-in-crossover-mac-26 — High Resolution Mode（192 DPI）
55. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/win32u/sysparams.c — `EmulateModeset` 与 AppCompatFlags
56. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/display.c — 刷新率回退到 60 与模式枚举
57. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/winemac.drv/display.c （以及 cocoa_display.m）— CrossOver Hack 18576、20512
58. https://developer.apple.com/documentation/bundleresources/information-property-list/nsprefersdisplaysafeareacompatibilitymode — 刘海兼容模式
59. https://developer.apple.com/documentation/appkit/nsscreen/safeareainsets — safe area
60. https://developer.apple.com/documentation/quartzcore/cametaldisplaylink — CAMetalDisplayLink（macOS 14）
61. https://developer.apple.com/documentation/quartzcore/cametallayer/wantsextendeddynamicrangecontent — EDR
62. https://raw.githubusercontent.com/3Shain/dxmt/main/src/d3d11/d3d11_swapchain.cpp — DXMT 的色彩空间与 HDR 元数据
63. https://github.com/3Shain/dxmt/releases — DXMT v0.72–v0.80（2025-12 至 2026-04）
64. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/clipboard.c — 剪贴板格式与 UTI 映射
65. https://gitlab.winehq.org/wine/wine/-/merge_requests/6717 — winemac 改用 win32u 拖放接口
66. https://raw.githubusercontent.com/wine-mirror/wine/master/programs/winemenubuilder/winemenubuilder.c — 只生成 XDG 条目
67. https://raw.githubusercontent.com/wine-mirror/wine/master/programs/winebrowser/main.c — xdg-open 与 /usr/bin/open
68. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winemac.drv/systray.c — 不处理 NIF_INFO（Bug 34645、40642）
69. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/windows.ui/Makefile.in — windows.ui 的源文件清单（没有 Toast）
70. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/winspool.drv/cups.c — CUPS 与 Mac 默认纸张
71. https://developer.apple.com/documentation/technotes/tn3179-understanding-local-network-privacy — 本地网络隐私（macOS 15）
72. https://support.apple.com/guide/mac-help/change-firewall-settings-on-mac-mh11783/mac — 防火墙对签名应用自动放行
73. https://raw.githubusercontent.com/wine-mirror/wine/master/server/sock.c （以及 dlls/ntdll/unix/socket.c）— Winsock 的 macOS 适配
74. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/hnetcfg/port.c — UPnP NAT 实现
75. https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.device.audio-input （以及 NSMicrophoneUsageDescription 文档）— 麦克风权利与 usage 键
76. https://support.apple.com/en-us/105118 — macOS Game Mode
77. https://developer.apple.com/documentation/gamecontroller/gcmouse — GCMouse（macOS 11）
78. https://developer.apple.com/documentation/coreservices/1447760-lssetdefaulthandlerforurlscheme — 该 API 于 macOS 12 废弃
79. https://developer.apple.com/documentation/usernotifications/unusernotificationcenter — 系统通知
80. https://bugs.winehq.org/buglist.cgi?component=winemac.drv&resolution=--- — winemac.drv 开放 bug 列表（经 REST 查询：34645、40642、42973、53243、56478、60266 等）
81. https://www.codeweavers.com/crossover/changelog — CrossOver 18.5（FAudio）、23.5（GStreamer）等条目
82. https://bugs.winehq.org/show_bug.cgi?id=54831 — macOS 上 GStreamer 初始化崩溃（2023 年修复）
83. https://raw.githubusercontent.com/dappermint/winecx/crossover-26.3.0/dlls/winemac.drv/cocoa_app.m — CW Hack 22310、24141、25964（Dock 与 AUMID）
84. https://developer.apple.com/documentation/bundleresources/information-property-list/nslocalnetworkusagedescription — 本地网络 usage 键
85. https://raw.githubusercontent.com/wine-mirror/wine/master/libs/ffmpeg/Makefile.in — 内置 FFmpeg 只编译 libavutil、libswresample、libswscale（核查新增）
86. https://raw.githubusercontent.com/wine-mirror/wine/master/libs/ffmpeg/VERSION — 内置 FFmpeg 版本 8.1.1（核查新增）
87. https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/ir50_32/Makefile.in — `ir50_32` 导入 winegstreamer；`mp3dmod`、`l3codeca.acm`、`iccvid`、`msvidc32`、`imaadp32.acm` 同在 master 的 `dlls/` 下（核查新增）
88. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.12/ANNOUNCE.md — 内置 libswresample/libswscale，导入 FFmpeg 8.1.1（核查新增）
89. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.15/ANNOUNCE.md — “Prefer hidraw for (some) switch 1 controllers”（核查新增）
90. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.16/ANNOUNCE.md — !11681 的三个提交（核查新增）
91. https://raw.githubusercontent.com/wine-mirror/wine/wine-11.17/ANNOUNCE.md — Disable Hidraw 开关同样影响 IOHID 后端（核查新增）
92. https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10 — SDL2 2.32.10（2025-09-01）（核查新增）
93. https://gitlab.winehq.org/api/v4/projects/wine%2Fwine/merge_requests/11681 （以及同一路径下的 `/commits`）— !11681 的创建、合入日期与提交列表（核查新增）
94. https://gitlab.winehq.org/api/v4/projects/wine%2Fwine/merge_requests/11982 — downmix MR 的状态（opened，2026-09-24 更新）（核查新增）
95. https://gitlab.winehq.org/api/v4/projects/wine%2Fwine/merge_requests/11370 — 默认设备 MR 的状态（opened）（核查新增）
96. https://gstreamer.freedesktop.org/releases/1.28/ — GStreamer 1.28 发布说明：applemedia 的 VP9/AV1 硬解与 vtdec 补充解码器（核查新增）
97. https://developer.apple.com/tutorials/data/documentation/technotes/tn3179-understanding-local-network-privacy.json — TN3179 的 JSON 版（含修订日期）（核查新增）

本机实测：macOS 26.5（25F71）上用 `swift` 读取 NSScreen/CGDisplayMode 属性，用 `ls /System/Library/Fonts` 查看字体、用 `find /System/Library/AssetsV2` 定位 PingFang，用 `locale -a` 列出区域。

## 事实核查记录

独立事实核查共检查 10 条声明：3 条证实，5 条部分成立，2 条被推翻。本批没有因“法律分析不在范围内”而跳过的条目。

| 声明 | 结论 | 说明/更正 |
|---|---|---|
| 截至 master/11.18（2026-09-18），MF 默认用 GStreamer；winedmo 只做解复用；11.12 内置的 FFmpeg 8.1.1 只启用 libswresample/libswscale；**所有**音视频解码器都在 winegstreamer | 部分成立 | MF 默认值、winedmo（链接宿主 FFmpeg）、内置 FFmpeg（只编译 libavutil/libswresample/libswscale，`CONFIG_*` 全为 0）都正确。“所有解码器”言过其实：`mp3dmod`、`l3codeca.acm`、`iccvid`、`msvidc32`、`imaadp32.acm` 是 Wine 自带的实现，`ir50_32` 导入 winegstreamer。结论不变，仍须打包 GStreamer。已改摘要与 2.1 [13][14][16][85][86][87][88] |
| 上游 winebus 只支持 SDL2；IOHID 只接 Joystick/GamePad；DS4、DualSense、Switch Pro、Joy-Con 默认走 hidraw；SDL 3.4.16 与 sdl2-compat 2.32.72 发布于 2026-09-02 | 部分成立 | 除 Joy-Con 外都正确。只有 Joy-Con R（057e:2007）默认走 hidraw；Joy-Con L 的判断写成了 `pid == 0x057e`，默认仍走 SDL。注册表键为 `Services\WineBus`，按设备覆盖用 `Devices\<VID>/<PID>` 下的 DWORD `Hidraw`。已改摘要、3.1、3.3 [25][26][27][30][89][92] |
| macOS 上 Unix 代码页固定为 CP_UTF8；系统区域先取 `LC_CTYPE`，空时用 CFLocaleCopyCurrent；用户区域先取 `LC_MESSAGES`，其次 CFLocaleCopyPreferredLanguages 首项 | 证实 | 核查补充两点：值为 `C` 时改读 `LC_ALL`；`LC_CTYPE=UTF-8`（裸字符集名）会跳过 CFLocale 回退，系统区域落回 en-US（ACP 1252）[推断，高置信]。已补进摘要、4.3、乱码表和建议 2 [49] |
| macOS 上 Wine 没有 fontconfig 回退；字体经 CTFontCollectionCreateFromAvailableFonts 加载；CJK 默认 SystemLink 指向 SIMSUN.TTC、MSYH.TTC | 部分成立 | 代码事实正确，但用不用 fontconfig 是**构建时**选择，不是 macOS 固有属性。链接 fontconfig 的构建会改用 `load_fontconfig_fonts()`、不再走 CoreText。内部默认链接按替换后的族名解析，Replacements 别名即可满足；注册表文件条目需要 `find_face_from_filename()` 匹配文件名。已改摘要、4.2、建议 3、未解问题 4 [27][45][46] |
| winecoreaudio 从 11.16 起（07c7fcc8/da0b0847，2026-08-16，!11681）设置 BufferFrameSize 并返回实际 period；!11982（2026-09-15）和 !11370（2026-07-12）仍未合入 | 证实 | MR 于 2026-08-17 合入，wine-11.16 tag 为 2026-08-21；两个开放 MR 状态确认。附注：!11370 作者 Rhodri Richards 的 CodeWeavers 所属**未核实**，已改 1.2 的推断 [90][93][94][95] |
| TN3179：本地网络隐私始于 macOS 15；局域网 TCP 连接、UDP 单播、组播/广播需要授权；终端/SSH 运行的命令行工具及子进程自动放行；按负责代码、签名和主可执行文件 UUID 识别；macOS 上无法重置（FB14944392） | 证实 | 无需更正；补充了 TN3179 JSON 来源和修订日期 [71][97] |
| （2.3/摘要）vtdec 支持 H.264、HEVC、MPEG-2、JPEG、ProRes，不支持 VP9 和 AV1，这两种只能软解 | 推翻 | GStreamer 1.28 系列（1.28.0，2026-01-27）给 applemedia 加入了 VideoToolbox 的 VP9 和 AV1 硬解，以运行时注册的补充解码器实现，所以静态 caps 里看不到。已改摘要、2.3、2.4、建议 4，未解问题 7 标为已解答 [22][96] |
| （3.1/3.3/建议 6）按 VID/PID 配置 `EnableHidraw`；让 PS/Switch 手柄按 Xbox 处理可设 `EnableHidraw=0` 或 `DisableHidraw=1` | 推翻 | `EnableHidraw` 是 `VVVV:PPPP` 的 REG_MULTI_SZ 列表，只能强制启用，写 0 无效。正确做法是 `WineBus\Devices\<VID>/<PID>` 下 DWORD `Hidraw=0`。`DisableHidraw=1` 虽然有效，但会关掉整个 IOHID 总线（飞行摇杆、方向盘一起失效）。已改 3.1、3.3、建议 6 [25][91] |
| （3.1/3.3 隐含）IOHID 是手柄的可用后端，SDL 只是可选增强 | 部分成立 | [推断，高置信，基于源码] 默认设置下 Xbox 和通用手柄的 IOHID 实例会被丢弃，必须依赖 SDL2；libSDL2 缺失或加载失败时手柄整体消失。已补进摘要、3.1、3.3、风险，建议 6 的 SDL 一条提升为 P0 [25][26][27] |
| （3.3 表格）Switch Pro/Joy-Con 走 IOHID raw（11.15 起偏好 hidraw） | 部分成立 | 11.15 起只有 Switch Pro（057e:2009）和 Joy-Con R（057e:2007）默认走 hidraw；Joy-Con L 因 PID 笔误仍走 SDL，拆开使用时左右不一致。建议提交上游修复，或在 Cider 里用 `Devices\057E/2006` 覆盖（0x2006 待核实）。已拆分 3.3 表格行 [25][89] |
