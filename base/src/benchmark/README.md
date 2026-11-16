# Base Native 日志性能基准

本目录提供独立的设备端 Native 可执行程序，用于分层观察 `base` 日志库的
运行成本。它不是正确性单元测试，也不设置跨设备的绝对性能通过线。

## 测量层次

| 层次 | 主要 Case | 能回答的问题 | 不能直接证明的内容 |
| --- | --- | --- | --- |
| 过滤 | 低于最低等级的 `LOGD_TAG` | 保存 `errno`、原子等级读取和分支的合计成本 | 已输出日志的成本 |
| 格式化 | 当前栈优先算法与旧 `vasprintf` 对照 | 512 字节边界及不同正文长度下的格式化差异 | `liblog`、logd 和 Logcat 成本 |
| 分片 | ASCII、多行、UTF-8，64 B～16 KiB | 来源前缀、换行处理和 UTF-8 安全分片的库内成本 | 平台是否接收或限流 |
| 平台突发写入 | 可选的 1/2/4 线程真实 `LOGI_TAG` | 从公开宏到 `liblog` 返回的突发吞吐，以及调用首片的外部观测比例 | 单条调用尾延迟、多片正文完整留存，或单独归因某一项库内优化 |

格式化对照组复刻日志重构前的 `vasprintf` 算法，只用于同一二进制、同一轮
运行中的算法对比。它不是旧提交的完整版本级回放；严格 A/B 仍应在两个提交
上分别构建和交错运行。

## 构建与执行

连接一台 arm64 Android 设备后，在仓库根目录执行：

```powershell
.\base\src\benchmark\run_benchmark.ps1
```

默认只运行不会写入 logd 的库内 Case。需要补充平台突发写入层时执行：

```powershell
.\base\src\benchmark\run_benchmark.ps1 -IncludeLogd
```

快速冒烟可增加 `-Quick`。脚本会：

1. 构建 `unit-test` Release 变体对应的 arm64 `RelWithDebInfo` Native Target；
2. 定位独立的 `base_logging_benchmark`，推送到 `/data/local/tmp`；
3. 记录提交、设备、Android、ABI、电量、温度和统计参数；
4. 把 CSV 输出保存在被 Git 忽略的 `base/build/benchmark/results/`；
5. 运行真实 logd Case 时同步采集指定 Tag，先通过唯一就绪标记确认采集已建立，
   再在看到完成标记后停止采集，并统计带本轮 ID 的调用首片。

平台突发写入结果后会附加 `FIRST_CHUNK_OBSERVATION` 行，分别给出各正文
长度和线程数的预期调用数、实际观测到的首片数与比例，格式为
`FIRST_CHUNK_OBSERVATION,name,expected_calls,observed_first_chunks,ratio`。当单次调用被拆为
多个 logd 条目时，这个指标只能证明携带调用标识的首片被采集，不能证明后续
分片和完整正文都已留存。脚本不清空设备现有 Logcat，而是使用本轮 ID 隔离数据。

脚本同时记录 `worktree_state`。工作区有未提交内容时，文件名带
`dirty-source-<fingerprint>`；其中 source 指纹只覆盖日志实现、基准程序、
采集脚本和关键构建输入，不会因归档结果文件而递归变化。完整工作区状态另有
独立指纹。这类结果可以用于当前审核，但不能冒充只凭 `HEAD` 即可复现的正式
提交基线。提交基准代码后应再运行一次，得到 clean 结果。

该 Native 变体使用 `-O2 -DNDEBUG`，用于代表工程的优化构建路径，不能与
Debug 数据混用。可执行文件不打入 APK，也不依赖 GTest。命令只选择
`unit-test` 的 arm64 Target；由于 `:base` 是 Prefab Project 依赖，Gradle
仍可能按工程现有 ABI 配置刷新 `base` 的全部 Native 产物。

## 统计口径

- 库内 Case 先自适应校准，使每轮接近 20 ms，再预热 3 轮、测量 20 轮；
- 输出每次操作的墙钟中位数、P95、MAD 和进程 CPU 中位数；
- 当前与 `vasprintf` 对照入口都保持相同的非内联调用边界，相邻长度交替
  使用 AB/BA 顺序，降低只对某一算法有利的编译和运行次序偏差；
- 分片 Case 额外输出输入字节吞吐和每次调用产生的片段数；
- 平台突发写入 Case 使用固定迭代数，避免自动校准造成无边界的日志写入；
- 不用一次运行的最快值代表稳定性能，也不把单一设备数字设为 CI 阈值。

`effective_payload_bytes_per_second` 只按调用方正文长度计算，不等于实际写入
logd 的总字节数。多线程突发 Case 的墙钟 `ns/op` 是“整批墙钟时间 ÷
总调用数”，表示聚合吞吐对应的单位成本，不是每个线程中单条调用的延迟。
若要研究尾延迟，需要另行逐调用采样。

真实 logd 结果会同时受到设备温控、CPU 调度、logd 缓冲、Logcat 消费速度
和平台限流影响。`ns/op` 只描述写调用返回速度；当首片观测比例小于 1 时，
快速返回可能同时伴随调用首片未被观测，不能解释成“成功送达更快”。并发数据
适合观察平台突发趋势，不应反推格式化或分片本身的成本。正式比较两个实现
时，应固定设备和环境，交错运行多次，并结合中位数、P95、MAD 与接收数量
一起判断。

## 已归档结果

基准框架提交后，应在没有未提交变更的工作区中使用同一设备重新运行，
再将 clean 基线的结果摘要和脱敏 CSV 归档到 `results/` 目录。
