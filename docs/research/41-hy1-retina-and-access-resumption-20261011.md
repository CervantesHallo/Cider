# 原队列恢复：HY1 Retina显示与接入检查点（2026-10-11）

用户确认柚子社专项本轮结束，恢复HY1→HY2→HY3→HY4/HY5。视频有效帧及长期回归保留为质量待办；原生Win32、WMA及mixer已通过的基线不重跑。原Goal工具仍返回paused且没有active恢复接口，按聊天授权推进并同步原heartbeat，不创建重复Goal。

## HY1本轮真实操作

用户确认米哈游启动器当前正常、没有下载/更新，并允许必要检查。只操作米哈游瓶子bottle-ca8f，r2、zh_CN.UTF-8和默认msync。

| 阶段 | 实际证据 | 边界 |
|---|---|---|
| 原模式 | Cider高分辨率开关off，用户报告正常；此前完整截图、设置/最小化/拖动验收已有记录 | 原配置未显式存highResolution；不把缺值当成已测得像素缩放倍率 |
| 开启Retina | CUA停止原HYP49797，UI确认停止；在明确选中米哈游后开启，配置highResolution=1、RetinaMode=y、Desktop/Fonts LogPixels=192；经资料库重开，HYP3791及helpers出现 | 这是设置与进程证据，不能单独替代画面 |
| 实际画面 | 用户上传2662×1528完整启动器窗口，顶部/底部内容、设置/最小化/关闭和开始游戏按钮完整，无此前顶部裁切 | 静态布局通过；截图不能证明本轮真实点击、拖动或原神游戏可玩 |
| 恢复原模式 | CUA停止Retina实例，明确重新选中米哈游后关闭；highResolution=0、RetinaMode=n、Desktop/Fonts LogPixels=96；资料库重开，HYP5591及helpers出现 | 恢复原显示行为，不称注册表逐字节恢复；未停止Steam或启动游戏 |

资料库入口实际启动目标是根目录launcher.exe，随后生成版本目录HYP.exe。按HYP.exe筛选spawn审计会漏掉bootstrap记录，不能当作审计未写入。最近对应session.json显示DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1、WINEMSYNC=1、LC_ALL=zh_CN.UTF-8。

使用只读Darwin KERN_PROCARGS2检查恢复后的HYP5591与五个HYPHelper（5605/5607/5610/5612/5626）：全部实际携带上述三个值。探针仅在私有缓存，先核对CIDER_BOTTLE，只输出三项允许字段；不保存完整环境。ps eww未取得这些字段，不作为变量不存在的证据。此项验证GUI→bootstrap→HYP/CEF继承路径，不覆盖保存启动器、自由运行命令和首次安装全部入口。

截图保留私有，公开只记录SHA256：Retina图e50d83b94e7aa84346480086ccef0443dee3e5da4b78a8195d7a9cb81a94704f。

## Windows接入的新信息

用户确认Windows代理为Clash Verge；上传首页实际显示Verge v2.2.2、Mihomo v1.19.3、Windows11 Pro、服务模式、混合代理端口7897。该图不是TUN设置页，没有stack、auto-route、strict-route或路由排除项，不能据此认定TUN导致公网SSH超时。首页图874×701，SHA256为64d2555d92c97241df29137a4b75c895b80cb995c7d525550b66150a3a48991f；不公开原图。

当前只读Windows查看预扣准备180秒，预约7ab55f34-be07-4136-8ef3-69bb539737ff仍pending；剩余准备1740秒、测试3600秒，不退款或新开远程批次。等待TUN设置页及查看结束回执，随后在现有预算内判断下一步。没有关闭TUN、改代理/路由/防火墙、重新安装SSH或重复拨号。此前Mac DIRECT与Windows本机监听已取得证据，公网端到端仍未建立。

[Mihomo TUN配置](https://wiki.metacubex.one/config/inbound/tun/)区分自动路由和路由排除项；官方最新说明不能自动当成用户1.19.3的实际配置。采用配置修正前仍须核对版本支持、当前设置及对应路由。用户终端代理继续保留。

## HY2旁路交付与复核

只启用一个有界代理检查wineserver共享token锁依赖，产出[42](42-token-locking-dependency-audit-20261011.md)，已关闭。父代理独立核对十份源码SHA256及adjust_token_privileges、请求立即回复、kill_thread清理这三条关键链；其他入口和具体并发方案仍按报告保留待实现/验证。

关键约束：锁须覆盖共享token真实修改；server请求不能阻塞事件循环；延期输入/对象引用与持有者失联释放要一起处理。报告是源码事实和提案，不是运行时实现或原生kernel验收，不提升游戏门禁。HY1完整入口回归与Retina真实点击仍有缺口，HY2继续准备真实契约与依赖实现。
