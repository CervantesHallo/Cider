# 对象引用生命周期定向检查

`run_checks.py`从指定源码抽取实际对象头和9个C函数，依赖使用有界本机模型。覆盖稳定临时句柄、正常/失败引用、溢出、输出不发布、嵌套构造异常、句柄和递归锁清理、FREE对正引用拒绝、释放在副作用前后报错的终止状态，以及100轮真实pthread双线程关闭/查找交错。ASan/UBSan启用；没有执行Wine、PE、驱动或Windows。

指定旧源码时，相同合法关闭/查找交错在原实现触发ASan heap-use-after-free，作为负对照。宿主模型验证具体控制流和所有权条件，不替代Windows原生契约或Wine全局并发测试。固定实现增加一次同权限句柄复制与关闭，是内存安全的成本；没有扩大句柄权限或新增令牌访问路径。

```sh
python3 tests/kernel/object-lifetime/run_checks.py \
  --source /path/to/repaired/src \
  --old-source /path/to/original-quarantined/src
```

`verify_queue.py`在临时树中逆向原0009/0010得到基线，再依次应用0009/0012/0013；检查零fuzz/offset、六个文件与实际源码逐字节一致，以及旧primary函数体没有重新进入队列。

```sh
python3 tests/kernel/object-lifetime/verify_queue.py \
  --reference /path/to/original-quarantined/src \
  --expected /path/to/repaired/src
```

原生IRP和线程的持有链有源代码依据：server线程持有process，IRP持有thread，current_call持有IRP；客户端在下一次释放current_call的RPC前清空TEB借用。持有链范围以外不能保存借用指针。本检查没有证明所有异步/关闭场景，也不允许据此绕过门禁。

32/64位实际模块构建与对齐断言、独立复核和最终摘要见research62及其evidence。Windows原生结果仍待。
