# Native 异步更新的接纳、校验与销毁

接 0027 普通实现，复用 [host-status-live](../host-status-live/) 的相同 include。先 include 本目录再 live，五文件 diagnostic.patch／完整 spec／`-Fwinemac.drv`；两张 Unix 表各 9 槽、watch v2，live 帧 128B／新帧 288B/v1，Native coordinator NDEBUG。

真实 context／known ticket／实际模型建立后，在 Cocoa 队列放入仅用有界 Unix 条件变量的屏障。绑定的 Wine 调用线程真实调用 macdrv_remote_host_refresh 两次，普通函数自己接纳 job/ref、计算几何、完成预检并异步排队。屏障仍在时冻结实际 jobs=2、两次入队成功及最新 serial，再解除屏障；不是制造 job 计数或直接调用 Cocoa 更新。

原始观察记录实际接纳计数1/2、Wine预检均真、Cocoa旧job校验假/新job真、仅新job进入普通方法且users1、两个普通销毁分别看到jobs1/0。context/RO保持，实际jobs0／相应销毁观察及屏障完成后才释放独立观察引用，之后真实CANCEL／SAFE及原始清理。

观察引用仅维持host寿命，不参与模型/资源/完成guard。验证状态以0未见、1实际false、2实际true区分。Cocoa只有Unix同步和标量观察，没有NT／PE工作；失败ABORT释放屏障，已经接纳的更新不重放、不假定结束。同步借用没有跨异步任务保留调用方frame。

实际双PE各32/0/CLI0，独立源审无P1/P2，实际产物／原始session／EXE、5份完整清理、原字节恢复和四保护摘要绑定，见research/124。记录的是方法入口/参数及逻辑job归还，不是几何无异常应用、回调尾部退出、线程join、全队列空闲或全部内存释放证明；外层真实进程清理另证。无全局DPI/跨进程/像素/GPU/性能/游戏/默认采用结论。
