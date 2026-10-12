# HY1 安装检测：拒绝没有证据的成功

2026-10-12，延续 [49](49-hy1-install-preparation-20261012.md) 的首次安装可靠性工作。当前仍只完成源码、静态审查和 Mac 构建，没有运行安装或新测试。

## 静态缺陷与已实施修正

| 旧判定 | 修正 |
|---|---|
| 所有 detect 列表为空时，allSatisfy 自动为 true | Detect.hasConditions；数据接纳及直接 isInstalled 调用都拒绝空条件 |
| 缺失 DWORD 使用 `nil ?? 0`，可满足 at_least=0 | 必须实际解析到 UInt32，再比较阈值；存在的零值仍允许满足零阈值 |
| fileExists 同样接受目录 | file 条件必须 stat 得到普通文件；合法 Wine 盘符/符号链接映射仍可读取 |
| 任意文本中出现 `[key]` 就算键存在，DWORD 从该子串后读取 | 从行首匹配完整节名及合法分隔，只在该节后、下一节前查值 |

RecipePolicy.detectionViolation 在数据接纳、49的安装整图预检和直接检测中复用。支持子集为非空条件、非空 any_files、绝对 Windows 盘符文件路径、HKLM/HKCU 及完整名称的注册表根键；拒绝文件 NUL、点路径分量，以及空/多行/含 NUL 的注册表路径和值名。尚不支持的 UNC、相对路径或其他根键明确拒绝，不等待安装结束后才失败。

拒绝点分量是检测边界的具体取舍：现有 hostURL 将 Windows 路径拼接为宿主路径，`C:\..\...` 不能被当成可信检测证据。要求配方先提供规范化盘符路径，暂不自行模拟 Windows 的完整路径归一化。没有修改其他入口的 WindowsPath 行为。

注册表节可有时间戳，DWORD 文本兼容 CRLF 和尾部空白，空名称按 `@=dword:` 读取默认值。依据 [Wine 上游 server/registry.c](https://raw.githubusercontent.com/wine-mirror/wine/master/server/registry.c) 的 dump_value/save_subkeys 格式核对；上游源码核对不等于当前引擎运行证据。Unicode/特殊字符转义名称、隐式父键、即时未刷盘值及完整注册表语法仍不是本读取器已验收的能力。

## 采用取舍、审查和验收边界

收益：安装器不会仅凭空条件、缺失值、目录或文本子串宣称完成；检测源的格式错误在执行前报告。代价：原先没有检测条件、使用目录作为 file 条件或依赖未支持路径/根键的自定义配方会被拒绝，需要补正元数据。已逐份静态查看当前五个 bundled 配方的检测字段，没有添加 Epic 或修改这些配方。

对抗性自审按源码检查：空条件/空 any_files；不存在与真实零 DWORD；目录/普通文件/合法映射；行内相似键名/相邻键/空节/时间戳；CRLF；无效路径/NUL/取消前的整图验证。这些是静态反例分析，不是合成夹具运行或测试通过。

实际安装、读取时并发写入、首装/已装跳过、取消及数据更新回归仍待验收。存在一个文件不是文件来源/版本或实际可执行证明，宿主 stat 不是完整 Windows 文件系统契约；本检测不改变游戏 Verdict 或许可。安装退出码被忽略、primary/handoff 生命周期和重启处理仍按48/49待完成，不能因 detect 更严格而标为闭合。

回退可单独撤回 Recipe 的检测校验及 RecipeInstaller 检测读取改动；没有执行配方、写入注册表或修改用户前缀。

## 本机实际结果

- 最终 CLI swift build：退出 0，3.13 秒。
- 最终 scripts/build-app.sh debug：退出 0，App 构建 3.25 秒；out/Cider.app 已 ad-hoc 签名，未重启应用。上一包在 out/backups/hy1-detect-20261012-TCdtZz/Cider.app。
- git diff --check 通过。没有新测试/安装/Wine/Windows操作或 Actions/子代理。
- 本机 budget status 仍准备 0、测试 3600、同一超额预约 pending；本轮未预约、结算、扩额或连接 Windows。

下一步仍按 HY1 完整入口/显示/首装运行验收 → HY2 原生 kernel/context 依据 → HY3 许可闭环 → 三款会话；构建结果不提升阶段状态。49已提交并推送为9338898，作者/提交者为用户、带skip ci。
