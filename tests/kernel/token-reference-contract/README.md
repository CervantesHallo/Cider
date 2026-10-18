# 只读自身令牌句柄契约观察

`handle-access.c`只观察当前进程的公开Win32接口：零访问打开、TOKEN_QUERY打开、复制零权句柄、查询/关闭。它不输出SID/账户/TokenId，不修改令牌、DACL或权限，不模拟身份，不访问外部进程，也不加载驱动。

用已有MinGW编译：

```sh
x86_64-w64-mingw32-gcc -std=c11 -Wall -Wextra -Werror -O2 -static-libgcc -Wl,--no-insert-timestamp handle-access.c -ladvapi32 -o cider-own-token-access-x86_64.exe
i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -Werror -O2 -static-libgcc -Wl,--no-insert-timestamp handle-access.c -ladvapi32 -o cider-own-token-access-i686.exe
```

输出记录观察值；返回0仅表示采集及关闭完成，不表示某条K2实现已通过。当前提交结果为隔离Wine中的32/64观察，没有运行新的原生Windows参考。公开API预期和错误路线判断见research70，不能修改访问检查来迎合一个预设成功结果。

`check-release.py`另抽取0017的真实释放函数体，以宿主对象依赖检查恰好一次委托和错误传播。真实PE反汇编/构建结果保留在results；它不执行token后端或实现取得接口。


## K2 对象引用候选及实际 Wine 运行

0018/0019沿用Wine kernel对象映射：manager归属调用地址空间，已保留process句柄和映射须指向同一真实server对象。持有该process实际primary token，返回canonical引用代理；不创建用户token句柄，不改公开API访问检查，不采用退休0010。0017归还引用，0020补公共DDK声明。协议1810的server/ntdll必须配套，非默认候选。

两个`check-*-reference.py`抽取真实C函数，使用有界依赖和ASan/UBSan。server模型覆盖归属/目标绑定/alias/primary槽更换/OOM/无token等70项；client模型覆盖引用接纳、零到一/多引用、溢出、回滚和关闭/异常39项。`check-sensitivity.py`要求六个实际编译的故意破坏变体断言失败，编译失败不算检出；这些是模型证据。

`driver.c`和`client.c`只在全新、无账号的私有Wine前缀运行。客户端拒绝native Windows，IOCTL没有输入/PID/句柄/地址；驱动只引用当前IRP请求方process的实际token。公开Win32查询的TokenId与kernel引用相同，且与实际winedevice host不同；TokenId只用于进程内比较，不输出账户/SID。64轮alias/零到一引用及两真实工作线程合计64轮并发查询，最后由自有子进程留下token引用、退出后再查询和释放。服务停止读回、删除、所有prefix进程和XPC job的外层清理分别核对。不是Windows原生kernel或真实游戏结果。

`policy-child.c`把自己复制为受禁basename，仅用来检查新native配对子进程门禁；没有游戏代码，预期CreateProcess失败1260且没有child marker。绝不解除真实游戏门禁。

```sh
python3 tests/kernel/token-reference-contract/build-runtime.py --source /private/wine/src --build /private/wine/build --out /private/fixtures
python3 tests/kernel/token-reference-contract/check-server-reference.py --source /private/wine/src --report server.json
python3 tests/kernel/token-reference-contract/check-client-reference.py --source /private/wine/src/dlls/ntoskrnl.exe/ntoskrnl.c --report client.json
python3 tests/kernel/token-reference-contract/check-sensitivity.py --source /private/wine/src --report mutations.json
python3 tests/runtime/product/run_runtime.py --root /absolute/runtime-acceptance-owned-root --owned-token-reference
python3 tests/runtime/product/run_runtime.py --root /absolute/runtime-acceptance-owned-root --resume --owned-token-reference --32
```

准备根目录须按现有运行控制器的owner.json记录来源、App组件/fixture/匹配native模块摘要与保护数据。只复制引擎代码并创建新瓶子，不复制任何已登录prefix。结果、失败历史和原生验证边界见research71。
