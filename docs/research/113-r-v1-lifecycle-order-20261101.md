# R v1：按原阶段补取消先行与根窗口关闭

日期：2026-11-01，UTC+8。接续[112](112-r-v1-live-host-status-20261031.md)。用户重申不要打乱原开发顺序；本轮回到当前per-host安装／取消／清理契约。

## 顺序纠正

跨进程准备仅初写于私有host-status-cross-fixture-v1，没有构建或运行，已按文件摘要封存为parked。当前阶段门槛没有通过，就不以多进程、reparent或像素准备替代它。原生Windows、HY/U8-15及游戏门禁也不改变。

## 两个正常时序的实际结果

[客户端](../../tests/graphics/host-status-lifecycle-order/)复用112已确认的Native／PE驱动及普通0027字节，没有更改产品逻辑或追加Native模拟返回。

| 当前阶段用例 | PE64 | PE32 |
|---|---|---|
| 预留后先取消，再由消费者尝试安装 | 25检查、0失败、CLI0 | 同左 |
| 真实安装后根窗口关闭 | 22检查、0失败、CLI0 | 同左 |

取消先行：实际context和RO页已建立，取消后真实HostSAFE及Cocoa模型0/0/0。消费者再尝试LIVE_INSTALL，被既有RO检查在发送前拒绝，状态INVALID_STATE、install_sent仍0。context保持到后续producer完成与实际清理。没有把消费者拒绝冒称为服务器安装请求验收；111已独立覆盖元数据ticket的服务器拒绝。

根关闭：实际安装模型1/1/1后DestroyWindow(root)，窗口和其child实际关闭。客户端不再显式调用CANCEL／RELEASE，保留的实际RO页达到HostSAFE和producerSAFE，模型0/0/0且context仍持有，再进行真实资源释放。清理helper可能尝试已失效的端点；这个未知回复不作为完成依据。

## 审查和证据

两份客户端相对112的限定源码独立复审均接纳，MinGW GCC两位宽严格编译通过。Native和PE驱动未重编，按原组件摘要及其有效输入重新核对；每个客户端新增编译依赖也有绑定。

父代理另行解析4份raw/session/EXE、检查16PE＋2native/F27、10份完整进程清理回执、普通配置及DLL原字节或缺失状态、四个保护摘要。见[证据](evidence/r-v1-lifecycle-order-20261101.json)。这是正常时序验收，不是全部并发交错或完整泄漏统计。

平台工具此前拦截的真实NT清理失败＋取消并发验收仍待，未构建运行其部分代码，也未重复提交或换代理绕过。新增正常用例不替代它。当前仍停留在per-host阶段的验收门槛，后续顺序保持：补当前缺口，再接多进程消费者、事件reparent、两三进程像素。用户默认引擎和评级保持，Windows、Actions、定时、Goal零操作。
