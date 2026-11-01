# 安装在途与取消

接 0027 普通候选；复用 [host-status-live](../host-status-live/) 的 fixture.h、native.inc、pe.inc、cocoa.inc 和 probe.h，相同文件不重复保存。构建时先包含本目录，再包含该目录。应用本目录六文件 diagnostic.patch 到普通 0027 源，完整 spec／两张 Unix 表各 9 槽匹配，watch v2；Native coordinator 使用 `-DNDEBUG`。诊断帧 160B/v1，原 live 帧仍 128B/v1。

普通 USER setup／PREPARE 获得实际 CAContext、已知 ticket 和 RO 页。ARM 在创建线程之前保守锁存 import-attempt，工作线程独立进入普通 packet_producer_begin／install_prepared_host／end。普通安装取得实际 control、request 和 model_job 后，在锁外等待自有事件；主线程通过真实 USER CANCEL 取消该 ticket。

取消返回 IO_PENDING。原 context 和读方保持，真实图层／登记／选择虽均为零，Host RO 仍是有效非 SAFE snapshot；同锁观察实际 control/model_job/request、待退役和 owner 均保留。释放事件并实际 join 后，普通安装返回 INVALID_HANDLE，control／model_job 归零；实际模型清理及 Host／producer SAFE 之后才销毁 context、读方和观察引用。没有单独记录 commit 回调是否执行，不把操作错误码当作其执行证明。

本夹具的 INSTALL 使用私有 wrapper 进入普通函数，CANCEL 使用实际 USER 传输。它不制造真实 INSTALL 超时，也不代表未知消息结果。观察引用只维持对象寿命，不加入完成 guard；辅助入口只支持给定的单控制线程／单工作线程时序。

独立源码审查通过；最终双 PE 各 36 项、零失败、CLI0，原始帧／session／EXE／组件、五次完整清理和原字节恢复另行核对。见 research/117。初稿调用普通函数参数数目错误导致编译失败，已修且保留旧记录；核对脚本初稿误要求所有身份均为高位，已按实际非零身份与高位请求序号分别纠正，未改夹具或重跑有效结果。

不证明全部交错、跨进程、像素、GPU、性能、游戏或默认采用。
