#define LOG_TAG "base-test"
#include <base/logging.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "logging_splitters.h"

namespace ndksamples::base {
namespace {

class ScopedMinimumLogSeverity {
 public:
  explicit ScopedMinimumLogSeverity(LogSeverity severity)
      : previous_(GetMinimumLogSeverity()) {
    SetMinimumLogSeverity(severity);
  }

  ~ScopedMinimumLogSeverity() { SetMinimumLogSeverity(previous_); }

  ScopedMinimumLogSeverity(const ScopedMinimumLogSeverity&) = delete;
  ScopedMinimumLogSeverity& operator=(const ScopedMinimumLogSeverity&) =
      delete;

 private:
  const LogSeverity previous_;
};

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
  const std::size_t max_size =
      kLoggerEntryMaxPayload - std::strlen(kTag) - kLoggerEntryReservedBytes;
  const std::string line(max_size / 2 + 1, 'x');

  const auto chunks =
      CaptureChunks(INFO, kTag, nullptr, 0, line + "\n" + line + "\n" + line);

  ASSERT_EQ(3U, chunks.size());
  EXPECT_EQ(line, chunks[0].message);
  EXPECT_EQ(line, chunks[1].message);
  EXPECT_EQ(line, chunks[2].message);
}

TEST(BaseLoggingSplitter, SplitsOversizedLineWithoutDataLoss) {
  constexpr char kTag[] = "base-test";
  const std::size_t max_size =
      kLoggerEntryMaxPayload - std::strlen(kTag) - kLoggerEntryReservedBytes;
  const std::string oversized(max_size + 100, 'x');

  const auto chunks =
      CaptureChunks(INFO, kTag, nullptr, 0, oversized + "\ntail");

  ASSERT_EQ(2U, chunks.size());
  EXPECT_EQ(max_size, chunks[0].message.size());
  EXPECT_EQ(std::string(max_size, 'x'), chunks[0].message);
  EXPECT_EQ(std::string(100, 'x') + "\ntail", chunks[1].message);
  EXPECT_EQ(oversized + "\ntail", chunks[0].message + chunks[1].message);
}

TEST(BaseLoggingSplitter, SplitsUnbrokenOversizedMessageWithoutDataLoss) {
  constexpr char kTag[] = "base-test";
  const std::size_t max_size =
      kLoggerEntryMaxPayload - std::strlen(kTag) - kLoggerEntryReservedBytes;
  const std::string oversized(max_size + 100, 'x');

  const auto chunks = CaptureChunks(INFO, kTag, nullptr, 0, oversized);

  ASSERT_EQ(2U, chunks.size());
  EXPECT_EQ(max_size, chunks[0].message.size());
  EXPECT_EQ(100U, chunks[1].message.size());
  EXPECT_EQ(oversized, chunks[0].message + chunks[1].message);
}

TEST(BaseLoggingSplitter,
     RepeatsSourcePrefixWhileSplittingOversizedMessage) {
  constexpr char kTag[] = "base-test";
  constexpr char kSourcePrefix[] = "[source.cpp:42] ";
  const std::size_t max_size =
      kLoggerEntryMaxPayload - std::strlen(kTag) - kLoggerEntryReservedBytes;
  const std::string oversized(max_size + 100, 'x');

  const auto chunks =
      CaptureChunks(INFO, kTag, "source.cpp", 42, oversized);

  ASSERT_EQ(2U, chunks.size());
  std::string reconstructed;
  for (const auto& chunk : chunks) {
    EXPECT_LE(chunk.message.size(), max_size);
    EXPECT_EQ(0U, chunk.message.find(kSourcePrefix));
    reconstructed.append(chunk.message.substr(std::strlen(kSourcePrefix)));
  }
  EXPECT_EQ(oversized, reconstructed);
}

TEST(BaseLoggingSplitter, HandlesExactPayloadBoundaryBeforeNewline) {
  constexpr char kTag[] = "base-test";
  const std::size_t max_size =
      kLoggerEntryMaxPayload - std::strlen(kTag) - kLoggerEntryReservedBytes;
  const std::string exact(max_size, 'x');

  const auto chunks = CaptureChunks(INFO, kTag, nullptr, 0, exact + "\ntail");

  ASSERT_EQ(2U, chunks.size());
  EXPECT_EQ(exact, chunks[0].message);
  EXPECT_EQ("tail", chunks[1].message);
}

TEST(BaseLoggingSplitter, DoesNotSplitValidUtf8CodePoint) {
  constexpr char kTag[] = "base-test";
  const std::size_t max_size =
      kLoggerEntryMaxPayload - std::strlen(kTag) - kLoggerEntryReservedBytes;
  std::string message(max_size - 1, 'x');
  constexpr char kUtf8Character[] = "\xe4\xb8\xad";
  message.append(kUtf8Character);

  const auto chunks = CaptureChunks(INFO, kTag, nullptr, 0, message);

  ASSERT_EQ(2U, chunks.size());
  EXPECT_EQ(max_size - 1, chunks[0].message.size());
  EXPECT_EQ(kUtf8Character, chunks[1].message);
  EXPECT_EQ(message, chunks[0].message + chunks[1].message);
}

TEST(BaseLoggingSplitter, FallsBackInsteadOfAbortingForOversizedTag) {
  const std::string oversized_tag(kLoggerEntryMaxPayload + 100, 't');

  const auto chunks =
      CaptureChunks(INFO, oversized_tag.c_str(), nullptr, 0, "message");

  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ(kLoggerFallbackTag, chunks[0].tag);
  EXPECT_EQ("message", chunks[0].message);
}

TEST(BaseLoggingSplitter, CompactsOversizedSourcePrefix) {
  const std::string oversized_file(kLoggerEntryMaxPayload + 100, 'f');

  const auto chunks = CaptureChunks(INFO, "base-test", oversized_file.c_str(),
                                    42, "message");

  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ("[...:42] message", chunks[0].message);
}

TEST(BaseLoggingSplitter, AddsReadableSourcePrefix) {
  const auto chunks =
      CaptureChunks(INFO, "base-test", "source.cpp", 42, "message");

  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ("[source.cpp:42] message", chunks[0].message);
}

TEST(BaseLoggingSplitter, RepeatsSourcePrefixForMultilineMessage) {
  const auto chunks =
      CaptureChunks(INFO, "base-test", "source.cpp", 42, "first\nsecond");

  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ("[source.cpp:42] first\n[source.cpp:42] second",
            chunks[0].message);

  const auto leading_empty =
      CaptureChunks(INFO, "base-test", "source.cpp", 42, "\nsecond");
  ASSERT_EQ(1U, leading_empty.size());
  EXPECT_EQ("[source.cpp:42] \n[source.cpp:42] second",
            leading_empty[0].message);
}

TEST(BaseLogging, PrintfMacrosRestoreErrno) {
  errno = EACCES;
  LOGI("formatted value=%d", 7);
  EXPECT_EQ(EACCES, errno);

  int tag_evaluations = 0;
  errno = EBUSY;
  LOGI_TAG((++tag_evaluations, std::string("base-test-override").c_str()),
           "explicit tag value=%d", 8);
  EXPECT_EQ(1, tag_evaluations);
  EXPECT_EQ(EBUSY, errno);

  errno = EIO;
  LOGF("non-terminating fatal-priority message");
  EXPECT_EQ(EIO, errno);

  const std::string long_value(700, 'x');
  LOGI("long formatted value=%s", long_value.c_str());
  EXPECT_EQ(EIO, errno);
}

TEST(BaseLogging, FiltersBeforeEvaluatingTagAndMessageArguments) {
  ScopedMinimumLogSeverity minimum_severity(INFO);
  int tag_evaluations = 0;
  int message_evaluations = 0;

  errno = EACCES;
  LOGD_TAG((++tag_evaluations, "filtered-tag"), "value=%d",
           ++message_evaluations);
  EXPECT_EQ(0, tag_evaluations);
  EXPECT_EQ(0, message_evaluations);
  EXPECT_EQ(EACCES, errno);

  LOGI_TAG((++tag_evaluations, "allowed-tag"), "value=%d",
           ++message_evaluations);
  EXPECT_EQ(1, tag_evaluations);
  EXPECT_EQ(1, message_evaluations);
  EXPECT_TRUE(IsLoggable(INFO));
  EXPECT_FALSE(IsLoggable(DEBUG));
  EXPECT_TRUE(IsLoggable(FATAL));

  {
    ScopedMinimumLogSeverity fatal_only(FATAL);
    int error_tag_evaluations = 0;
    int error_message_evaluations = 0;
    int fatal_tag_evaluations = 0;
    int fatal_message_evaluations = 0;
    LOGE_TAG((++error_tag_evaluations, "filtered-error-tag"), "value=%d",
             ++error_message_evaluations);
    LOGF_TAG((++fatal_tag_evaluations, "allowed-fatal-tag"), "value=%d",
             ++fatal_message_evaluations);
    EXPECT_EQ(0, error_tag_evaluations);
    EXPECT_EQ(0, error_message_evaluations);
    EXPECT_EQ(1, fatal_tag_evaluations);
    EXPECT_EQ(1, fatal_message_evaluations);
    EXPECT_FALSE(IsLoggable(ERROR));
    EXPECT_TRUE(IsLoggable(FATAL));
  }
}

}  // namespace
}  // namespace ndksamples::base
