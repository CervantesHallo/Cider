# R v1：多个预留的完成状态互不替代

日期：2026-11-01，UTC+8。接续[113](113-r-v1-lifecycle-order-20261101.md)，留在per-host安装／取消／清理阶段。

## 当前契约的实际验证

同一producer可以有多个待处理宿主。单个宿主完成不能替代另一个宿主或整个producer的许可。[客户端](../../tests/graphics/host-status-reservations/)实际申请两个不同ticket，并分别打开其真实只读NT section；context_id=1仅为元数据，未发送实际导入请求。

取消A后，A实际读取SAFE／values_valid0；B仍是完整身份匹配、values_valid1、safe0的ACTIVE页。再次请求安装已取消的A，服务器实际返回INVALID_HANDLE，B仍保持独立。释放producer后，B及producer保留页达到真实SAFE；A的映射仍可读取终端许可。最后正常关闭所有读方及窗口。

| 正常元数据用例 | PE64 | PE32 |
|---|---|---|
| 同lease双ticket隔离与剩余预留回收 | 29检查、0失败、CLI0 | 29检查、0失败、CLI0 |

两个ticket真实分配且不同，lease／root／generation等身份保持一致。实际USER调用顺序为查询、申请、预留A、预留B、取消A、拒绝A重装、释放producer。RO观察在这些调用之间独立进行，不发送额外恢复请求驱动清理。

## 验证与边界

限定独立源码审查接纳，MinGW两位宽严格编译完成。普通Native／PE组件复用已确认字节，未重编或重跑旧正常用例。父代理另行读取raw/session/EXE、16PE＋2native／F27、5份完整进程退出回执、瓶子原字节或缺失恢复、四个保护摘要，见[证据](evidence/r-v1-reservation-isolation-20261101.json)。

这只证明同进程元数据预留及状态页的隔离，不是两个真实图层或共享context最终释放的证明；也不是NT清理失败／取消并发、未知INSTALL回复、多进程、像素、性能或游戏验收。平台拦截的关键故障验收仍待，不把本次29/0作为替代，不降低正式采用门槛。

继续按原顺序留在当前阶段。跨进程草稿仍搁置、未构建运行，普通用户引擎与游戏门禁不变；Windows、Actions、定时、Goal零操作。
