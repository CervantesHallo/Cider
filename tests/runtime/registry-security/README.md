# RegCreateKeyExA/W 安全属性定向检查

`probe.c` 检查 0018：新键的 DACL 和句柄继承、已存在键不被新描述符覆盖、递归中间键的默认安全属性、重复尾部分隔符，以及默认/显式 32/显式 64 位注册表视图。它只操作确认新建的 HKCU nonce 树，不需要管理员权限。

用两种 MinGW GCC 分别编译：

```sh
gcc -std=gnu11 -O2 -Wall -Wextra -Werror probe.c -ladvapi32 -o registry-security.exe
```

通过私有 Cider GUI/XPC 实验瓶运行，唯一参数为新生成的 32 位小写十六进制 nonce。完整成功必须包含 195 项检查、18 个案例、3 种视图、零失败和 CLI 0；不能只检查退出码。失败时输出具体检查编号和标签。查询安全描述符失败不会被算作“默认 ACL 不同”。

同一 PE64 二进制在未修尾部分隔符的候选上产生 12 个预期失败；最终修复后两种 PE 各 195 项通过。它是 Mac/Wine 上的局部 API 结果，不代替真实 Windows 对照、完整权限行为或游戏会话。运行前后仍需确认实验所有权、session/模块身份和进程清理。原记录见 research96。
