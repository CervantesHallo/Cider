# HY1 快捷方式参数修正与执行检查

2026-10-11，用户要求确保方向正确并严格继续原计划。本轮从HY1入口审查收尾开始，参考research/35、36、41；没有开展Windows工作、重试RDP或启用额外代理。

## 方向与范围

原目标继续是柚子社全系及米哈游三款官方Windows国服的本地兼容；当前按HY1→HY2→HY3→HY4/HY5推进。HY1的Retina真实点击、全部入口与首次安装显示仍有未验项；HY2的Win32对照不能替代kernel/context结果；HY3启动许可闭环及三款实际会话尚未完成，游戏门禁保持。公网接入是参考环境前置，本轮不继续占用它。

## 源码确定的缺口

`AppCatalog.shortcutPrograms`将ShellLink的目标与参数分别送入lifecycle。旧`splitArguments`按Character遍历，只认空格、切换双引号状态并跳过空结果。因此`""`参数丢失、Tab没有拆分、反斜杠引号被错误解释，带引号的路径或字符串可能变化。此为静态确定的解析缺陷；未复现某款启动器因此失败，不把它解释成此前WMA/Steam挂起的根因。

## 采用的修正

新增CiderCore.WindowsArgumentParser，AppCatalog只委托此函数；依据[微软C运行时参数规则](https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments?view=msvc-170)实现参数尾的Space/Tab分隔、引号组合、反斜杠与引号的奇偶关系和空参数保持。以Unicode scalar识别ASCII语法并保留其他文字，遇第一个NUL停止，避免不可达尾部转成额外POSIX参数。

可执行目标在快捷方式中单独解析，故不套argv[0]规则。不是[CommandLineToArgvW](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-commandlinetoargvw)完整克隆，也不解释cmd.exe/PowerShell语法。未知产品自有命令行规则仍需按具体应用对照。

收益：快捷方式参数不再因旧拆分逻辑丢失/变形，支持同一lifecycle入口。成本：原来含转义字符的快捷方式将按明确规则解析，需实际运行回归。保存启动器的typed参数数组、环境/cwd/profile、进程归属、同步及门禁源码均未更改。回退可整体撤回本独立解析提交；旧app包已在私有out/backups保留。

## 对抗性自审与实际构建

- 空参数不依赖输出字符串长度判断，已开始的参数即使为空也写入数组；纯空白没有参数。
- 每个内层路径要么前进索引、要么遇边界跳出；外层消耗分隔符，没有空串/未闭合引号的停滞循环。
- 连续反斜杠只在紧接引号时特殊处理；尾部及普通字符前保留。相邻引号只在已经引号包围时产生字面引号，不全局替换。
- 使用标量避免把引号与后随Unicode组合字符当成一个语法单位；不将任意Unicode空白泛化为Windows分隔符。
- 不执行参数内容；Spawn仍传argv而非调用宿主shell。NUL终止是审查后补的边界，不扩展程序启动许可。

最终源码的CLI `swift build`退出0（1.39秒）、App `swift build`退出0（1.66秒）；`scripts/build-app.sh debug`完成debug app打包/ad-hoc签名（复用构建0.50秒）。产物为out/Cider.app；没有重启当前GUI/Wine或操作用户游戏会话。未新增/运行测试套件或Windows参考，源码和构建结果不能标为快捷方式真实运行、Retina交互或内核兼容通过。

源码SHA256：

```text
85c6d864f0e3ce156a3461faa86c44ad1fdd678f54c3b211bedc360fc67d1907  WindowsArgumentParser.swift
c767c1eb45bbe1197f33c82986b87153e644c5d3e3f8b011dfe2b0e06a9f7b71  AppCatalog.swift
```

## 后续检查点

先补HY1真实特殊参数/Retina/入口显示证据，再按research/28、33、42的依赖组推进HY2。共享token读锁需覆盖真实写入口、非阻塞延期回复及失联清理，不直接把宿主互斥或永久副本当成Windows锁。Windows原生kernel输出缺失前不提升权限API、KMDF或三款游戏的兼容状态；research/45的未结算预约继续阻止Windows操作，Mac源码准备可继续。
