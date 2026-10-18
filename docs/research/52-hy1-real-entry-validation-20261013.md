# HY1 真实入口与显示验收

2026-10-13 整理。原始会话名称/证据按实际捕获时间保留。继续51的已授权重启/入口验收；用户确认没有下载或更新任务后才停止启动器。原HY順序保持，未使用Windows主力机。

## 实际回归与修复

51的严格工作目录校验在真实快捷方式上造成回归：新GUI首次重开显示“工作目录路径无效”。桌面和开始菜单的两份启动器链接flags=0x93，Unicode WORKING_DIR计数33，实际目录32个字符后还包含一个NUL。旧解析器保留它，51的新校验拒绝启动。这是本次新版本的实际失败，不能只记编译成功。

依据[MS-SHLLINK StringData](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-shllink/17b69472-0f34-4bcf-b290-eccdb8de224b)，计数字符串不应包含终止NUL；此处采用有证据的窄兼容取舍：完整消费原计数字段，仅移除一个末尾NUL；中间NUL、双末尾NUL、无法解码/截断的已声明字段拒绝整个链接，不再把坏cwd变成缺失字段。没有改写用户快捷方式、放宽Spawn检查或隐藏Wine。其余StringData ANSI代码页、复杂路径/IDList等不是本轮完整实现或验收。

本轮还实际发现，独立调试CLI从Tools/ciderctl运行时未找到当前兼容数据；初次恢复会话的自动profile和DXMT变量均为空。该会话已停止。build-app.sh现在将既有CLI放入App的Contents/MacOS，与GUI共用Resources/data，并分别签名CLI和App。从/tmp调用实际列出五个配方，启动记录及HYP/helper的环境随后与GUI一致；没有依靠临时切回仓库根目录来冒充任意cwd分发支持。独立调试CLI仍须仓库根/CIDER_DATA，README/AGENTS已说明。

采用收益是修复真实重开失败和打包CLI的数据缺口；成本是兼容一个非规范的终止符、额外CLI构建及约6.6MiB的本机debug可执行文件。回退可撤回两个独立改动，旧App包在out/backups/hy1-link-cli-20261012-UX7YNW/Cider.app。源码自审检查计数偏移不随清理改变、缺失必需字符串整体失败、CLI在资源复制前构建完成、内外签名顺序。未新增合成夹具或运行测试套件。

## 本轮实际验收

| 范围 | 实际证据 | 状态 |
|---|---|---|
| GUI停止/资料库启动 | 原HYP+五helper退出，快捷方式修正后创建新HYP，界面报告已开始运行，正确cwd/profile记录 | 本轮通过 |
| 打包CLI，从/tmp运行 | profile.launcher.mihoyo-cn rev4及相同内容摘要；DXMT=1/msync=1/zh_CN.UTF-8在HYP与本轮四个helper实际读回 | 本轮通过 |
| GUI重启 | 旧主/辅助PID不再存在，新主/辅助PID创建；消息已开始运行 | 本轮通过 |
| 重复GUI启动 | 已在运行提示，主PID及最新session保持，没有新顶层启动记录 | 本轮通过 |
| 标准显示 | 用户确认画面完整、设置及最小化恢复正常 | 用户验收通过 |
| Retina/运行命令入口 | GUI临时开启；配置1、Desktop DPI192、RetinaMode=y读回；HYP+五helper创建、自动profile及环境记录；用户确认顶部/按钮/拖动正常 | 用户验收通过 |
| 恢复原显示 | GUI恢复off，配置0、DPI96、RetinaMode=n读回；资料库正常重开，HYP+五helper存活 | 操作/进程通过；没有额外最终截图 |

GUI及CLI相同profile摘要为91ee46130c107bda2994e640611b55243b8aae1b77399ac5827563798ba398ae。它是已解码对象的序列化摘要，不是来源签名。normal/Retina下helper数量可变，不要求每次恒为五个。Steam主PID97843全程保持；没有启动其登录副本或改其设置。

CLI最终构建3.02秒、App3.20秒均退出0；打包复用构建0.49/0.35秒，App及CLI严格codesign验证退出0。实际观察与失败保留在[结构化证据](evidence/hy1-shortcut-packaged-cli-20261012.json)。Wine窗口工具仍无法独立取得画面；视觉/按钮是用户回执，过程/环境是本机实读，二者区分记录。

## 下一缺口

本轮覆盖当前HYP1.18.0.380、r2、原瓶子已有安装和上述三入口。保存启动器、首次安装器子树/首渲染、特殊参数及长期/其他版本仍需各自验收，HY1整体仍为部分完成。没有直接观察guest CurrentDirectory，不把host cwd请求/进程环境当完整Windows契约。

下一步用全新无账户瓶子准备首装入口，不克隆已登录Steam。Windows内核/context/KMDF、HY3许可闭环和三款游戏会话均未因启动器验收提升；门禁保留。定时任务仍PAUSED，Windows超额预约/预算保持，没有Windows操作、Actions或额外代理。
