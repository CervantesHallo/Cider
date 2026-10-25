# R v1：注册表回调真实往返与安全属性修复

日期：2026-10-25。接续 [95](95-r-v1-legacy-window-bridge-20261025.md) 的回调运行缺口。操作均在 Mac 私有 GUI/XPC 实验环境，普通用户引擎和瓶子未切换。

## 发现与实现

真实 native → PE 往返首次完成五个 API、131073 字节读写和 33 项检查，却有 4 项失败。定向观察确认两个继承标志和绝对/自相对描述符的 DACL 没有保留。回调已正确封送，固定 Wine 11.18 的 `kernelbase/registry.c` 则未将 `RegCreateKeyExA/W` 的 `sa` 传给 `NtCreateKey`。

新增 **0018** 使真实 OBJECT_ATTRIBUTES 接收描述符和 `OBJ_INHERIT`，递归中间键仍使用默认属性，最后实际路径组件接收调用者属性。视图重定向/已存在键的返回句柄用真实 `NtSetInformationObject` 设置继承，保留实际失败及关闭路径；不修改已存在键的 ACL，也没有伪造查询结果或跳过权限检查。接口依据是微软 [RegCreateKeyExA](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regcreatekeyexa) 和 [InitializeObjectAttributes](https://learn.microsoft.com/en-us/windows/win32/api/ntdef/nf-ntdef-initializeobjectattributes)。

源审发现 P2：单看当前分隔符是否到末尾，会将含尾部重复分隔符的真实叶键当作中间键，仍丢描述符。最终版先跳过分隔符串，再判断是否有下一个组件。限定复审确认该项闭合、无新增具体 P1/P2；同一公开探针的旧/新运行对照也拒绝这个反例。

这补齐了 95 所述回调语义的底层缺口。95 当时只有源码/构建，不能倒推当时已经实际保留安全属性。

## 实际结果

| 范围 | PE64 | PE32 | 结论 |
|---|---|---|---|
| 五个回调、大值、3 种安全属性、两次清理故障 | 33 检查、0 失败 | 33 检查、0 失败 | 本轮局部通过 |
| A/W、递归/重复尾部分隔符、三种视图、新/已有键 | 195 检查、18 案例、0 失败 | 195 检查、18 案例、0 失败 | 本轮局部通过 |
| 已存在树保护、版本错误拒绝 | 两个精确负例均通过 | 两个精确负例均通过 | 原 Sentinel 保持，未发生未拥有的修改/删除 |
| 未修尾部分隔符的候选，同一 EXE | 195 检查、12 失败、CLI 1 | 未重复 | 旧缺陷被明确拒绝 |

普通往返中，native 提交和 PE 入口的 QUERY/SET/OPEN/CREATE/CLOSE 计数均为 **[3,1,2,5,7]**。诊断只在本 fixture 线程两次阻止真实 VM 释放，真实 Open/Create 结果和 HKEY 仍交付；pending owner 在两次失败后分别为 1，后续 Close 后归零。对重试前后两端计数的检查确保没有重放注册表操作。

五个故意错误的私有 native 变体分别重复 CREATE、只做部分检查、修改已有树、忽略版本、返回无关错误。严格客户端全部以 CLI 1 拒绝，并完成本次拥有的清理。它们证明检查器能拒绝这些具体错误，不是额外的实现能力。

## 对抗性审查与失败保留

- 夹具首稿可能操作未拥有的 nonce 树，且只在清理重试前检查计数；均已修正。只有确认新建才允许修改和删除，并同时检查 PE/native 完整计数。
- 负例首稿把任意失败视为成功，已改为精确状态、工作量、计数、pending 和 Sentinel 条件。修后限定源审无新增具体 P1/P2；上述五个实际错误变体验证这些拒绝条件。
- 初次诊断 spec 文件名使导出模块名错误，LoadLibrary 返回 4；显式 `-Fwinemac.drv` 后才进入真实回调。首次加载失败、后来的 4 项安全属性失败及观察日志都保留。
- 观察引擎的 unsigned source SHA 曾继承旧元数据，`registry-observe-staging-binding-v4.json` 明确记录正确 unsigned/signed 摘要和 section 对照；未改写原 stage/run。部分复用 runner 的 `scope` 文字沿用回调描述，直接 API/错误变体的准确范围以原 fixture 类型、实际输出和本报告为准，不能从那段模板文字推定执行了额外检查。

## 构建、身份和清理

0018 在固定源码上 zero fuzz/offset 精确重建；两种 PE 的完整 registry 翻译单元按 Werror 编译并链接 kernelbase。诊断驱动沿用 95 的显式 SDK15.4/min14，签名只修改私有副本；固定 F27 ntdll/wineserver 四摘要保持。最终诊断候选是 `cider-lab-wine11.18-registry-security-r6-x86_64`，不允许普通采用。

父代理离线独立核对最终 14 次实际运行的原始 log/session、18 个 PE 模块和 2 个 native 模块及完整计数；公开夹具与实际输入逐字一致，诊断补丁展开 include 后与执行的翻译单元逐字一致。每批 finally 有 GUI 真实退出、至少两次空观察、无 pending；普通实验配置/DLL恢复，生产 App、Windows账本和两个用户瓶配置四摘要保持。首次独立核对器误把两种负例共用一个 mode 值，按原客户端的 1/2 枚举修正后完成核对，未重跑或改写运行结果。

公开夹具在 `tests/runtime/legacy-registry/` 和 `tests/runtime/registry-security/`，包含五个故意错误的精确 diff。原始资料根为 `out/engine-dxmt-bridge-20261025`；[结构化证据](evidence/r-v1-registry-roundtrip-20261025.json) 绑定摘要和运行清单。

## 取舍与接续

收益是保留原有五个 API、动态数据和完整安全属性能力；成本是额外的真实 kernelbase 参数传递及现有键句柄标志处理。采用标准是局部两位宽运行、错误路径和上游兼容审查；真实 Windows 对照仍是单独门槛。回退保留原稳定引擎，不将诊断导出/注入放入普通引擎。

下一步继续 95 的窗口释放/重建与 foreign-child/CEF。源码、构建和上述局部 Mac 运行已完成；真实 Windows、完整权限/内核、窗口视觉/跨进程、三款本地游戏会话和完整 R v1 仍未验收。U90 原 8/15 单元未过、Windows 忙/待重启、HY1→HY2→HY3→HY4/HY5 和游戏门禁保持。本轮 Windows、Actions、自动化和 Goal 操作均为零。
