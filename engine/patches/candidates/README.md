# 独立候选补丁

本目录不由任何当前引擎配方引用。候选代码、模块构建、运行对照和最终采用分开记录；有导出或构建产物不代表API已支持，不用于游戏放行。

| 顺序 | 补丁 | 范围与依赖 | 记录 |
|---|---|---|---|
| 1 | 0009-token-kernel-object-mapping.patch | server token的kernel对象映射、登记/首次引用失败处理；已有CX26.3审计树 | [56](../../../docs/research/56-token-object-candidate-20261013.md) |
| 2 | 0012-reference-callers.patch | 构造、排队及异常清理的真实错误/所有权，依赖0009 | [62](../../../docs/research/62-quarantine-kernel-repairs-20261015.md) |
| 3 | 0013-object-reference-lifetime.patch | 同权限稳定句柄、map/ref/FREE同步及对齐，依赖0012 | [62](../../../docs/research/62-quarantine-kernel-repairs-20261015.md) |
| 4 | 0015-close-borrowed-object-cleanup.patch | CLOSE分配IRP失败不误释放借用文件，依赖0013 | 后续真实Wine验证记录 |
| 5 | 0016-ioctl-allocation-buffer-ownership.patch | IOCTL分配失败保留context拥有缓冲并回收独立/替换旧缓冲 | 后续真实Wine验证记录 |
| 6 | 0017-primary-token-release.patch | 仅委托已有对象管理器释放引用；取得接口仍未实现 | [70](../../../docs/research/70-k2-contract-and-refusal-evidence-20261017.md) |

0010已撤出并仅保留在rejected作历史追溯，不能按旧57记录加入队列。当前顺序以[修复队列](QUARANTINE-REPAIR.md)为准。

32/64模块构建采用既有macOS15.4配置。通用事件/引用/回调已在私有Wine进程进行定向验证；它不覆盖token映射、K2或Windows原生kernel，也没有部署常用引擎。原生生命周期、失败/并发、访问范围及整个权限依赖组通过后，才评审加入配方；不能仅复制补丁列表便认为阶段已完成。
