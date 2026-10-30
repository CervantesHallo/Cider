# 实际 producer 完成页

普通0024创建实际NT section并发布终端状态；本夹具只建立观察者，不发布该页。创建真实Win32 root/child、self-process租约和exported CAContext，暂持一个普通INSTALL准入pin。相反位宽子进程在真实RELEASE前打开只读section，关闭自己的handle后继续持有view。context未导入宿主，不代表host_install/Cocoa安装重入或像素。

## 构建

固定Wine11.18及0023前置组上应用0024，再应用diagnostic.patch。普通Makefile新增status_cell.c，由#pragma makedep unix指定Native编译。把本目录加入native/PE include路径；重建受影响coordinator、main、watch/dllmain及匹配链接单元。私有Native/Wow64表均8入口，槽6为原48B probe，槽7为本96B/version1 frame；它与USER96 packet是两个类型。watch recovery头为version2，PE init必须匹配重编。

私有导出使用winemac-fixture.spec并显式-Fwinemac.drv。client.c包含reader.inc，链接同组status_cell.c，用两架构MinGW GCC的-std=gnu11 -O2 -Wall -Wextra -Werror编译，链接user32/ntdll。通过独立Cider GUI/XPC实验瓶运行`parent.exe C:\CiderTests\opposite.exe`；固定修后组可追加`--close-failure`。不用用户Steam瓶，不直接派生Wine。

## 判据

正常父29/0、读方22/0、CLI0。初始及持pin时应READ_SNAPSHOT0/ACTIVE，实际RO保护2；真实Win32窗口销毁后归还pin，读方以有界只读轮询等待SAFE_ONLY5/values_valid0，最后unmap并实际退出。SAFE不提供可消费的动态计数或数值序号前缀。

before.patch只从共享资源就绪判据省略request_inflight条件。同EXE应只有子CHECK16失败，读方22/1、primary实际SAFE_ONLY5，父29/0且记录native watch0/finalized1/status owner0，CLI1。其他失败不算有效负例。

关闭故障组父33/0、读方22/0。通过真实NtQueryObject/NtSetInformationObject保护writer section，实际unmap后NtClose返回c0000235，Native保留部分owner/watch。清保护期间cleanup_inflight阻止借用H被detach；实际清除成功先更新所有权标志，再判断是否超时，避免迟到副作用被cleanup重放。只对明确无动作的IO_PENDING重试。清除后只用纯op7观察恢复归零，再允许op4清理；op7不驱动普通清理。

PCF-01/02旧问题及原运行保留。所有成功组还需完整实际GUI退出/两空/no-pending回执、普通配置/DLL原字节或缺失恢复及模块/EXE/session绑定。单次调用超时不能取消同步NT，外层控制器仍须真实终止和清理。未覆盖失败START/no-watch、早期临时句柄故障、FIFO/epoch竞态或完整renderer；见research106。
