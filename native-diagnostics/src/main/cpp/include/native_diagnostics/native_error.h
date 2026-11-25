#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include <native_diagnostics/native_stack.h>

namespace ndksamples::diagnostics {

namespace detail {

// 诊断事件默认只暴露文件名，避免把构建机绝对路径写入 Logcat 或线上事件。
// 返回值仍指向 __FILE__ 字符串字面量内部，因此不引入额外分配。
constexpr const char* SourceFileName(const char* path) noexcept {
  if (path == nullptr) return "";

  const char* file_name = path;
  for (const char* cursor = path; *cursor != '\0'; ++cursor) {
    if (*cursor == '/' || *cursor == '\\') file_name = cursor + 1;
  }
  return file_name;
}

}  // namespace detail

// 线上聚合应优先依赖稳定的错误类别和错误码，而不是解析可能变化的 message。
enum class ErrorCategory : std::uint8_t {
  kInvalidArgument,
  kInvalidState,
  kIo,
  kResourceExhausted,
  kInternal,
  kUnknown,
};

// 项目当前公共基线是 C++17，因此使用轻量结构保存源码位置；未来升级到
// C++20 后，可以在不改变上层错误模型的前提下改用 std::source_location。
// 该结构本身只借用字符指针；直接构造时，file/function 至少要在
// 本次同步调用期间有效。NativeException 会拷贝它们，不保留外部指针。
struct SourceLocation {
  const char* file;
  const char* function;
  std::uint32_t line;
};

// JNI 入口提供的稳定上下文。module 表示 Native 子系统，operation 表示本次
// 对外操作；两者都应使用在本次调用期间有效的稳定常量，不能
// 包含用户输入、密钥、令牌或一次性随机值。
struct JniCallContext {
  const char* module;
  const char* operation;
  SourceLocation source;
};

// 业务 Native 代码可以抛出该类型，显式携带稳定的错误域、错误码和类别。
// module/operation 仍由 JNI 入口提供，避免底层领域代码反向依赖 Java 接口。
class NativeException : public std::runtime_error {
 public:
  NativeException(std::string domain, std::int32_t code,
                  ErrorCategory category, std::string message,
                  SourceLocation source);

  const std::string& domain() const noexcept { return domain_; }
  std::int32_t code() const noexcept { return code_; }
  ErrorCategory category() const noexcept { return category_; }
  // 返回值中的指针借用当前 NativeException 内部字符串，
  // 不能在异常对象被移动或销毁后继续保存。
  SourceLocation source() const noexcept {
    return SourceLocation{source_file_.c_str(), source_function_.c_str(),
                          source_line_};
  }
  const NativeStackTrace& stack_trace() const noexcept {
    return stack_trace_;
  }

 private:
  std::string domain_;
  std::int32_t code_;
  ErrorCategory category_;
  std::string source_file_;
  std::string source_function_;
  std::uint32_t source_line_;
  NativeStackTrace stack_trace_;
};

}  // namespace ndksamples::diagnostics

// 优先使用编译器提供的文件名宏，避免构建机绝对路径字面量残留在发布 so；
// 其它编译器仍回退到 constexpr basename 裁剪。
#if defined(__FILE_NAME__)
#define NATIVE_DIAGNOSTICS_SOURCE_FILE_INTERNAL __FILE_NAME__
#else
#define NATIVE_DIAGNOSTICS_SOURCE_FILE_INTERNAL                        \
  ::ndksamples::diagnostics::detail::SourceFileName(__FILE__)
#endif

// 该宏只能在函数体中使用；文件名、__func__ 和 __LINE__ 都由调用点展开。
#define NATIVE_DIAGNOSTICS_HERE                                      \
  ::ndksamples::diagnostics::SourceLocation {                       \
    NATIVE_DIAGNOSTICS_SOURCE_FILE_INTERNAL,                        \
        __func__, static_cast<std::uint32_t>(__LINE__)              \
  }
