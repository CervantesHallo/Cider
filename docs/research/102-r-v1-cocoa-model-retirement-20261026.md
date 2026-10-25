# R v1：Cocoa model 移除完成的实际所有权

日期：2026-10-26，UTC+8。接续[101](101-r-v1-real-close-inflight-20261026.md)，实现连接producer资源前所需的model退役边界；foreign renderer仍未启用。

## 普通候选0022

宿主进入RETIRED的同一锁事务，登记全局和对应producer的待移除计数。原retired-list owner转交给Cocoa完成回调；actual model移除成功后归还，失败则把同一个owner交给显式FIFO，计数不归零。USER入口和现有PE恢复timer每次重试一个；Cocoa回调不执行NT句柄/PE控制调用，不持registry锁同步等待Cocoa。

producer finalizer还必须等待pending_retirements0，避免native列表移除被当作Cocoa完成。失败日志使用解锁前保存的ticket，避免queued owner被重试释放后的访问。版本化Cocoa方法只确认指定generation已从model移除或缺失；旧void接口保留包装。异常报告失败，owner继续保留。计数修改都在assert外。

三文件相对0021精确重建。普通native使用SDK15.4/min14编译/链接；10条SDK警告映射到原始相同行和消息，仅源位置因插入移动。首轮Cocoa ERR用错C字符串而拒绝编译，NSString修后另存v3；复用逐字相同coordinator对象，没有重建无变化单元。限定三文件独立源审无新增具体P1/P2。

这里的完成指**Cocoa model中的layer移除**，不证明compositor提交、GPU停用或完整CAContext-ID ABA。

## 真实资源夹具与对照

[公开夹具](../../tests/graphics/remote-retirement/README.md)使用实际Win32根/带边框child、真实CAContext及Metal layer、真实self-process租约和PE等待线程。固定80B frame不含native指针；普通六个Unix入口保持顺序，私有追加48B probe和80B fixture，native/Wow64表均8项。

| 组 | PE64 | PE32 | 判据 |
|---|---|---|---|
| 修后 | 20/0，CLI0 | 20/0，CLI0 | model仍在时保持pending和watch |
| 只去掉model计数保护 | 20/1，CLI1 | 20/1，CLI1 | 仅CHECK12，model仍在却已finalized |
| 首次false完成、随后重试 | 20/0，CLI0 | 20/0，CLI0 | owner保留，实际重试后归零 |

每行是检查/失败。两种位宽分别执行，**不是两/三进程renderer呈现**。每次实际lease8589934607、ticket8589934608，高32位完整；context ID取实际CAContext返回，未造ID。held slot持有实际retired-list owner，Native pending计数已在普通代码中登记；测试暂缓提交Cocoa工作，观察真实完成到来时model仍在。

修后primary为pending1/watched1/finalized0/model_found1/retirement_total1；负例保留相同资源、等待和客户端，只省略finalizer条件，得到watched0/finalized1，精准失败CHECK12。之后同样执行实际Cocoa移除与清理，没有任意失败冒充反例。

受控false完成在实际移除之前触发，未抛真实NSException。原owner进入FIFO，原子快照证明pending1/watched1/finalized0/total1，已有timer使实际generation匹配移除成功，最后model、native及PE owner归零。故障只在私有组中，不进普通代码。最初在原生Cocoa使用Windows LONG/Interlocked的准备代码编译拒绝；修为仅主线程访问的native bool，旧失败保留。

夹具由一个有明确文件范围的代理实现header/client，父代理完成native/Cocoa/PE wrapper、编译、实际运行与整合。最终限定集成源审无新增具体P1/P2；日志故障增量由父代理另行复核。PE64直接FARPROC转换在Werror下失败，保留原件，用uintptr_t桥接修正；未降低编译选项。一次相对/绝对参数路径误配在编译前拒绝，NativeMain缺私有prototype的编译失败也保留；未追认这些准备版本通过。

## 受影响原窗口路径

匹配的修后私有组运行原D3D11 window-lifecycle两种EXE，各1415项、4窗口、56交换链、168次Present、零失败；原trace各56次snapshot/detach/destroy。它核对受影响局部窗口路径，不能转为屏幕、CEF或游戏验收。

父代理另核对原始log/session、实际EXE、16个PE/2个native模块、F27核心、所有清理回执、普通实验配置/DLL恢复和四个生产保护摘要。原始材料及结构化指纹见[证据](evidence/r-v1-cocoa-model-retirement-20261026.json)。继续及时归档差异和清理完成的故障/负例整树，保留当前工作组及必要参考。

## 下一步：终端完成状态与producer资源

立即RELEASE成功只表示操作已接纳，不能让资源调用者直接销毁CAContext。下一阶段要在根HWND消失后仍可取得终端状态，处理未知安装结果/取消及真实资源所有权，再连接事件/reparent和两/三进程呈现。

拟研究只读共享完成页及序号栅栏，记录实际租约身份、接纳/静止序号和终端状态。收益是移除结果可跨HWND生命周期保留；成本是实际section/handle所有权、ACK和有界读取契约，以及耦合wire版本验证。需证明取消不被迟到安装逆转、完成页没有半写/旧代数据、Root/process退出和失败清理、多个context保持原有成功替换语义；回退保留当前稳定实验组。**这仍是提案，尚未采用或实现。**

当前新栅栏只覆盖model层；无producer的局部host失败可能需要后续USER流量，任意线程终止/多host并发/compositor/GPU边界仍待。未把候选切为用户默认，未提升CEF/游戏评级，原HY顺序、U90的8/15缺口、Windows忙/待重启和反作弊门禁保持。没有Windows、Actions、定时或Goal操作。
