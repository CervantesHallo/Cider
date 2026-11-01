# 同producer下多个预留的独立性

继续当前per-host清理契约，普通0027和Native／PE驱动不改。客户端经真实USER96-v2申请同一producer下两个不同ticket，使用相同context_id=1作元数据，分别打开真实只读NT页；从不导入该context。

取消A后，A的保留映射实际读取SAFE；B仍为完整身份一致的ACTIVE页。再次请求安装已取消A被服务器拒绝，B仍ACTIVE。随后释放producer，剩余B及producer页均实际达到SAFE，A的映射仍能读终端许可，再实际关闭三个读方及Root。

双PE各29项、0失败、CLI0。使用与111相同的status_cell.c／头文件与严格MinGW参数，链接user32/ntdll，经Cider GUI/XPC隔离瓶运行。原始page记录用于另行核对ticket、lease、完整身份和许可；不把控制回复当成SAFE。

这是同进程元数据／NT页隔离，不是两个实际图层、共享CAContext释放策略、NT失败并发、多进程、GPU/像素或游戏证明。见research114及证据；原关键门槛保留。
