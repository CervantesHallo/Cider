# R v1：三进程的实际双矩形窗口图像

2026-11-01，UTC+8。接续[128](128-r-v1-three-pixel-20261101.md)、[129](129-r-v1-window-images-20261101.md)。在同一host／两个producer原四阶段流程上增加显式`--capture-images`，producer仍各自GPU-only。两者红帧完成且模型都1后，准确窗口取图／gate确认再回复；两者蓝帧完成后再取图，随后保持A完整退出／observer归还、B新读仍ACTIVE的原顺序。

| 方向 | 实际宿主／两个生产者 | 窗口图像与回收 |
|---|---|---|
| PE64 host＋两个PE32 producer | 144／35／35检查，均0失败 | 红红→蓝蓝，CLI0，两次kerneljoin/exit0，无强制 |
| PE32 host＋两个PE64 producer | 144／35／35检查，均0失败 | 同上 |

实际WindowServer PNG为864×468，两个独立矩形边界[22,78,377,273]及[422,78,777,273]，各69776像素；总139552红→139552蓝，对立颜色为0，中间跨全部共同高度有40列不透明白色间隔。两方向各自真实捕获，图片恰好同字节：[红红](../../tests/graphics/three-images/two-red.png)、[蓝蓝](../../tests/graphics/three-images/two-blue.png)。host实际像素入口仅两次HOST_WINDOW(3)，不代替producer绘制；精确64B ready／gate、窗口ID／Mac PID／Root／A的CID／nonce及两次host ACK绑定图片。B的真实GPU／身份／挂层和独立退出另行核对，不把同色图片当作不同颜色逐producer对应关系证明。

限定host增量源审无P1/P2，双PE严格构建。图像判定器初审发现四个P2：没核对白色间隔、非矩形台阶可过、丢弃短run遗漏连接桥、阈值包含边界。修后对所有匹配run进行完整8邻接连通归并，真实面积必须填满矩形，严格>10000像素／>60行，并逐行验证贯穿间隔；有限读取、≤2048尺寸、≤100000 run标签及明确PNG子集检查。复审无遗留P1/P2。11个明确标为synthetic的判定器控制覆盖评审反例及正／负条件，全部符合预期；这些控制不充当渲染证据。

9组有效编译闭包、252项绑定输入、原raw/session／双EXE／16PE／2Native／F27／SCK辅助程序源与二进制／4实际清理／原配置DLL字节或缺失恢复／四保护摘要另行核对。旧Native评审曾把PROJECT_MEMORY当输入记录；当前记忆已更新，构建前以准确Git df52ade文档字节固化历史阅读上下文，所有真正代码与编译依赖仍逐字核对，不回滚记忆或改旧评审。诊断五文件补丁精确重建，见[源码](../../tests/graphics/three-images/)与[证据](evidence/r-v1-three-images-20261101.json)。

这完成指定两／三进程窗口图像与合作回收集合；不同颜色的逐producer空间对应、物理显示／输入、性能及真实renderer／foreign HWND前端仍待。下一回到原计划的实际图形消费者接入／CEF，在该路径验证不同图案与空间对应，不另用更多元数据用例替代采用。默认引擎、游戏评级／门禁保持，Windows参考机未操作。
