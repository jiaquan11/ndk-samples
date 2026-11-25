# Native Diagnostics

`native-diagnostics` 是仓库内可复用的 Android Library / Prefab 模块，提供
两条刻意分开的轻量诊断路径：

1. 在当前进程仍可正常执行时，把同步 JNI 入口中的 C++ 异常收口为稳定结果；
2. 在应用下次启动后，通过 `ApplicationExitInfo` 只读查询当前应用包的 Native
   crash，以及退出原因被归类为 ANR 的历史现场。

同步 JNI 路径会产生三项结果：

1. 一次限长的统一 Native 日志写入尝试；
2. 一个可选的结构化 `NativeDiagnosticEvent`；
3. 一个可返回 Java/Kotlin 同步调用者的 pending exception。

历史退出路径只复制系统已经保存的事实和限长原始 trace，不安装 signal
handler，也不内置去重、落盘、上传或 tombstone 解析器。两条路径都不是
完整的 Native 崩溃捕获平台。

## 模块结构

```text
native-diagnostics
├── Java 契约
│   ├── NativeOperationException  同步 JNI 领域异常
│   ├── ProcessExitCollector      API 30+ 历史退出只读采集器
│   └── ProcessExitRecord         不可变退出记录与 trace 状态
└── C++ / Prefab
    ├── native_error       稳定错误域、错误码、分类与源码位置
    ├── native_stack       固定容量栈采集、模块相对地址与符号查询
    ├── reporter           结构化事件、关联 ID 与可选上报出口
    ├── exception_bridge   Java 异常对象创建与已有 pending 保留
    └── jni_boundary       同步 JNI 入口的统一 C++ catch 边界
```

## 消费方式

Gradle 模块依赖：

```kotlin
dependencies {
    implementation(project(":native-diagnostics"))
}
```

`native-diagnostics` 的静态链接接口依赖 `base::base`。当前 Prefab 不会因
`//base:base` 自动创建另一个 Package 的导入目标，因此消费端 CMake 需要
按以下顺序加载：

```cmake
find_package(base CONFIG REQUIRED)
find_package(native-diagnostics CONFIG REQUIRED)

target_link_libraries(my-jni-lib
    PRIVATE
    native-diagnostics::native-diagnostics
)
```

如果以本地 AAR 方式交付，必须同时交付 `base.aar` 和
`native-diagnostics.aar`；如果发布到 Maven，POM 也必须保留对
`base` 的传递依赖。

## 历史进程退出用法

`ProcessExitCollector` 运行在普通 Android 层，与 C++ Reporter 没有调用或
状态依赖。查询范围显式限定为当前应用包，同一包的多进程仍会保留；结果只关注
`REASON_CRASH_NATIVE` 和 `REASON_ANR`：

```kotlin
if (ProcessExitCollector.isSupported()) {
    // 必须放到应用自己的后台线程执行。
    val records = ProcessExitCollector.collect(
        applicationContext,
        8,             // 交给系统查询的最大历史记录数
        1024 * 1024,   // 每条原始 trace 最多复制 1 MiB
    )
}
```

Android 11/API 30 起可以读取历史退出元数据和 ANR trace；Android 12/API 31
起，Native crash 的 trace 才可能是 tombstone protobuf。系统使用有限循环
缓冲保存这些数据，因此列表为空或 trace 状态为 `NOT_AVAILABLE` 都是合法
结果，不能据此证明历史上没有发生故障。

当前 reason 筛选并不覆盖“发生 ANR 后恢复、最终因其它原因退出”的进程；即使
平台把此前 ANR trace 附到那条其它原因的退出记录上，本 Collector 也不会返回。
这是为保持轻量数据模型而主动接受的范围边界。

`ProcessExitRecord` 保留时间、PID、进程名、原始 reason/status、importance、
PSS/RSS、人类可读说明、trace 状态和限长字节。ANR trace 与 tombstone
protobuf 都保持原始格式，不在手机端解析。`description` 的格式不稳定，
只能展示或辅助人工判断，不能作为机器协议。

`TRUNCATED` 的 ANR 文本前缀可能仍有人工阅读价值，但被任意截断的 tombstone
protobuf 通常不再是完整可解析消息。需要离线解析时，调用方必须先检查状态，
并且只把 `COMPLETE` 作为完整 protobuf 输入。

Collector 是无状态的，重复调用可能返回相同系统记录。最终应用应在记录
可靠保存或上传成功后，再使用以下字段组合建立候选指纹：

```text
timestamp + pid + processName + platformReason + status
```

这组字段不是平台保证唯一的事件 ID，消费端仍需幂等处理；存在完整 trace 时可
追加内容摘要。这样可以避免“刚读取就标记完成、后续保存失败”造成诊断数据丢失。
历史故障版本、ABI、Build ID、隐私脱敏、文件保留和网络重试同样属于最终应用
或发布系统，不进入这个只读采集器。新进程可能已经升级，不能用当前启动版本
代替旧进程版本；Native crash 应优先用 tombstone Build ID 匹配同构建符号。

## JNI 入口用法

```cpp
void ReadConfiguration(JNIEnv* env, jobject /* this */) {
  ndksamples::diagnostics::InvokeJniVoid(
      env,
      ndksamples::diagnostics::JniCallContext{
          "configuration", "read", NATIVE_DIAGNOSTICS_HERE},
      [] {
        // 调用领域代码；异常不能逃出这个 JNI 边界。
        ReadNativeConfiguration();
      });
}
```

`module` 和 `operation` 是用于聚合的稳定维度，不应包含用户输入、令牌、
密钥或随机值。`NATIVE_DIAGNOSTICS_HERE` 保存文件名、函数名和行号，
不会把构建机绝对路径写入事件。

### 异常映射

| C++ 输入 | Java 输出 | 稳定错误码 |
| --- | --- | ---: |
| `NativeException` | `NativeOperationException` | 由领域代码提供 |
| `std::invalid_argument` | `IllegalArgumentException` | `1001` |
| `std::bad_alloc` | `OutOfMemoryError` | `1002` |
| 其它 `std::exception` | `NativeOperationException` | `1003` |
| 非标准 C++ 异常 | `NativeOperationException` | `1099` |

`bad_alloc` 路径只输出最小 Logcat 信息，不调用外部 Reporter，避免在
内存已紧张时进一步复制、排队或格式化大量数据。

## 两种 Native 栈来源

模块不会把所有栈都笼统叫作“异常堆栈”，而是在事件中记录明确来源：

| `origin` | 采集位置 | 能说明什么 | 不能说明什么 |
| --- | --- | --- | --- |
| `exception_construction` | `NativeException` 构造时 | 接近领域错误被建立的位置 | 如果对象稍后才被 `throw`，不等同于 throw 指令位置 |
| `jni_catch_boundary` | 普通异常到达 `InvokeJniVoid` 的 catch 后 | JNI 边界在什么调用链中处理失败 | 原始 throw 路径已经展开，不能声称是完整 throw-site 栈 |
| `unavailable` | 未采集或低内存退化 | 明确表示没有轨迹 | 不应根据空数组推断错误位置 |

采集器基于 `_Unwind_Backtrace()`，使用固定 32 帧数组，不在采集过程中为
容器动态分配内存。`_Unwind_GetIPInfo()` 会把上层返回地址规范化到实际调用
指令，避免离线解析落到调用后的下一行。`bad_alloc` 分支仍保持空栈，避免
在内存耗尽路径继续增加工作。

事件中的 `program_counters` 是受 ASLR 影响的进程绝对地址。外部 Reporter
必须在同步回调期间调用 `ResolveNativeStackFrame()`，复制模块名和模块相对
PC 后再异步保存；不能原样上传绝对地址，也不能等共享库卸载后再解析。

## Pending exception 规则

当前实现只保留一条规则：如果线程已经存在 Java pending exception，就保留
它并尽快返回 ART；只有当前没有异常时，才创建本次 Native 错误对应的 Java
异常。原异常通常更接近根因，而且在没有真实业务需求时，提前提供“清除旧
异常并强制替换”的接口只会扩大状态机和失败分支。

如果查类、字符串转换或对象创建失败，JVM 通常会留下更直接的新异常，模块
同样保留它。只有整个桥接过程结束后仍没有任何 pending exception，才使用
`FatalError()` 阻止 Java/Kotlin 误走成功路径。

## Reporter

```cpp
void UploadEvent(const ndksamples::diagnostics::NativeDiagnosticEvent& event) {
  // 所有 const char* 都只在本次同步回调期间有效；
  // 若需要异步排队，必须先拷贝必要字段。
}

ndksamples::diagnostics::SetNativeErrorReporter(UploadEvent);
```

Reporter 必须支持多线程并发进入。公共实现会阻止同一线程递归回调，
并捕获 Reporter 自身抛出的 C++ 异常，不让附加上报覆盖业务失败。
Reporter 收到的是原始事件，不自动限长或脱敏；连接线上 SDK 时，适配器
必须自行复制借用字段，并按产品策略做限长、脱敏、采样与队列背压。
栈字段同样是原始运行时数据：适配器还需同步转换为模块相对 PC，并补充
ABI、应用版本和不可变构建标识。

当前产物是静态 Prefab 库，Reporter、递增序号和递归标记都属于每个
最终链接镜像，不是全进程单例：

```text
libdecoder.so → 一份 Reporter 状态
libplayer.so  → 另一份 Reporter 状态
```

因此多 `.so` 项目需要在每个消费端分别安装 Reporter。注销回调只阻止
后续进入，不会等待其它线程上已在执行的回调结束。

## Release 符号归档与离线解析

栈事件只记录地址，不应让 Release APK 为了可读日志而携带完整调试节。
最终应用可以只在 Release 构建中配置：

```groovy
buildTypes {
    release {
        ndk {
            debugSymbolLevel 'FULL'
        }
    }
}
```

Android Gradle Plugin 会把四 ABI 的 `.dbg` 文件写入：

```text
app/build/outputs/native-debug-symbols/release/native-debug-symbols.zip
```

构建 AAB 时，同一组 `.dbg` 会放入包内的
`BUNDLE-METADATA/com.android.tools.build.debugsymbols/`，不需要再把未剥离
`.so` 塞进应用运行目录。

实际发布 APK 中的 `.so` 保持剥离。一次可靠的离线解析必须同时匹配：

```text
模块名 + 模块相对 PC + ABI + 不可变构建标识 + 同构建符号文件
```

例如将符号包中的对应文件准备好后执行：

```powershell
llvm-addr2line.exe -C -f -i -p `
    -e libexceptions.so.dbg 0x<relative-pc>
```

当前链接器已给产物写入 GNU Build ID，可在归档阶段用 `llvm-readelf -n`
核对 APK 内 `.so` 与 `.dbg` 是否属于同一次构建。当前事件模型尚未自动
读取 Build ID；在线上 Reporter 接入前，应由发布系统保存 Build ID 清单或
等价的不可变产物标识，不能拿“相近版本”的符号文件猜测解析。

## 当前边界

- 只适用于同步 Java/Kotlin → Native 入口；Native 自建线程需要另一套
  错误通知与退出协议。
- `diagnosticId` 只在当前最终 `.so` 的本次加载生命周期内单调唯一，
  不是跨进程、跨设备或跨 `.so` 的全局 ID。
- `IllegalArgumentException` 和 `OutOfMemoryError` 不携带 `diagnosticId`；
  只有 `NativeOperationException` 把该 ID 暴露给 Java/Kotlin。
- Java 异常消息最多转换 1024 个 UTF-16 code units；不完整 UTF-8 序列以
  U+FFFD 替换，嵌入 NUL 不在 C 字符串接口契约内。
- 当前事件包含最多 32 帧的原始轨迹，但不包含 Build ID、ABI、应用版本和
  离线符号化结果；普通异常的轨迹还是 catch 边界，而非原始 throw 栈。
- `ProcessExitCollector` 只在 API 30+ 事后读取系统保留的历史，不覆盖低版本，
  也不能保证循环缓冲中的每条记录仍有 trace。
- 本模块不自建 signal handler，不在崩溃线程中调用日志、JNI、栈展开或
  Reporter，也不尝试从 `SIGSEGV` / `SIGABRT` 恢复执行。
- 真正需要低版本覆盖、实时聚合、完整 minidump、服务端符号化与告警时，
  应由最终应用接入成熟崩溃 SDK 或另行建设平台。

## 验证入口

仓库的 `:unit-test:app` 同时包含纯 C++ GTest 与真实 `JNIEnv`
instrumentation 测试。纯 C++ 测试聚焦错误字段所有权、源码位置和栈解析；
设备测试覆盖异常映射、已有 pending 保留、Reporter 关联与自身失败、
低内存路径、UTF-8 边界，以及历史退出采集器在真实平台上的非破坏性边界。
普通 JVM 测试覆盖 trace 的完整、临界、截断、读取失败和字节所有权。真实
`SIGABRT`、`SIGSEGV` 与 ANR 会终止或阻塞测试进程，只适合作为独立人工
实验，不进入常规 instrumentation 必过用例。

```powershell
.\gradlew.bat :unit-test:app:connectedDebugAndroidTest
```
