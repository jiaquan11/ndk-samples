#pragma once

#include <cstdint>

#include <native_diagnostics/native_error.h>

namespace ndksamples::diagnostics {

// NativeError 中的字符串均为本次同步 Reporter 回调期间有效的借用指针。
// Reporter 如果需要异步保存或上传，必须在回调返回前复制所需字段。
// 回调接收的是原始事件，不会像 Logcat 出口那样自动限长或脱敏；
// 连接线上 SDK 的适配器必须根据产品策略限长、脱敏、采样并复制数据。
// stack_trace 中的 PC 是当前进程的绝对地址；适配器还必须在本次同步
// 回调中解析并复制模块名与模块相对 PC，不能原样上传绝对地址。
struct NativeError {
  const char* domain;
  std::int32_t code;
  ErrorCategory category;
  const char* module;
  const char* operation;
  const char* message;
  SourceLocation source;
};

struct NativeDiagnosticEvent {
  std::uint64_t diagnostic_id;
  NativeError error;
  std::int64_t thread_id;
  // NativeException 保存构造点轨迹；普通 std::exception 只能在 JNI catch
  // 边界采集，原始 throw 路径此时已经展开。必须结合 origin 解读。
  NativeStackTrace stack_trace;
};

// Reporter 只负责消费结构化事件，不负责改变 JNI pending exception。
// 回调可能从多个线程并发进入，安装方必须保证自身线程安全；同一线程递归
// 上报时，公共实现只保留 Logcat 记录，不会再次调用回调。外部 Reporter
// 即使错误地抛出 C++ 异常，公共实现也会在边界内捕获并回退到 Logcat，
// 不允许 Reporter 失败覆盖原始业务错误。
//
// native-diagnostics 当前以静态 Prefab 库交付：每个最终链接它的 .so 都拥有
// 独立 Reporter 状态，并非全进程单例。每个 .so 都应分别安装回调。
// SetNativeErrorReporter(nullptr) 只停止后续进入，不等待其它线程上已在执行
// 的回调结束；回调代码及其依赖必须活到所有在途调用完成。
using NativeErrorReporter = void (*)(const NativeDiagnosticEvent& event);

void SetNativeErrorReporter(NativeErrorReporter reporter) noexcept;

}  // namespace ndksamples::diagnostics
