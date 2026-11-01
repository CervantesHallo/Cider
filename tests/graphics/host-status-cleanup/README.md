# NT 清理失败与并发取消

接 0027 普通候选的逐调用 `owns_cell` 修复。夹具只操作本进程创建的窗口、预留 ticket、完成页和事件。没有导入真实 CAContext，context_id 是元数据。

客户端对真实 writer section 句柄设置 `HANDLE_FLAG_PROTECT_FROM_CLOSE`。普通 CANCEL 发布资源 SAFE，真实 NtClose 返回 `STATUS_HANDLE_NOT_CLOSABLE`，普通恢复路径保留部分 NT owner。另一线程进入普通取消函数、取得实际 status_controls 后，在锁外等待有界的自有事件。PE 恢复计时器执行普通重试；同锁观察器记录本次没有取得 cell，也没有尝试关闭，却完整保留原 cell、登记及 producer owner。释放事件并真实等待线程结束后，客户端只清除一次保护，由普通计时器完成真实 NtClose 和归零，再释放 producer 与观察引用。

观察引用仅维持对象寿命，不增加控制／资源 guard，不替代普通清理。`local_closed=0` 在 `attempted=0` 时只是初始化值，不能解释为 NT 成功。BIND／ARM 只对明确的 IO_PENDING 有界等待；未启动工作线程的 ARM 可 DISARM，避免辅助状态阻塞清理。清保护成功后不再访问原句柄，防止计时器关闭后复用。

应用 diagnostic.patch 到普通 0027 源，加入本目录 include 和 probe.h，使用完整 spec 及显式 `-Fwinemac.drv`。两张 Unix 表各 8 槽、watch v2、诊断帧 336B/v1；Native coordinator 使用 `-DNDEBUG`，所有所有权操作保留在断言之外。客户端严格 MinGW GCC 编译，两种 PE 均经 Cider GUI/XPC 隔离瓶无参数运行。

实际 PE64／PE32 各 42 项、0 失败、CLI0；五份完整外层清理、原瓶字节／缺失恢复及保护摘要另行验证。见 research/116。首次 printf 类型警告、一次构建脚本漏拷贝对象均作为失败记录保留，未放宽警告。独立审查发现的 ARM 瞬态和未使用 ARM 清理问题已修复。

这是指定 NT 故障／取消交错的验收，不是实际 INSTALL 超时、安装取消重入、像素、跨进程资源、Windows 原生或完整游戏兼容证明。诊断导出和调度点不属于默认引擎。
