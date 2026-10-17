# 只读自身令牌句柄契约观察

`handle-access.c`只观察当前进程的公开Win32接口：零访问打开、TOKEN_QUERY打开、复制零权句柄、查询/关闭。它不输出SID/账户/TokenId，不修改令牌、DACL或权限，不模拟身份，不访问外部进程，也不加载驱动。

用已有MinGW编译：

```sh
x86_64-w64-mingw32-gcc -std=c11 -Wall -Wextra -Werror -O2 -static-libgcc -Wl,--no-insert-timestamp handle-access.c -ladvapi32 -o cider-own-token-access-x86_64.exe
i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -Werror -O2 -static-libgcc -Wl,--no-insert-timestamp handle-access.c -ladvapi32 -o cider-own-token-access-i686.exe
```

输出记录观察值；返回0仅表示采集及关闭完成，不表示某条K2实现已通过。当前提交结果为隔离Wine中的32/64观察，没有运行新的原生Windows参考。公开API预期和错误路线判断见research70，不能修改访问检查来迎合一个预设成功结果。

`check-release.py`另抽取0017的真实释放函数体，以宿主对象依赖检查恰好一次委托和错误传播。真实PE反汇编/构建结果保留在results；它不执行token后端或实现取得接口。
