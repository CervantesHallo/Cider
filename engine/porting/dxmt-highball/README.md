# Highball DXMT 依赖候选

固定源：`gauthierpiarrette/dxmt` commit `7421db2076b7e4ef36be167294023feefbe7ba7e`。本目录保存 Cider 依赖修复候选，未进入默认组件或用户引擎。

`0001` 修复 `ArgumentEncodingContext` 的设备成员在清屏子组件构造后才初始化的问题。成员声明和 initializer 同时移到依赖者之前；保留全部真实管线创建。内部 C++ 布局改变，需重编译完整 PE 依赖闭包，不可单独拼接旧对象。

`0002` 修复原assert-only通知登记接口在NDEBUG下无返回值的问题：明确返回E_NOTIMPL，不发放cookie、不操作调用方事件；void注销入口记录没有登记，避免debug断言退出。两PE翻译单元在Werror=return-type下编译、分别只替换该对象重链接、五项导出名称／ordinal一致；实际各11项错误／资源检查通过。它没有实现硬件通知或变更能力位，Windows原生返回值未对照。见[研究139](../../../docs/research/139-dxgi-unsupported-status-20261102.md)，旧预览／默认组件不据此自动替换。

实际失败、复审、构建条件和运行边界见 [研究 93](../../../docs/research/93-r-v1-gpu-shared-and-window-foundation-20261025.md)。固定 Metal shader/native 输入未改变，不据此声称完整 DXMT 从源码构建或 GPU/游戏验收通过。
