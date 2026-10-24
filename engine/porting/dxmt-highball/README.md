# Highball DXMT 依赖候选

固定源：`gauthierpiarrette/dxmt` commit `7421db2076b7e4ef36be167294023feefbe7ba7e`。本目录保存 Cider 依赖修复候选，未进入默认组件或用户引擎。

`0001` 修复 `ArgumentEncodingContext` 的设备成员在清屏子组件构造后才初始化的问题。成员声明和 initializer 同时移到依赖者之前；保留全部真实管线创建。内部 C++ 布局改变，需重编译完整 PE 依赖闭包，不可单独拼接旧对象。

实际失败、复审、构建条件和运行边界见 [研究 93](../../../docs/research/93-r-v1-gpu-shared-and-window-foundation-20261025.md)。固定 Metal shader/native 输入未改变，不据此声称完整 DXMT 从源码构建或 GPU/游戏验收通过。
