# USER96 v2 与宿主只读状态页

Wine11.18、0026之后的0027候选，Native winemac和win32u必须成组；普通两Unix表各6槽，watch2，USER包仍96B。测试通过Cider GUI/XPC私有瓶运行，禁止直接派生Wine或使用登录Steam瓶子的副本。

客户端以MinGW GCC分别编译两位宽，链接user32/ntdll及同组status_cell.c，显式包含候选include与dlls/winemac.drv目录。使用-std=gnu11 -O2 -Wall -Wextra -Werror。status_cell.c的config.h仅Native路径包含；PE读方用同一函数实现。

每次新进程、无参数，预期28检查、0失败、CLI0。实际USER消息验证v2查询/申请、同协议幂等、跨协议明确拒绝且旧lease不变；真实NT页只读映射、同名但错误完整身份拒绝；禁止v2单步导入；按已知ticket取消、保留映射读取真实SAFE、旧ticket不能再次安装、最后读取producer完成页。

context_id=1只用于元数据，测试在取消前从不发送可用ticket的INSTALL。它不是实际CAContext或渲染证据。SAFE是精确身份的资源许可，values_valid=0；动态序号目前全部0，不是安装成功或完成前缀。每轮原始page事件记录实际身份、读数和许可，不能拿NOT_FOUND或超时替代SAFE。

见research111及其证据：最终source-v5、Native build-v5、PE客户端v2/build-v3，双PE各28/0。首次运行在CHECK18发现操作验证错误回复缺少身份；原失败记录保留，服务器只移动实际身份赋值并重新验收。新源还没有真实INSTALL、故障并发、跨进程、像素、性能或默认采用结论。
