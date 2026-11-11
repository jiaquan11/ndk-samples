#include <base/logging.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "logging_splitters.h"

namespace ndksamples::base {
namespace {

struct CapturedChunk {
  LogSeverity severity;
  std::string tag;
  std::string message;
};

std::vector<CapturedChunk> CaptureChunks(LogSeverity severity, const char* tag,
                                         const char* file, unsigned int line,
                                         const std::string& message) {
  std::vector<CapturedChunk> chunks;
  SplitByLogdChunks(severity, tag, file, line, message.c_str(),
                    [&](LogSeverity chunk_severity, const char* chunk_tag,
                        const char* chunk_message) {
                      chunks.push_back({chunk_severity, chunk_tag,
                                        std::string(chunk_message)});
                    });
  return chunks;
}

TEST(BaseLoggingSplitter, PreservesEmptyMessage) {
  const auto chunks = CaptureChunks(INFO, "base-test", nullptr, 0, "");

  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ(INFO, chunks[0].severity);
  EXPECT_EQ("base-test", chunks[0].tag);
  EXPECT_EQ("", chunks[0].message);
}

TEST(BaseLoggingSplitter, PreservesSingleAndMultilineMessages) {
  const auto single =
      CaptureChunks(INFO, "base-test", nullptr, 0, "single line");
  ASSERT_EQ(1U, single.size());
  EXPECT_EQ("single line", single[0].message);

  const auto multiline =
      CaptureChunks(INFO, "base-test", nullptr, 0, "first\nsecond\n");
  ASSERT_EQ(1U, multiline.size());
  EXPECT_EQ("first\nsecond\n", multiline[0].message);
}

TEST(BaseLoggingSplitter, PreservesLeadingAndRepeatedEmptyLines) {
  const auto leading =
      CaptureChunks(INFO, "base-test", nullptr, 0, "\n\nsecond");
  ASSERT_EQ(1U, leading.size());
  EXPECT_EQ("\n\nsecond", leading[0].message);

  const auto empty_lines =
      CaptureChunks(INFO, "base-test", nullptr, 0, "\n");
  ASSERT_EQ(1U, empty_lines.size());
  EXPECT_EQ("\n", empty_lines[0].message);
}

TEST(BaseLoggingSplitter, SplitsAtLineBoundaries) {
  constexpr char kTag[] = "base-test";
  const std::size_t max_size = kLoggerEntryMaxPayload - std::strlen(kTag) - 35;
  const std::string line(max_size / 2 + 1, 'x');

  const auto chunks =
      CaptureChunks(INFO, kTag, nullptr, 0, line + "\n" + line + "\n" + line);

  ASSERT_EQ(3U, chunks.size());
  EXPECT_EQ(line, chunks[0].message);
  EXPECT_EQ(line, chunks[1].message);
  EXPECT_EQ(line, chunks[2].message);
}

TEST(BaseLoggingSplitter, RetainsOversizedLineTruncationBehavior) {
  constexpr char kTag[] = "base-test";
  const std::size_t max_size = kLoggerEntryMaxPayload - std::strlen(kTag) - 35;
  const std::string oversized(max_size + 100, 'x');

  const auto chunks =
      CaptureChunks(INFO, kTag, nullptr, 0, oversized + "\ntail");

  ASSERT_EQ(2U, chunks.size());
  EXPECT_EQ(max_size, chunks[0].message.size());
  EXPECT_EQ(std::string(max_size, 'x'), chunks[0].message);
  EXPECT_EQ("tail", chunks[1].message);
}

TEST(BaseLoggingSplitter, DelegatesUnbrokenOversizedMessageToLiblog) {
  constexpr char kTag[] = "base-test";
  const std::size_t max_size = kLoggerEntryMaxPayload - std::strlen(kTag) - 35;
  const std::string oversized(max_size + 100, 'x');

  const auto chunks = CaptureChunks(INFO, kTag, nullptr, 0, oversized);

  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ(oversized, chunks[0].message);
}

TEST(BaseLoggingSplitter, HandlesExactPayloadBoundaryBeforeNewline) {
  constexpr char kTag[] = "base-test";
  const std::size_t max_size = kLoggerEntryMaxPayload - std::strlen(kTag) - 35;
  const std::string exact(max_size, 'x');

  const auto chunks = CaptureChunks(INFO, kTag, nullptr, 0, exact + "\ntail");

  ASSERT_EQ(2U, chunks.size());
  EXPECT_EQ(exact, chunks[0].message);
  EXPECT_EQ("tail", chunks[1].message);
}

TEST(BaseLoggingSplitter, AddsReadableFatalSourcePrefix) {
  const auto chunks = CaptureChunks(FATAL_WITHOUT_ABORT, "base-test",
                                    "source.cpp", 42, "fatal-priority message");

  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ("source.cpp:42] fatal-priority message", chunks[0].message);
}

TEST(BaseLoggingSplitter, RepeatsSourcePrefixForFatalMultilineMessage) {
  const auto chunks = CaptureChunks(FATAL_WITHOUT_ABORT, "base-test",
                                    "source.cpp", 42, "first\nsecond");

  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ("source.cpp:42] first\nsource.cpp:42] second", chunks[0].message);

  const auto leading_empty = CaptureChunks(FATAL_WITHOUT_ABORT, "base-test",
                                           "source.cpp", 42, "\nsecond");
  ASSERT_EQ(1U, leading_empty.size());
  EXPECT_EQ("source.cpp:42] \nsource.cpp:42] second",
            leading_empty[0].message);
}

TEST(BaseLogging, PrintfMacrosRestoreErrno) {
  SetDefaultTag("base-test");

  errno = EACCES;
  LOGI("formatted value=%d", 7);
  EXPECT_EQ(EACCES, errno);

  errno = ENOENT;
  PLOGE("errno-aware message");
  EXPECT_EQ(ENOENT, errno);

  // LOGF intentionally logs at fatal priority without terminating.
  errno = EIO;
  LOGF("non-terminating fatal-priority message");
  EXPECT_EQ(EIO, errno);

  SetDefaultTag("");
}

}  // namespace
}  // namespace ndksamples::base
