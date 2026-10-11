# HY2：wineserver 共享 token 读锁依赖审查（2026-10-11）

整合复核：父代理独立重新计算下列十份源码SHA256，全部匹配，并抽查adjust_token_privileges的访问权/原地修改、call_req_handler立即回复与kill_thread清理链。该抽查不代表全表或方案已运行验证；延期回复、锁域和native条件继续待实现/验证。

仅源码审查；未实施、编译或运行。复用 [28](28-security-context-baseline-20261007.md) 与 [33](33-token-dependency-reuse-20261007.md)，原生 Windows kernel 结果仍未取得。指定报告写入前不存在。

固定根：`/Users/cervantes/Library/Caches/Cider/engine-build/cider-cx26.3-r1-x86_64/src`；下列文件位置均相对其 `server/`，哈希固定本次实际内容，不沿用33的上游快照身份。

微软契约：[捕获](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-secapturesubjectcontext)保存 token 引用；[锁定](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-selocksubjectcontext)取得 primary/client token 读锁；每次须平衡[解锁](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-seunlocksubjectcontext)。这是跨调用一致性要求。

源码事实（P=`protocol.def`；请求名对应 `DECL_HANDLER`）：

| 请求/读写入口 | 实际对象与变化 | 精确位置/协议 |
|---|---|---|
| `adjust_token_privileges` | 同一 token 原地 enable/remove/disable-all；读取 previous state 后写；需 `TOKEN_ADJUST_PRIVILEGES`，取旧值另需 QUERY | `token.c:1252–1289`；`token_adjust_privileges:824–855`、`token_disable_privileges:858–866`；P:3463 |
| `set_thread_info` → `security_set_thread_token` | 替换目标 `thread->token`；零 handle 撤销；释放旧引用，接管新引用；需 THREAD_SET_INFORMATION/TOKEN_IMPERSONATE | `thread.c:1896–1905,923–924`；`token.c:179–199`；P:1305,1319 |
| `ioctl(FSCTL_PIPE_IMPERSONATE)` → `pipe_server_ioctl` | 新建模拟 token、替换 `current->token`；**FIXME：复制服务端自身主令牌，未采用客户端 token** | `fd.c:3169–3180`；`named_pipe.c:1325,1380–1387`；P:2553 |
| `set_process_info` / `grant_process_admin_token` | 前者把已存在 token 引用赋给 `process->token`，后者新建再替换；均需 PROCESS_SET_INFORMATION并释放旧引用；前者另需 TOKEN_QUERY/ASSIGN_PRIMARY | `process.c:1904–1924,2011–2025`；P:1253,1266,3825 |
| `new_process` → `create_process` | 显式 token 或父 primary **复制**为新 primary；不继承当前模拟 token；无父进程时创建 admin token | `process.c:1309,1471–1483,883–909`；P:1086 |
| `create_token` / `duplicate_token` / `filter_token` / `create_linked_token` | 创建新对象；过滤修改新对象的组/特权；`primary`、`impersonation_level` 在创建时赋值，非槽位替换 | `token.c:1118,1325,1350,1605`；`create_token:469–493`、`token_duplicate:576–660`；P:3413,3491,3501,3847 |
| `open_token`；`get_token_privileges/info/sid/groups/default_dacl`；`check_token_privileges`、`access_check` | 打开原对象/查询；QUERY读；`token_check_privileges`读 enabled；隐式对象鉴权选当前模拟，否则 primary | `token.c:1216,1294,1533,1457,1495,1558,1378,1403`；`token_check_privileges:869`、`check_object_access:1087–1109`；`thread_get_impersonation_token`在`thread.c:1649`；`security.h:126–133`；P:3433,3474,3831,3523,3531,3541,3482,3511 |
| `set_token_default_dacl` / `set_security_object` → `token_set_sd` | 前者原地写 token 默认DACL且不更新 modified_id；后者写对象SD，屏蔽LABEL，不改 token 用户/组/特权；两者不可混同 | `token.c:1579–1601,173–176`；`handle.c:733–758`；P:3548,3553 |

对象身份：`struct token`含对象头、token_id、modified_id（`token.c:103–118`）。`create_token`分配新ID（`token.c:480`）；`token_duplicate`可能沿用 modified_id（`token.c:581–599`）。`open_token`与`duplicate_handle`共享原对象（`token.c:1228,1242`、`handle.c:573–627`）；句柄值、版本号均不能代替对象身份。宿主互斥管不到其他 guest 的特权写；永久副本会丢失捕获后、锁前的真实变化。

引用/清理：`get_handle_obj`检查访问权并 grab（`handle.c:469–499`）；`close_handle`/`handle_table_destroy`释放引用（`handle.c:427–443,170–198`）。失联经 `thread_poll_event`（`thread.c:596`）、`read_request`/`write_reply`（`request.c:368–369,243–244`）、`process_poll_event`（`process.c:1038`）进入kill。`kill_thread`（`thread.c:1609–1631`）清请求并移出进程，最后线程触发 `process_killed`（`process.c:1138–1146`）关闭句柄。token引用却在 `destroy_thread`（`thread.c:653–662`）、`process_destroy`（`process.c:931–954`）才释放；外部引用可延寿。引用归零才析构（`object.c:537–558`、`token_destroy`在`token.c:435–458`），不能靠析构及时撤销锁。

最小方案（提案）：新增 capture/lock/unlock/release 协议及 server context对象，原子捕获经确认的请求主体两份 token引用/level，context有宿主所有者与不可复用cookie；不以winedevice自己的 `current`冒充主体。capture确认类型/权限，失败完整回滚，沿用33的kernel对象映射；token增共享读计数，lock一次取得两对象（去重、嵌套计数），unlock平衡；release归还引用。槽位替换保持旧context引用，不自动重选token；是否需额外阻塞赋值待原生验证。

写门控放在特权旧值读取和默认DACL写之前。冲突请求保存输入、已解析对象/访问权并延迟真实回复，解锁后执行；新建/复制不写源对象。`call_req_handler`立即回复、`read_request`随后释放输入（`request.c:292–324,360–362`），因此须补非阻塞延期/取消机制，不能在handler堵住事件循环（`fd.c:983–1000`）。独立锁租约绑定持有连接；连接死亡在 `cleanup_thread`释放输入前撤销读计数/待写并唤醒等待，重复清理幂等；宿主退出清全部context，主体退出仍保留存活context。跨工作线程移交须明确所有权；不能把未取得锁当成功返回。

对抗性复核：只给lock增加计数而漏改普通guest写入口仍会穿透；仅保存等待者handle会遇到关闭/复用，须固定对象引用；等写者时禁止新读者的策略也不能把已有读者的嵌套lock卡死。以上均为方案的必要审查条件，非已实现行为。

待原生条件/父代理必复核：捕获→修改→锁定、锁内跨进程enable/remove/disable与DACL写的事件顺序；双token/别名/重复锁配对；槽位替换和主体退出后的引用；锁持有者退出/失联及延期写取消、句柄复用、饥饿。另确认对象SD是否属于同一锁域、请求者映射与命名管道身份缺口。收益是锁覆盖共享实对象；成本是协议及请求生命周期改动。验收须自有通用API参考驱动与候选逐项对照；独立补丁可整体撤回，本报告不提升兼容状态。

本次 SHA256（只读采集）：

```text
3f4ede2a7499939d11e2d9b10e9dfd6fc08572f8cbc5361317ef6654c199cbc0  token.c
ad946fe2e7d12245d11005b07ceaa806339a53729c4afb391d93bf8fa6f5d971  thread.c
633c80ed3b79acd4da4eac2021f82572e0fdec3281266a68b3fbad52eeb68b2c  process.c
efcfc4fef3a0e2c02ada7eeae79a32cc947b1537d4f218caad220b2a6bac3c2b  protocol.def
cecaf58e496cd5f976a42fa4097c49b9d8f6a6621fb11e41c5d8c0d4a09d869b  named_pipe.c
b27eddb334f64515d29ac6b9404d4dd7beacfad6b5502ed2ca64fbb0a36e9b73  request.c
e7203d62046bf2188902ae742cc8a590b9f4344c6c3b29b43d420d969add57f6  handle.c
9345f5c644ce544c3d29d6c7f604bf1964d3c81de14fb5287977e99ac7580244  object.c
ded71616b01527dc23826cac16f29ca4be925838f6a28bb28cddd7fb53aa656d  security.h
7896c3e09d7a388a77d2c99ad8c8572ee03deb789db60e0c0c2965440a442993  fd.c
```
