# Setup FIFO 公平性

固定Wine11.18及0025上的私有夹具。实际NtOpenThread结果受ProtectFromClose保护，两次普通acquisition各留下一个setup owner。通过Cider GUI/XPC私有瓶运行同一客户端无参数，不直接派生Wine或使用用户Steam副本。

应用diagnostic.patch并加入本目录include；导出使用winemac-fixture.spec并显式-Fwinemac.drv。两张Unix表均8项，槽6原probe、槽7本120B/version2帧；watch2、USER96不变。client.c使用普通status_cell.h及ntdll/user32，两架构MinGW GCC以-std=gnu11 -O2 -Wall -Wextra -Werror编译。

测试限定一个合法的单消费者时序：私有mutex串行化实际retry，旧项被普通pop后留busy，等待独立客户端销毁Root并清新项；不持remote_lock等待。旧项实际NtClose失败，普通代码requeue后，在锁内观察两项位置，要求新项仍queued且可选。该锁不是产品策略或全并发覆盖。

正常20/0、CLI0：CHECK13须有实际order，新/旧位置0/1；CHECK14还要求新项实际retry成功/移除，旧项仍保护且c0000235。before.patch只把失败retry插到队首，初次入队不变；相同EXE应20/1、仅CHECK14，实际旧/新0/1。错误顺序直接拒绝，超时不等同饥饿。随后清旧项，两组都须实际归零和完整清理。

RUN失败或迟到先记录已捕获owner再拒绝，cleanup只要求实际创建者的clear位。真实clear先记副作用、后判断时限，Native幂等标志防重放。完整模块/EXE/session和真实进程清理须独立绑定。旧审查、观察器更正及结果见research108。
