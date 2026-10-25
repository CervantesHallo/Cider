# Legacy 注册表回调夹具

范围是 Engine R v1 的 native → PE32/PE64 五个真实注册表 API，不是 Windows 内核或游戏验收。只在一次性实验瓶运行；每次使用新的 32 位十六进制 nonce。仅删除本次确认新建的 `HKCU\Software\CiderLegacyProbe\<nonce>`。

## 准备与运行

1. 在固定 Wine 11.18 源码及 0011、0015–0017 候选的独立副本应用 `diagnostic.patch`，将本目录加入 PE/native include 路径。它追加一个私有 Unix-call 和 PE 导出，不能放入普通发布引擎。
2. 成组重建 native `winemac.so` 及两种 PE 的 `winemac.drv`。PE 导出使用 `winemac-fixture.spec`，并显式传给 winebuild **`-Fwinemac.drv`**（winegcc 链接参数 `-Wb,-F,winemac.drv`）；否则测试 spec 文件名会导致 Unix 库查找错误。其他编译/SDK/签名条件与原引擎一致。
3. 用各架构 MinGW GCC 编译客户端：`gcc -O2 -Wall -Wextra -Werror client.c -ladvapi32 -o registry-client.exe`。将匹配的模块组装入私有实验引擎，通过 Cider GUI/XPC 运行客户端，参数为 `<nonce>`、`<nonce> --existing` 或 `<nonce> --reject`。不要直接派生 Wine，也不要复用用户已登录的瓶子。

普通模式要求 33 项检查、131073 字节真实值、3 个安全属性案例、两次实际清理失败，pending owner 依次为 1/1/0。五项 native 提交计数和 PE 入口计数都必须是 `[3,1,2,5,7]`。只有全部结果、清理及 CLI 0 同时满足才通过。测试需配合 0018 的真实 `RegCreateKeyExA/W` 安全属性修复；没有该补丁时的 4 个失败已保留。

`--existing` 要求指定的已存在分支和 CREATE/CLOSE 计数，原 Sentinel 保持、Large 不存在；`--reject` 要求精确版本错误、零工作。不能将任意失败当作负例通过。两种模式的树都由 PE 客户端先独立确认所有权。

## 故意错误的变体

`mutants/` 在上述诊断源码之后分别应用，互不叠加。native 源码的 `#include "fixture-native.inc"` 应先按原位展开为该文件内容，以重建本轮已运行的精确翻译单元。

| 变体 | 错误行为 | 客户端必须拒绝 |
|---|---|---|
| replay | pending 清理时再次 CREATE | 普通模式，CLI 1 |
| partial | 成功但只执行部分检查 | 普通模式，CLI 1 |
| unowned | 修改预先存在的树 | `--existing`，CLI 1 |
| version | 忽略版本错误 | `--reject`，CLI 1 |
| early | 返回无关的前置错误 | `--existing`，CLI 1 |

这些都是检查器的负例，不能作为实现补丁采用。完整操作还要核对 session、模块/EXE/log 摘要、异常标记、GUI 真实退出、至少两次空观察和普通实验配置恢复；客户端退出本身不是整树清理证明。真实结果与已知边界见 research96。
