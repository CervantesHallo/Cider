# 实际 Wine 通用对象运行检查

这是自有事件、借用文件、IRP缓冲区和工作项的定向检查。客户端检测Wine后才创建自己的服务；不在Windows加载驱动，不访问游戏、令牌、第三方驱动、任意地址或其他进程。所有服务/前缀/引擎模块放在标记的`runtime-acceptance-*`私有根中。

`driver.c`与`client.c`使用固定输出、无输入的IOCTL：事件引用后关闭原句柄256次；工作项真实回调64次，包括有序持有/关闭负对照和21次真实线程间关闭/引用竞争，回调自行释放work item。返回失败必须保留真实状态，不能以输出文件或IOCTL传输成功代替断言结果。客户端读回服务STOPPED后删除自己的服务，外层控制器另确认整个私有前缀和job实际清理。

编译复用现有MinGW GCC和`out/kernel-remediation/{src,build}`：

- driver：`-D__WINESRC__ -fno-stack-protector`，Wine的include，链接对应架构`libntoskrnl.a`，`-shared -nostdlib --subsystem,native`，入口为x64 `DriverEntry`或i386 `_DriverEntry@8`。
- client：`-municode -static-libgcc -ladvapi32`。两者均使用`-Wall -Wextra -Werror -O2 --no-insert-timestamp`。
- 私有根需有现有`owner.json`、产品App、引擎代码副本、host-control及自有probe；只替换复制引擎的两份ntoskrnl PE。原生wineserver/loader与门禁绑定保持，不把未替换server的运行记成token后端验证。

运行：

```sh
python3 tests/runtime/product/run_runtime.py --root /absolute/runtime-acceptance-owned-root --kernel-objects
python3 tests/runtime/product/run_runtime.py --root /absolute/runtime-acceptance-owned-root --resume --kernel-objects --32
```

两个故障检查抽取真实函数体，依赖以宿主模型替代：

```sh
python3 tests/kernel/wine-object-runtime/check-close-failure.py --source repaired/ntoskrnl.c --old-source before/ntoskrnl.c --report close-result.json
python3 tests/kernel/wine-object-runtime/check-ioctl-failure.py --source repaired/ntoskrnl.c --old-source before/ntoskrnl.c --report ioctl-result.json
```

前者覆盖3条CLOSE路径并要求旧未配对解引用负对照失败；后者覆盖12条IOCTL失败/成功缓冲路径并要求旧ASan double-free负对照重现。它们不是Wine故障注入或原生Windows对照。两种实际架构的最终Wine原始结果及证据边界见research69。
