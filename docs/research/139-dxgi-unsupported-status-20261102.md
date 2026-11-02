# DXMT：未实现通知的明确失败与资源边界

接续94遗留的采用前warning，在138的整树Wine编译期间完成依赖修正。
固定Highball源的`RegisterHardwareContentProtectionTeardownStatusEvent`
原先只有`assert(0)`，NDEBUG发布构建会无返回值地离开HRESULT函数。
两架构旧编译日志均存在该警告，不能把不确定返回值当真实登记成功。

[微软接口契约](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiadapter3-registerhardwarecontentprotectionteardownstatusevent)
规定成功为S_OK、失败为HRESULT错误，登记成功才产生可注销的cookie。
0002选择E_NOTIMPL描述本后端尚未实现的能力；文档没有指定必须用这一错误，
它也不是本轮取得的原生Windows参考结果。未新增硬件支持声明、成功空桩、
事件信号或句柄接管。配套void注销入口记录未登记，不做资源操作或触发断言。

在保留的两份合格源码副本上，只应用该补丁；原件未修改。
重放记录的真实编译参数，重定向对象／依赖文件输出，增加
`-Werror=return-type`。两种NDEBUG翻译单元均编译成功、无诊断。
随后使用原合格CRT和依赖重链接各自dxgi.dll，仅替换adapter对象；
两DLL的五项导出名称和ordinal与旧版本相同。没有重编译Metal库、
native winemetal或Wine同步核心，没有改变类成员／接口签名。

实际GUI／XPC在既有无账户、无游戏实验瓶中完成双PE各11项：
factory／adapter／Adapter3获取、事件创建、明确未实现错误、无cookie发放、
事件仍未置位、调用方仍持有句柄、注销后仍有效、空参数调用不解引用，
以及最终由调用方成功关闭事件。日志分别包含两次新登记错误和一次注销说明，
同时核对候选引擎、实际模块摘要与session身份，无原生异常标记。
这个检查是本后端的失败／资源边界，不能代替真实硬件通知或Windows对照。

首个测试EXE链接缺少IID_IDXGIAdapter3；按SDK方式加入initguid.h后两架构
编译通过，失败回执保留。父代理导出解析器初版未支持当前objdump的
`+base[ordinal]`／hint列，仅修解析并复读同一批二进制，没有重新构建或更改旧结果。
原链接回执的reusedInputs中保留了旧adapter对象作为核对上下文，实际args已替换；
父绑定另外列出真正的显式文件输入，不能把那份旧对象称作实际复用。

最后确认测试GUI和Wine进程回收、两次空观察、无pending；
原配置和两份前缀dxgi.dll逐字恢复，测试文件副本移除，四项受保护摘要一致。
限定父对抗性审查未发现具体P1／P2；异常日志、handles、exports与实际输入
分别核对，不用单个退出码代替。

候选供后续完整引擎整合，不替换137的冻结预览、用户组件或默认引擎。
138的整树Wine编译仍进行中，完整M1／性能／硬件通知／游戏裁定没有升级。
没有Windows、Actions或定时操作。

结构化证据：`evidence/dxgi-unsupported-status-20261102.json`；
完整源码、重链接、实际两份raw与恢复回执在
`out/engine-dxmt-bridge-20261025/dxgi-status-build-v1`。
