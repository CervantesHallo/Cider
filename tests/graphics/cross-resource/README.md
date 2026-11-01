# 双进程 context 资源生命周期

应用五文件 diagnostic.patch，include 本目录，完整 spec／`-Fwinemac.drv`；两张 Unix 表各8槽、watch v2。fixture160B/version2，普通 USER prepared 协议仍96B/version2，pipe报文200B/version1，Native coordinator NDEBUG。

GUI/XPC启动host，普通Windows CreateProcess启动另一架构producer。host拥有真实Root及自己的child placeholder，producer持有实际导出context和RO页。普通QUERY／ACQUIRE／PREPARE先取得ticket／RO，再一次INSTALL到host；独立宿主模型1，真实CANCEL／HostSAFE后宿主模型0，ProducerSAFE后才释放context。不是producer自有HWND的foreign-child路由。

匿名双管道仅传固定宽数据，绑定nonce、递增序号及实际PID／birth／epoch／lease／ticket／CID。host不照抄producer所有权；producer清理报告不算join，必须等待真实创建的进程对象、退出0，再fresh probe缺失并释放观察引用。20秒工作／25秒含清理；正常验收不得强制停止。

host参数`--host <C:\CiderTests\peer.exe> <32hexnonce>`；producer参数`--producer <roothex> <childhex> <nonce> <readhandlehex> <writehandlehex>`由host构造。明确application name、受限HANDLE_LIST、宿主消息泵、Peek后分段读取和共享4秒握手预算。效果先记账再验收耗时，INSTALL不重放。

v4修复无producer时成功／部分HOST_SETUP的观察引用清理：以实际Native回执为准，不要求已经失败的birth／epoch查询，不伪造模型缺失／SAFE；原设置错误仍失败，该分支未运行故障注入。正常64→32及32→64各host55/0、producer28/0、CLI0、kerneljoin/exit0，无强制。

第一方向控制器因前两条preparse日志nonce为0误拒，完整failure保留；只重判原数据，未重跑程序。此后全部完整记录／wire严格nonce绑定。单次fwrite不是跨进程原子性保证，完整JSON和role计数须另核对。原始记录、7份完整清理／恢复／保护摘要见research/125；不证明像素／GPU／compositor、性能、全终止故障、游戏或默认采用。
