# 无 PE watch 时的启动失败恢复

固定Wine11.18及0025上的私有夹具。分别用两种PE运行`fixture.exe --mode 1`和`--mode 2`，每种Native故障模块匹配同一客户端和PE驱动。它不启动游戏、不创建renderer，不把错误返回替换成成功或伪造NT失败。

## 两种真实故障

- mode1：实际NtCreateSection仅申请READ|QUERY，真实保护该handle，原PAGE_READWRITE映射返回c0000022，发生在WatchSTART之前。原NtClose返回c0000235，部分section owner保留。
- mode2：实际NtOpenThread结果先登记到setup，再设置真实ProtectFromClose。原首次NtClose及收尾失败，setup FIFO持有同一thread handle，不创建producer/section/PE watch。

cell-diagnostic.patch和thread-diagnostic.patch分别应用于0025普通源码。包括本目录native.inc/fixture.h，重建匹配Native及PE；导出用winemac-fixture.spec并显式-Fwinemac.drv。两张Unix表均8项，槽6为既有48B probe，槽7为本96B/version1帧；USER96不变，watch恢复协议version2。client.c以两架构MinGW GCC的-std=gnu11 -O2 -Wall -Wextra -Werror编译，提供普通status_cell.h及ntdll/user32链接。

通过私有Cider GUI/XPC运行，不直接派生Wine，不用用户Steam瓶。每次新进程，Root PID/TID/birth实际查询，Native owner ID从保留列表读取。

## 完整成功条件

每模式/位宽25检查、零失败、CLI0。RUN须返回零lease和精确真实错误；pending时PE watches0、定时器owner非零、对应Native owner保留。Win32根窗口销毁后仍保留；目标handle和真实epoch的登记位必须消失。Wine内部窗口可留在全局root表，不能把全局root数量当作目标窗口存亡。

mode1 CLEAR持有真实控制pin，等待实际timer retry在该pin已存在时执行finalizer；同一尝试在决定前、决定后均验证，才设置RETRY_UNDER_CLEAR_PIN。这证明0025在无法完成关闭时仍交回可运行FIFO owner。observer只记录，不能enqueue/close/retry。随后真实清保护；mode2只复制已保护H，不在解锁后使用setup指针，Native清除标志防止重放。

之后只用纯SNAPSHOT/probe及Sleep观察所有producer/watch/setup/cleanup/model和PE watch/timer owner归零。SNAPSHOT不调用清理；没有额外USER消息推动。QPC预算前后检查，实际清除副作用先记录，再判断是否迟到。每次还需独立绑定EXE/模块/session、实际GUI退出/两空/no-pending和原配置/DLL恢复。

mode1原始日志逐个记录实际map/close的同一handle、root_pid、lease和NTSTATUS；必须同时出现c0000022、c0000235和最终0。mode2使用setup真实last_close及同setup日志。定时器owner0是取消确认，不证明最后一个callback已完全退出；进程清理是另一份证据。完整结果及旧失败/复审见research107。
