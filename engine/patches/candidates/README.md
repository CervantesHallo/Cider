# 独立候选补丁

本目录不由任何当前引擎配方引用。候选代码、模块构建、运行对照和最终采用分开记录；有导出或构建产物不代表API已支持，不用于游戏放行。

| 顺序 | 补丁 | 范围与依赖 | 记录 |
|---|---|---|---|
| 1 | 0009-token-kernel-object-mapping.patch | server token的kernel对象映射、登记/首次引用失败处理；已有CX26.3审计树 | [56](../../../docs/research/56-token-object-candidate-20261013.md) |
| 2 | 0010-ntoskrnl-primary-token-reference.patch | 主令牌引用/释放，依赖0009；请求主体/模拟context组未完成 | [57](../../../docs/research/57-primary-token-candidate-20261013.md) |

两项仅在独立源码/已配置构建副本中编译，SDK15.4；没有安装、执行或打包引擎。固定输入摘要和静态顺序应用见记录。原生生命周期、失败/并发、访问范围及整个权限依赖组通过后，才评审加入配方；不能仅将本目录复制到正常引擎或补丁列表便认为阶段已完成。
