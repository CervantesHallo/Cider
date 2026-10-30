# R v1：真实context的已知ticket安装与取消

日期：2026-10-31，UTC+8。接续[111](111-r-v1-host-status-metadata-20261031.md)。本轮把正常per-host状态流程接到实际Cocoa模型；普通0027候选未改。

## 实际生命周期

[夹具](../../tests/graphics/host-status-live/)创建真实Win32 Root及有边框child，通过普通v2 USER发送器查询/申请lease，创建并持有实际导出的Metal/CAContext，取得真实context ID。PREPARE返回已知ticket后先打开其真实RO页，成功读取完整身份的SNAPSHOT，才发送INSTALL。

安装返回后，纯Cocoa观察分别记录真实父图层中的对象、逐ticket登记和当前选择，各1。取消只请求退役；保留原context和RO映射，等实际HostSAFE且三种模型观察各0之后，才允许释放。随后实际producer完成页也达到SAFE，真实context、本地NT读方、窗口及引用全部按顺序关闭。

| 正常路径 | PE64 | PE32 |
|---|---|---|
| 实际context＋已知ticket安装/取消 | 25项、0失败、CLI0 | 25项、0失败、CLI0 |

原始记录包括context ID、epoch/lease/ticket及高位请求序号；安装时1/1/1，取消后0/0/0且context仍持有，最终释放标记及本地owner0。许可来自实际RO读取，不把控制回复或字典缺失当作完成，也不把终端无效动态值解释为数量证明。清理先锁存已经观察的不可逆许可，再关闭读方，避免关闭失败后用不可再读的view反复证明先前事实。

## 审查、构建与绑定

夹具限定独立源码复审接纳；父代理另核对集成及有效输入。最终fixture-v2、diagnostic-source-v1，128B/version1，普通watch2、双Unix表8项。Native及两PE驱动首次构建成功；客户端四处同一行if/goto后赋值产生misleading-indentation错误，只改换行后独立重编两PE。原失败记录保留，未重编或伪称其整体通过。

9组实际编译依赖、114个项目输入、2份EXE/raw/session、16PE＋2native及F27核心、5份完整进程清理回执、原配置/DLL字节或缺失恢复、四个保护摘要另行绑定。五文件诊断补丁零fuzz、零offset逐字重建；见[证据](evidence/r-v1-live-host-status-20261031.json)。

## 尚未跨越的门槛

这是同进程、实际导出context和模型的正常生命周期，不是画面像素、GPU/compositor栅栏、跨进程、故障并发、完整泄漏或游戏验证。夹具是首个资源消费者，尚未把所有生产renderer/reparent事件接入该协议；不切用户引擎或默认foreign-child路由。

111记录的NT清理失败＋取消并发验收仍因平台工具自动安全审查拦截而未完成，不能由本轮25/0替代。已完成源码与正常运行保持，关键故障门槛继续阻止正式采用；不以改名、换代理或删掉门槛冒充解决。后续按原计划补该缺口，再完成实际多进程资源、事件reparent及两三进程像素，保留HY/U8-15、Windows忙待重启和游戏门禁。Windows、Actions、定时、Goal零操作。
