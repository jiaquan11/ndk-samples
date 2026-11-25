#include <native_diagnostics/native_stack.h>

#include <dlfcn.h>
#include <unwind.h>

#include <cstddef>
#include <cstdint>

#include "internal/runtime.h"

namespace ndksamples::diagnostics {
namespace {

struct UnwindState {
  NativeStackTrace* trace;
  std::size_t frames_to_skip;
};

_Unwind_Reason_Code CollectFrame(_Unwind_Context* context,
                                 void* argument) noexcept {
  auto* state = static_cast<UnwindState*>(argument);
  int instruction_pointer_is_before_instruction = 0;
  std::uintptr_t program_counter = static_cast<std::uintptr_t>(
      _Unwind_GetIPInfo(context,
                       &instruction_pointer_is_before_instruction));
  if (program_counter == 0U) return _URC_NO_REASON;

  // 对上层栈帧，unwinder 通常返回“被调函数返回后继续执行”的地址。
  // 只有它明确表示地址不在指令之前时才减一，避免离线符号化落到
  // 调用语句之后，同时兼容不同 ABI 的指令宽度。
  if (instruction_pointer_is_before_instruction == 0) --program_counter;

  if (state->frames_to_skip > 0U) {
    --state->frames_to_skip;
    return _URC_NO_REASON;
  }

  NativeStackTrace& trace = *state->trace;
  if (trace.frame_count >= trace.program_counters.size()) {
    trace.truncated = true;
    return _URC_END_OF_STACK;
  }

  trace.program_counters[trace.frame_count++] = program_counter;
  return _URC_NO_REASON;
}

}  // namespace

namespace detail {

NativeStackTrace CaptureNativeStackTrace(
    NativeStackOrigin origin,
    std::size_t frames_to_skip) noexcept {
  NativeStackTrace trace;
  trace.origin = origin;

  // _Unwind_Backtrace 的第一个可见帧仍是采集器调用链的一部分，
  // 固定多跳过一帧，再叠加调用方要求的业务跳过数。
  UnwindState state{&trace, frames_to_skip + 1U};
  _Unwind_Backtrace(CollectFrame, &state);
  return trace;
}

}  // namespace detail

NativeStackFrameInfo ResolveNativeStackFrame(
    std::uintptr_t program_counter) noexcept {
  NativeStackFrameInfo frame;
  frame.program_counter = program_counter;
  if (program_counter == 0U) return frame;

  Dl_info info{};
  if (dladdr(reinterpret_cast<const void*>(program_counter), &info) == 0 ||
      info.dli_fbase == nullptr) {
    return frame;
  }

  const std::uintptr_t module_base =
      reinterpret_cast<std::uintptr_t>(info.dli_fbase);
  if (program_counter < module_base) return frame;

  frame.relative_program_counter = program_counter - module_base;
  frame.module_path = info.dli_fname;
  frame.symbol_name = info.dli_sname;
  if (info.dli_saddr != nullptr) {
    const std::uintptr_t symbol_address =
        reinterpret_cast<std::uintptr_t>(info.dli_saddr);
    if (symbol_address <= program_counter) {
      frame.symbol_offset = program_counter - symbol_address;
    }
  }
  frame.resolved = true;
  return frame;
}

}  // namespace ndksamples::diagnostics
