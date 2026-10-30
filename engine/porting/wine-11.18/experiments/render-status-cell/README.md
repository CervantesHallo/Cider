# 未接入的 renderer 完成页组件

这是普通用户态 NT section 所有权与固定160B完成页的候选组件，**没有接入 Wine 驱动、producer资源或引擎配方**。native和PE共享源码；不创建新的消息线程或隐藏窗口。

- 根进程创建当前session内的命名section并映射RW，读方只申请READ|QUERY并映射RO。创建失败保留实际错误；不使用OBJ_OPENIF覆盖已有页。
- 名称包含根PID/实际创建时间/lease/ticket，读回另外核对完整root epoch、producer PID/birth/generation、context等身份。默认ACL和可预测名称不构成对同用户恶意进程的安全隔离。
- 40个原子32位word，不在跨位宽wire中放指针。单个外部串行writer以奇偶序号发布，reader只load，不向RO视图做RMW。一次读失败返回RETRY；耗尽不回绕、不重初始化。
- SAFE_ONLY为显式终端安全断言，`values_valid=0`，不可将动态字段当零计数快照。允许`cleanup_pending`，不等于全部NT资源关闭。
- create/open失败可能保留部分owner；close先unmap后close，失败保留剩余资源供重试。关闭读方handle不释放其view。调用者必须pin对象、串行控制操作，并在关闭前排空发布/回调用户。
- create/open/close只在Wine线程执行。publish/read没有NT调用；它们本身不提供并发free保护。Native Cocoa回调未来只能纯发布，不能在其中执行NT生命周期调用。

`tests/graphics/remote-status-cell`提供针对跨位宽RO映射、真实关闭失败及窗口销毁后读回的夹具。普通驱动何时发布、取消请求序号、迟到安装、inflight计数与真正producer CAContext资源释放仍须接入并验证，不能因组件通过就开启foreign renderer路线。
