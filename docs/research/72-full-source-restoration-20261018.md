# 全面源码恢复与原计划接续

2026-10-18。用户明确要求全面恢复隔离代码，并再次要求严格按原计划、自建引擎方向推进。此次授权解除此前的源码隔离等待条件；Windows预算、原生验收和游戏许可各自保持。

## 恢复内容

- 已修复快照：`344148a`，其中K2源码为`47c7283`，应用/安装主体候选为`aab2970`。
- App、Packages、Tools、engine、data、scripts、CI、tests、AGENTS全部恢复，包括A/B/C/D；各子树Git对象与修复快照逐项一致。此前缺失的52–59研究/证据、05及14配套文档一并恢复。
- 使用已修后的实现；退休0010只在rejected作历史留存。全部恢复不是重新采用原缺陷稿。
- 恢复前主线保存为`codex/pre-full-restore-20261018`；原隔离分支、修复分支和私有归档继续保留。

## 主线构建与实际界面

在主工作区执行现有`build-app.sh release`，App、bundled CLI和XPC Bridge均构建成功，资源和严格签名检查通过。旧App由原子发布流程保存在`out/backups/Cider-20261018-102701-27Nkbe/Cider.app`；它不是正式Developer ID/公证验收。

Cider主界面从旧PID90909正常退出并以新PID26910重开；Steam主PID91113及原wineserver91115保留。重开后12份瓶子/引擎清单及真实Windows账本摘要未变。原生Cider资料库实际显示5项、Steam仍运行，bundled CLI经新GUI的engine list正常返回。

随后通过真实Cider界面点击现有米哈游启动器，界面先显示等待响应、再转为“已开始运行”。实际HYP PID27008，使用原瓶子的r2引擎；会话记录正确匹配`profile.launcher.mihoyo-cn` revision4，DXMT跨进程环境没有偏离。没有新建/复制登录瓶子，也没有重新下载游戏。此次只证实恢复后的GUI入口/派生/运行状态，不冒称启动器窗口、登录、验证码或完整更新验收。

恢复任务没有新增/重跑测试套件；此前61–71受影响验证按固定源码/产物证据复用。构建、签名、真实界面和命令读取分开记录。Windows/SSH/RDP/VM及Actions操作为0；账本status实读仍准备0、测试3600、原pending，读取没有迁移/改账。定时未恢复。

## 自建引擎状态与顺序

已重读00/01/02/05/09/13/14。计划中的Engine R v0已经自建并在使用：Steam实际运行于本地构建的`cider-cx26.3-r1-wma1-mm1-x86_64`，源码、工具链和配方在仓库。02原“当前Highball二进制”是9月27历史状态，现已明确更新。媒体、窗口、同步、音频与K1–K3都是引擎层研发。

下一R线交付按原02的v0 Steam通过后安排：主题补丁清点、固定新Wine输入及逐主题迁移，维持CX oracle对照；不能把旧CX树重命名当成v1完成。P-3余项可继续使用已有带符号v0分析。Engine A按00/P3原2027年6–8月及前置probe/FEXUnixLib/A-lite/平台条件推进；R的自建不等待A。

本轮已实际开始v1首步：从Wine只读官方mirror核对wine-11.18 tag及commit `7b3fff76fa5178f6ce0141b2c776afa2a822f101`，按实际commit重取process.c，与tag取得的字节一致；其SHA256、21个现有配方补丁的主题/摘要及迁移状态写入`engine/porting/r-v1-inputs.json`。GitHub的tag签名验证为false（原始reason保留），没有冒称已独立验证签名。

首个preflight补丁移植到该固定源码，helper函数体与CX旧补丁相同，仅增加该函数和进程建立前的同语义检查。父代理逐项核对：空环境沿旧行为、名称匹配/分隔/大小写函数体不改、原PE失败及非PE分支不改、拒绝后沿现有done清理。临时副本zero fuzz/offset应用后字节完全匹配，没有编译/运行新Wine、生成可用引擎配方或更改EnginePolicy绑定。新目录是源码准备，不能升级游戏裁定；迁移接续须完整核对CX msync、loader/WoW64及图形依赖，kernel新base要重新生成协议而非照搬1810。

执行主序仍为HY1启动器余项→HY2原生契约/依赖组→HY3证据与许可闭环→HY4原神→HY5崩铁/绝区零，柚子社全系保持同等最终目标。Epic弃用，官方国服、现有下载和反作弊红线保持。源码中的kernel候选现已恢复，但Windows原生对照尚缺，仍非默认；恢复不自动切换已登录瓶子或提升游戏评级。

## 回执

结构化来源、子树对象、App组件摘要和UI/进程边界见`evidence/full-source-restore-20261018.json`。本机完整构建日志位于`out/restore-20261018-build.log`，恢复操作记录在`out/restore-20261018/`。
