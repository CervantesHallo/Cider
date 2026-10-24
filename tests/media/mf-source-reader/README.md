# MF SourceReader 限定回归

使用固定 Wine 11.18 的原测试 helpers，增加三项明确范围。`probe.patch` 保留原断言；生成结果 SHA256 固定。原完整套件不在这些范围内运行，缺少 `MF_FIXTURE_SCOPE` 会立即失败。

```sh
python3 tests/media/mf-source-reader/build.py \
  --wine-source out/engine-r-v1/msync-source \
  --wine-build out/engine-r-v1/build-msync \
  --output out/mf-source-reader-probes
```

仅复用现有 Wine PE GCC、头文件、导入库和运行库，不重配置或重编译 Wine。`--prepare-only` 只核对重建并记录命令，不重复编译已有相同源。构建不执行 Wine，也不安装或切换引擎。

| `MF_FIXTURE_SCOPE` | 检查内容 |
|---|---|
| `allocator` | fake MFT 的真实 SourceReader 同步/异步 EMPTY 完成、显式重试、Flush、Release；不是物理 GPU 池 |
| `processor` | 实际软件视频 processor 的复制后备、Flush、重复 Flush、无重新协商的新亮帧 |
| `oom-pair` | 两个已接受的异步请求，在专用分配失败诊断模块下各完成一次；不要求跨线程回调的全局顺序 |
| `gpu-forward`（需 `--gpu-forward` 构建） | 自有 MFT 提供真实 D3D11 样本；共享属性匹配时保留身份，不匹配时检查上传后备、元数据及实际像素 |

`--gpu-forward` 只追加 `gpu-forward.patch`，默认构建仍产生原固定 v9 字节。GPU 夹具首轮 PE64 575 项普通检查通过；PE32 诊断出现 native Objective-C 异常，即使主线程报告通过也拒绝验收。详见研究 93；修后须检查完整 native 日志、四种情况及清理，不能靠重试挑一个通过轮次。

运行只使用已登记的私有实验引擎和独立瓶子，经签名 GUI/XPC 启动；正常用户瓶子不替换。示例执行形式：

```sh
ciderctl run -b '<独立实验瓶>' --wait --env MF_FIXTURE_SCOPE=allocator \
  /absolute/path/mf-source-reader-x86_64.exe mfplat
```

`oom-pair` 要求专用诊断 reader：只在 `source_reader_queue_response` 中让前两个 `status == MF_E_SAMPLEALLOCATOR_EMPTY` 的响应节点分配返回真实 `E_OUTOFMEMORY`，普通 S_OK 帧不受影响；须核对两次命中及模块指纹。它不能当普通引擎通过，也不能只凭 mock EMPTY 次数推定故障注入发生。研究 92 保留旧、新实际对照及没有交付诊断标记的首轮失败。

每次运行要绑定 EXE/模块/引擎/环境，保留原失败，确认私有 GUI 退出并取得两次进程空观察。内部 worker 20 秒、最终 watchdog 30 秒；超时为失败。构建成功、这些局部通过、GPU/共享纹理、完整 MF、游戏和 Windows 原生对照分别记状态。
