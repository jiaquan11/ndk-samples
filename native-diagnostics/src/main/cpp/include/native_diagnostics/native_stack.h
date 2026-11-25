#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ndksamples::diagnostics {

enum class NativeStackOrigin : std::uint8_t {
  kUnavailable,
  // NativeException 在构造点采集，通常接近业务 throw 位置。
  kExceptionConstruction,
  // 普通 std::exception 到达 JNI catch 后才采集，原抛出路径可能已展开。
  kJniCatchBoundary,
};

// 使用固定容量保存当前线程的原始程序计数器，避免在异常路径
// 中为栈容器进行动态分配。这些是经过返回地址规范化、但仍受 ASLR
// 影响的运行时绝对地址，不能直接上传；Reporter 应在同步回调期间
// 转为模块相对偏移、复制模块名，再结合同版本、同 ABI 的未剥离
// 符号文件解析。共享库卸载后再调用 dladdr() 可能无法恢复模块信息。
struct NativeStackTrace {
  static constexpr std::size_t kMaxFrames = 32;

  std::array<std::uintptr_t, kMaxFrames> program_counters{};
  std::uint16_t frame_count = 0;
  NativeStackOrigin origin = NativeStackOrigin::kUnavailable;
  bool truncated = false;
};

// dladdr() 提供的模块名和符号名由系统加载器持有，只应在对应
// 共享库仍处于加载状态时借用；Reporter 异步保存前必须复制。
struct NativeStackFrameInfo {
  std::uintptr_t program_counter = 0;
  std::uintptr_t relative_program_counter = 0;
  const char* module_path = nullptr;
  const char* symbol_name = nullptr;
  std::uintptr_t symbol_offset = 0;
  bool resolved = false;
};

// 外部 Reporter 可在同步回调期间把绝对 PC 转为模块相对 PC。
NativeStackFrameInfo ResolveNativeStackFrame(
    std::uintptr_t program_counter) noexcept;

}  // namespace ndksamples::diagnostics
