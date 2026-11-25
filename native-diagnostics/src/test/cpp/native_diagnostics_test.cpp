#include <native_diagnostics/native_error.h>
#include <native_diagnostics/native_stack.h>

#include <cstdint>
#include <string>

#include "gtest/gtest.h"

namespace ndksamples::diagnostics {
namespace {

TEST(NativeDiagnosticsTest, NativeExceptionOwnsStructuredFields) {
  std::string file = "decoder.cpp";
  std::string function = "Decode";
  const SourceLocation source{file.c_str(), function.c_str(), 27};
  const NativeException exception("decoder", 2001, ErrorCategory::kIo,
                                  "decode failed", source);

  // NativeException 拷贝源码位置，不依赖构造参数后续存活。
  file.assign("changed.cpp");
  function.assign("Changed");

  EXPECT_STREQ("decode failed", exception.what());
  EXPECT_EQ("decoder", exception.domain());
  EXPECT_EQ(2001, exception.code());
  EXPECT_EQ(ErrorCategory::kIo, exception.category());
  EXPECT_STREQ("decoder.cpp", exception.source().file);
  EXPECT_STREQ("Decode", exception.source().function);
  EXPECT_EQ(27U, exception.source().line);
  EXPECT_EQ(NativeStackOrigin::kExceptionConstruction,
            exception.stack_trace().origin);
  EXPECT_GT(exception.stack_trace().frame_count, 0U);
}

TEST(NativeDiagnosticsTest, SourceMacroUsesFileName) {
  const SourceLocation source = NATIVE_DIAGNOSTICS_HERE;

  EXPECT_STREQ("native_diagnostics_test.cpp", source.file);
  EXPECT_STREQ("TestBody", source.function);
  EXPECT_NE(0U, source.line);
}

TEST(NativeDiagnosticsTest, CapturedStackContainsResolvableModuleFrame) {
  const NativeException exception("test", 1, ErrorCategory::kInternal, "test",
                                  NATIVE_DIAGNOSTICS_HERE);
  const NativeStackTrace& trace = exception.stack_trace();

  EXPECT_EQ(NativeStackOrigin::kExceptionConstruction, trace.origin);
  ASSERT_GT(trace.frame_count, 0U);

  bool resolved_frame_found = false;
  for (std::uint16_t index = 0; index < trace.frame_count; ++index) {
    EXPECT_NE(0U, trace.program_counters[index]);
    const NativeStackFrameInfo frame =
        ResolveNativeStackFrame(trace.program_counters[index]);
    if (frame.resolved && frame.module_path != nullptr &&
        frame.module_path[0] != '\0') {
      resolved_frame_found = true;
    }
  }
  EXPECT_TRUE(resolved_frame_found);
}

}  // namespace
}  // namespace ndksamples::diagnostics
