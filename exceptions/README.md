# Exceptions

本示例演示同步 Java/Kotlin → Native 调用中，C++ 异常怎样在 JNI 边界内
结束传播，并被显式转换成当前线程的 Java pending exception。

## 当前调用链

```text
MainActivity.throwsException()
  → RegisterNatives 绑定 ThrowsException()
  → MightThrow() 抛出 std::runtime_error
  → InvokeJniVoid() 在 JNI 边界捕获
  → 生成 NativeDiagnosticEvent，采集 catch 边界栈并写入统一日志
  → 构造 NativeOperationException 并设为 pending exception
  → Native 返回 ART
  → Kotlin catch(RuntimeException) 处理
```

业务 JNI 入口不再各自复制一套 `try/catch + ThrowNew()`：

```cpp
void ThrowsException(JNIEnv* env, jobject /* this */) {
  ndksamples::diagnostics::InvokeJniVoid(
      env,
      ndksamples::diagnostics::JniCallContext{
          "exceptions", "throwsException", NATIVE_DIAGNOSTICS_HERE},
      [] { MightThrow(); });
}
```

`InvokeJniVoid()` 由公共 `:native-diagnostics` Android Library / Prefab 模块
提供，集中负责：

- 拦截领域异常、常见标准异常和非标准 C++ 异常；
- 生成错误域、错误码、模块、操作、源码位置和诊断 ID；
- 区分 NativeException 构造点栈与普通异常的 JNI catch 边界栈；
- 调用可选 Reporter，同时隔离 Reporter 自身失败与递归；
- 把错误映射成符合 Java API 语义的异常；
- 保留入口处已经存在的 Java 异常，不主动清除或替换根因；
- 异常对象创建彻底失败且 JVM 也没有留下可传播异常时，阻止上层误走成功分支。

## 历史退出示例

示例页面还会在后台调用 `ProcessExitCollector`，显示系统当前保留的 Native
crash，以及退出原因被归类为 ANR 的历史记录摘要。该入口和同步 JNI 异常桥彼此独立：前者读取已经
结束的旧进程现场，后者处理当前调用栈中仍可恢复的 C++ exception。

Collector 只在 Android 11/API 30 以上可用，并对每条 trace 设置显式大小
上限。Android 12/API 31 起，Native crash trace 才可能包含原始 tombstone
protobuf；系统允许列表为空或 trace 不可用。示例仅展示最近三条摘要，不做
持久化、去重或上传。

当前筛选不覆盖“ANR 后恢复、最终因其它原因退出”的记录，也不会把截断的
tombstone protobuf 当作完整消息解析。这两项都是轻量示例主动保留的边界。

Kotlin 侧仍在原始同步调用点处理 Java 异常：

```kotlin
try {
    throwsException()
    binding.sampleText.text = getString(R.string.no_exception_thrown)
} catch (e: RuntimeException) {
    binding.sampleText.text = getString(
        R.string.runtime_exception_caught,
        e.message.orEmpty(),
    )
}
```

这里跨过 JNI 的不是原来的 C++ 异常对象。`Throw()` / `ThrowNew()` 只会为
当前线程设置 Java pending exception；Native 代码完成必要清理并返回 ART 后，
Java/Kotlin 异常机制才会继续派发。

## 栈与符号

普通 `std::exception` 被 catch 时，原始 throw 路径已经完成栈展开，所以示例
日志明确标记：

```text
Native stack origin=jni_catch_boundary
  #00 pc=0x... libexceptions.so symbol=<unknown>
```

`pc` 是模块相对地址，不要求发布包携带符号。Release 构建通过：

```groovy
release {
    ndk {
        debugSymbolLevel 'FULL'
    }
}
```

把四个 ABI 的完整符号单独写入：

```text
app/build/outputs/native-debug-symbols/release/native-debug-symbols.zip
```

构建 AAB 时，同一组 `.dbg` 会写入
`BUNDLE-METADATA/com.android.tools.build.debugsymbols/`。

线上解析必须使用同一次构建、同一 ABI 的符号文件；Build ID 可用于确认
Release APK 内已剥离 `.so` 与归档符号是否精确匹配。示例离线解析命令：

```powershell
llvm-addr2line.exe -C -f -i -p -e libexceptions.so.dbg 0x<relative-pc>
```

## 能力边界

- 该守卫适用于外层确有 Java/Kotlin 调用者等待返回的同步 JNI 入口；Native
  自建线程没有可自动返回的 Java 调用栈，需要独立的错误通知和退出协议。
- `NativeException` 在对象构造时采集，接近但不承诺等于后续 `throw` 位置；
  普通标准异常只能保存 JNI catch 边界栈，不能还原已经展开的原始 throw 栈。
- 事件保存固定上限的原始 PC，但不包含 Build ID、ABI、应用版本或符号化
  结果。线上适配器必须补充不可变构建标识并归档精确符号文件。
- `native-diagnostics` 当前发布为静态 Prefab 库；Reporter 状态属于每个最终
  链接它的 `.so`，多个业务 `.so` 需要分别安装 Reporter。
- `std::bad_alloc` 使用低分配路径，不调用外部 Reporter，以免在内存紧张时
  继续放大故障。
- 历史退出读取不安装 signal handler，也不会把 `SIGSEGV` / `SIGABRT` 转成
  Java 异常；真正的故障现场仍由 Android 系统产生。

## Screenshot

![screenshot](screenshot.png)
