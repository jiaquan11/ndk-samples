// SPDX-License-Identifier: Apache-2.0

#define LOG_TAG "BASE-BENCH"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "logging_internal.h"
#include "logging_splitters.h"

namespace {

using ndksamples::base::DEBUG;
using ndksamples::base::FATAL;
using ndksamples::base::INFO;
using ndksamples::base::LogSeverity;

constexpr char kLogdTag[] = "BASE-BENCH-LOGD";
constexpr std::uint64_t kMaximumIterations = 100'000'000;

struct Options {
  int rounds = 20;
  int warmup_rounds = 3;
  int target_milliseconds = 20;
  int logd_rounds = 5;
  int logd_iterations_per_thread = 50;
  bool include_logd = false;
  bool capture_ready_marker_only = false;
  bool capture_marker_only = false;
  std::string commit = "unknown";
  std::string run_id = "manual";
};

struct BatchTiming {
  std::uint64_t wall_nanoseconds;
  std::uint64_t cpu_nanoseconds;
  bool cpu_available;
};

struct Statistics {
  double median;
  double p95;
  double mad;
};

struct SplitSummary {
  std::size_t chunks = 0;
  std::size_t output_bytes = 0;
  std::uint64_t checksum = 0;
  bool within_logd_limit = true;
  bool valid_utf8_chunks = true;
  bool valid_source_prefix = true;
  std::string reconstructed_without_newlines;
};

std::atomic<int> g_tag_evaluations{0};
std::atomic<int> g_argument_evaluations{0};

const char* CountingTag() {
  g_tag_evaluations.fetch_add(1, std::memory_order_relaxed);
  return "SHOULD-NOT-BE-EVALUATED";
}

int CountingArgument() {
  g_argument_evaluations.fetch_add(1, std::memory_order_relaxed);
  return 42;
}

template <typename T>
inline void DoNotOptimize(const T& value) {
#if defined(__clang__) || defined(__GNUC__)
  asm volatile("" : : "g"(value) : "memory");
#else
  volatile const T* sink = &value;
  (void)sink;
#endif
}

inline void ClobberMemory() {
#if defined(__clang__) || defined(__GNUC__)
  asm volatile("" : : : "memory");
#endif
}

std::optional<std::uint64_t> ProcessCpuTimeNanoseconds() {
  timespec value{};
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value) != 0) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(value.tv_sec) * 1'000'000'000ULL +
         static_cast<std::uint64_t>(value.tv_nsec);
}

template <typename Operation>
BatchTiming RunBatch(std::uint64_t iterations, Operation&& operation) {
  const std::optional<std::uint64_t> cpu_start = ProcessCpuTimeNanoseconds();
  const auto wall_start = std::chrono::steady_clock::now();
#if defined(__clang__)
#pragma clang loop unroll(disable)
#endif
  for (std::uint64_t iteration = 0; iteration < iterations; ++iteration) {
    operation();
  }
  ClobberMemory();
  const auto wall_end = std::chrono::steady_clock::now();
  const std::optional<std::uint64_t> cpu_end = ProcessCpuTimeNanoseconds();
  const auto wall = std::chrono::duration_cast<std::chrono::nanoseconds>(
      wall_end - wall_start);
  const bool cpu_available =
      cpu_start.has_value() && cpu_end.has_value() && *cpu_end >= *cpu_start;
  return {static_cast<std::uint64_t>(wall.count()),
          cpu_available ? *cpu_end - *cpu_start : 0, cpu_available};
}

template <typename Operation>
std::uint64_t CalibrateIterations(const Options& options,
                                  Operation&& operation) {
  const std::uint64_t target =
      static_cast<std::uint64_t>(options.target_milliseconds) * 1'000'000ULL;
  std::uint64_t iterations = 1;
  for (int attempt = 0; attempt < 12; ++attempt) {
    const std::uint64_t elapsed =
        RunBatch(iterations, operation).wall_nanoseconds;
    if (elapsed >= target || iterations == kMaximumIterations) {
      break;
    }

    if (elapsed == 0) {
      iterations = std::min(kMaximumIterations, iterations * 10);
      continue;
    }

    const long double ratio =
        static_cast<long double>(target) / static_cast<long double>(elapsed);
    const long double bounded_ratio = std::clamp(ratio, 1.25L, 10.0L);
    const auto next = static_cast<std::uint64_t>(
        std::ceil(static_cast<long double>(iterations) * bounded_ratio));
    iterations = std::min(kMaximumIterations, std::max(iterations + 1, next));
  }
  return iterations;
}

double MedianOfSorted(const std::vector<double>& sorted) {
  if (sorted.empty()) {
    return 0.0;
  }
  const std::size_t middle = sorted.size() / 2;
  if ((sorted.size() & 1U) != 0U) {
    return sorted[middle];
  }
  return (sorted[middle - 1] + sorted[middle]) / 2.0;
}

Statistics CalculateStatistics(std::vector<double> samples) {
  std::sort(samples.begin(), samples.end());
  const double median = MedianOfSorted(samples);
  const std::size_t p95_index =
      static_cast<std::size_t>(
          std::ceil(0.95 * static_cast<double>(samples.size()))) -
      1;

  std::vector<double> deviations;
  deviations.reserve(samples.size());
  for (const double sample : samples) {
    deviations.push_back(std::abs(sample - median));
  }
  std::sort(deviations.begin(), deviations.end());
  return {median, samples[p95_index], MedianOfSorted(deviations)};
}

void PrintResult(std::string_view name, std::size_t payload_bytes, int threads,
                 std::uint64_t iterations, int rounds, const Statistics& wall,
                 double median_cpu_nanoseconds, double chunks_per_operation) {
  const double bytes_per_second =
      wall.median > 0.0
          ? static_cast<double>(payload_bytes) * 1'000'000'000.0 / wall.median
          : 0.0;
  std::cout << std::fixed << std::setprecision(2) << "RESULT," << name << ','
            << payload_bytes << ',' << threads << ',' << iterations << ','
            << rounds << ',' << wall.median << ',' << wall.p95 << ','
            << wall.mad << ',' << median_cpu_nanoseconds << ','
            << bytes_per_second << ',' << chunks_per_operation << '\n';
}

template <typename Operation>
void RunAdaptiveCase(const Options& options, std::string_view name,
                     std::size_t payload_bytes, double chunks_per_operation,
                     Operation&& operation) {
  const std::uint64_t iterations = CalibrateIterations(options, operation);
  for (int round = 0; round < options.warmup_rounds; ++round) {
    RunBatch(iterations, operation);
  }

  std::vector<double> wall_samples;
  std::vector<double> cpu_samples;
  wall_samples.reserve(static_cast<std::size_t>(options.rounds));
  cpu_samples.reserve(static_cast<std::size_t>(options.rounds));
  for (int round = 0; round < options.rounds; ++round) {
    const BatchTiming timing = RunBatch(iterations, operation);
    wall_samples.push_back(static_cast<double>(timing.wall_nanoseconds) /
                           static_cast<double>(iterations));
    if (timing.cpu_available) {
      cpu_samples.push_back(static_cast<double>(timing.cpu_nanoseconds) /
                            static_cast<double>(iterations));
    }
  }

  const Statistics wall = CalculateStatistics(std::move(wall_samples));
  std::sort(cpu_samples.begin(), cpu_samples.end());
  const double median_cpu =
      cpu_samples.empty() ? -1.0 : MedianOfSorted(cpu_samples);
  PrintResult(name, payload_bytes, 1, iterations, options.rounds, wall,
              median_cpu, chunks_per_operation);
}

#if defined(__clang__) || defined(__GNUC__)
#define BENCHMARK_NOINLINE __attribute__((noinline))
#else
#define BENCHMARK_NOINLINE
#endif

// 两个对照入口都禁止跨调用点内联，避免只让同一翻译单元内的旧算法获得
// 额外优化机会；该约束只属于性能基准，不影响日志库生产实现。
BENCHMARK_NOINLINE std::string FormatCurrent(const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::string result = ndksamples::base::internal::StringVPrintf(format, args);
  va_end(args);
  return result;
}

// 复刻日志重构前的 vasprintf 路径，只作为同一进程内的算法对照组。
BENCHMARK_NOINLINE std::string FormatWithVasprintf(const char* format, ...) {
  if (format == nullptr) {
    return "<null log format>";
  }

  va_list args;
  va_start(args, format);
  va_list format_args;
  va_copy(format_args, args);
  char* buffer = nullptr;
  const int written = vasprintf(&buffer, format, format_args);
  va_end(format_args);
  va_end(args);
  if (written < 0 || buffer == nullptr) {
    free(buffer);
    return "<log formatting failed>";
  }

  std::string result(buffer, static_cast<std::size_t>(written));
  free(buffer);
  return result;
}

#undef BENCHMARK_NOINLINE

std::string MakeAsciiPayload(std::size_t size) {
  constexpr std::string_view pattern =
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  std::string result;
  result.reserve(size);
  while (result.size() < size) {
    const std::size_t remaining = size - result.size();
    result.append(pattern.substr(0, std::min(remaining, pattern.size())));
  }
  return result;
}

std::string MakeUtf8Payload(std::size_t approximate_size) {
  constexpr std::string_view pattern = u8"日志性能基准";
  std::string result;
  result.reserve(approximate_size);
  while (result.size() + pattern.size() <= approximate_size) {
    result.append(pattern);
  }
  result.append(approximate_size - result.size(), 'x');
  return result;
}

std::string MakeMultilinePayload(std::size_t approximate_size) {
  const std::string line = MakeAsciiPayload(120);
  std::string result;
  result.reserve(approximate_size);
  while (result.size() + line.size() + 1 <= approximate_size) {
    result.append(line);
    result.push_back('\n');
  }
  result.append(approximate_size - result.size(), 'x');
  return result;
}

bool StartsWith(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() &&
         text.substr(0, prefix.size()) == prefix;
}

bool IsValidUtf8(std::string_view text) {
  std::size_t position = 0;
  while (position < text.size()) {
    const auto lead = static_cast<unsigned char>(text[position]);
    if (lead <= 0x7fU) {
      ++position;
      continue;
    }

    std::size_t trailing = 0;
    std::uint32_t code_point = 0;
    std::uint32_t minimum = 0;
    if (lead >= 0xc2U && lead <= 0xdfU) {
      trailing = 1;
      code_point = lead & 0x1fU;
      minimum = 0x80U;
    } else if (lead >= 0xe0U && lead <= 0xefU) {
      trailing = 2;
      code_point = lead & 0x0fU;
      minimum = 0x800U;
    } else if (lead >= 0xf0U && lead <= 0xf4U) {
      trailing = 3;
      code_point = lead & 0x07U;
      minimum = 0x10000U;
    } else {
      return false;
    }

    if (position + trailing >= text.size()) {
      return false;
    }
    for (std::size_t offset = 1; offset <= trailing; ++offset) {
      const auto continuation =
          static_cast<unsigned char>(text[position + offset]);
      if ((continuation & 0xc0U) != 0x80U) {
        return false;
      }
      code_point = (code_point << 6U) | (continuation & 0x3fU);
    }
    if (code_point < minimum || code_point > 0x10ffffU ||
        (code_point >= 0xd800U && code_point <= 0xdfffU)) {
      return false;
    }
    position += trailing + 1;
  }
  return true;
}

void VerifyFormatterResult(const std::string& actual,
                           const std::string& expected,
                           std::string_view case_name) {
  if (actual != expected) {
    std::cerr << "格式化预检失败: " << case_name << '\n';
    std::exit(3);
  }
}

void RunFilteredCase(const Options& options) {
  const LogSeverity previous = ndksamples::base::GetMinimumLogSeverity();
  ndksamples::base::SetMinimumLogSeverity(FATAL);
  g_tag_evaluations.store(0, std::memory_order_relaxed);
  g_argument_evaluations.store(0, std::memory_order_relaxed);
  LOGD_TAG(CountingTag(), "value=%d", CountingArgument());
  if (g_tag_evaluations.load(std::memory_order_relaxed) != 0 ||
      g_argument_evaluations.load(std::memory_order_relaxed) != 0) {
    std::cerr << "过滤预检失败: Tag 或正文参数被求值\n";
    std::exit(3);
  }

  auto operation = []() { LOGD_TAG("FILTERED", "value=%d", 42); };
  RunAdaptiveCase(options, "filter.debug_below_fatal", 0, 0.0, operation);
  ndksamples::base::SetMinimumLogSeverity(previous);
}

void RunStringFormatterPair(const Options& options, std::size_t size,
                            bool current_first) {
  const std::string payload = MakeAsciiPayload(size);
  VerifyFormatterResult(FormatCurrent("%s", payload.c_str()), payload,
                        "current.string");
  VerifyFormatterResult(FormatWithVasprintf("%s", payload.c_str()), payload,
                        "vasprintf.string");

  const std::string current_name =
      "format.current.string." + std::to_string(size);
  auto current = [&]() {
    const std::string result = FormatCurrent("%s", payload.c_str());
    DoNotOptimize(result.data());
    DoNotOptimize(result.size());
  };
  const std::string legacy_name =
      "format.vasprintf.string." + std::to_string(size);
  auto legacy = [&]() {
    const std::string result = FormatWithVasprintf("%s", payload.c_str());
    DoNotOptimize(result.data());
    DoNotOptimize(result.size());
  };
  // 相邻长度交替采用 AB/BA 顺序，降低固定运行次序带来的温度和频率偏差。
  if (current_first) {
    RunAdaptiveCase(options, current_name, size, 0.0, current);
    RunAdaptiveCase(options, legacy_name, size, 0.0, legacy);
  } else {
    RunAdaptiveCase(options, legacy_name, size, 0.0, legacy);
    RunAdaptiveCase(options, current_name, size, 0.0, current);
  }
}

void RunFormatterCases(const Options& options) {
  constexpr std::size_t sizes[] = {16, 128, 511, 512, 1024, 4096, 16384};
  for (std::size_t index = 0; index < std::size(sizes); ++index) {
    RunStringFormatterPair(options, sizes[index], (index & 1U) == 0U);
  }

  const std::string integer_expected =
      FormatCurrent("value=%d hex=%08x", 123456789, 123456789U);
  VerifyFormatterResult(
      FormatWithVasprintf("value=%d hex=%08x", 123456789, 123456789U),
      integer_expected, "integer");
  auto integer_current = []() {
    const std::string result =
        FormatCurrent("value=%d hex=%08x", 123456789, 123456789U);
    DoNotOptimize(result.data());
    DoNotOptimize(result.size());
  };
  auto integer_legacy = []() {
    const std::string result =
        FormatWithVasprintf("value=%d hex=%08x", 123456789, 123456789U);
    DoNotOptimize(result.data());
    DoNotOptimize(result.size());
  };
  RunAdaptiveCase(options, "format.current.integer", integer_expected.size(),
                  0.0, integer_current);
  RunAdaptiveCase(options, "format.vasprintf.integer", integer_expected.size(),
                  0.0, integer_legacy);

  const std::string mixed_payload = MakeAsciiPayload(112);
  const std::string mixed_expected =
      FormatCurrent("name=%s value=%d", mixed_payload.c_str(), 42);
  VerifyFormatterResult(
      FormatWithVasprintf("name=%s value=%d", mixed_payload.c_str(), 42),
      mixed_expected, "mixed");
  auto mixed_current = [&]() {
    const std::string result =
        FormatCurrent("name=%s value=%d", mixed_payload.c_str(), 42);
    DoNotOptimize(result.data());
    DoNotOptimize(result.size());
  };
  auto mixed_legacy = [&]() {
    const std::string result =
        FormatWithVasprintf("name=%s value=%d", mixed_payload.c_str(), 42);
    DoNotOptimize(result.data());
    DoNotOptimize(result.size());
  };
  // 混合格式反转顺序，与整数格式组成另一组 AB/BA 对照。
  RunAdaptiveCase(options, "format.vasprintf.mixed", mixed_expected.size(), 0.0,
                  mixed_legacy);
  RunAdaptiveCase(options, "format.current.mixed", mixed_expected.size(), 0.0,
                  mixed_current);
}

SplitSummary InspectSplit(std::string_view message) {
  constexpr std::string_view tag = "BASE-BENCH";
  constexpr std::string_view source_prefix =
      "[base_logging_benchmark.cpp:321] ";
  const std::size_t maximum_chunk_size =
      ndksamples::base::kLoggerEntryMaxPayload - tag.size() -
      ndksamples::base::kLoggerEntryReservedBytes;
  const std::string owned_message(message);
  SplitSummary summary;
  ndksamples::base::SplitByLogdChunks(
      INFO, tag.data(), "base_logging_benchmark.cpp", 321,
      owned_message.c_str(), [&](LogSeverity, const char*, const char* chunk) {
        const std::size_t size = std::strlen(chunk);
        ++summary.chunks;
        summary.output_bytes += size;
        summary.within_logd_limit &= size <= maximum_chunk_size;
        summary.valid_utf8_chunks &= IsValidUtf8(std::string_view(chunk, size));
        if (size != 0) {
          summary.checksum += static_cast<unsigned char>(chunk[0]);
          summary.checksum += static_cast<unsigned char>(chunk[size - 1]);
        }

        std::string_view remaining(chunk, size);
        // 原始换行若恰好触发提交，会表现为两个 logd 条目的边界；超长单行
        // 分片也使用条目边界，单看回调无法区分二者。因此预检重建全部非换行
        // 字节及顺序，换行位置的完整语义继续由 base_logging_test 覆盖。
        while (!remaining.empty()) {
          if (!StartsWith(remaining, source_prefix)) {
            summary.valid_source_prefix = false;
            break;
          }
          remaining.remove_prefix(source_prefix.size());
          const std::size_t newline = remaining.find('\n');
          if (newline == std::string_view::npos) {
            summary.reconstructed_without_newlines.append(remaining);
            break;
          }
          summary.reconstructed_without_newlines.append(
              remaining.substr(0, newline));
          remaining.remove_prefix(newline + 1);
        }
      });
  return summary;
}

void RunSplitterCase(const Options& options, std::string_view name,
                     const std::string& payload) {
  const SplitSummary summary = InspectSplit(payload);
  std::string expected_without_newlines = payload;
  expected_without_newlines.erase(
      std::remove(expected_without_newlines.begin(),
                  expected_without_newlines.end(), '\n'),
      expected_without_newlines.end());
  if (summary.chunks == 0 || !summary.within_logd_limit ||
      !summary.valid_utf8_chunks || !summary.valid_source_prefix ||
      summary.output_bytes == 0 ||
      summary.reconstructed_without_newlines != expected_without_newlines) {
    std::cerr << "分片预检失败: " << name << '\n';
    std::exit(3);
  }

  std::uint64_t observed = 0;
  auto operation = [&]() {
    ndksamples::base::SplitByLogdChunks(
        INFO, "BASE-BENCH", "base_logging_benchmark.cpp", 321, payload.c_str(),
        [&](LogSeverity, const char*, const char* chunk) {
          observed += static_cast<unsigned char>(chunk[0]) + 1U;
          DoNotOptimize(chunk);
          ClobberMemory();
        });
  };
  RunAdaptiveCase(options, name, payload.size(),
                  static_cast<double>(summary.chunks), operation);
  DoNotOptimize(observed);
}

void RunSplitterCases(const Options& options) {
  constexpr std::size_t sizes[] = {64, 512, 4000, 4096, 8192, 16384};
  for (const std::size_t size : sizes) {
    RunSplitterCase(options, "split.ascii." + std::to_string(size),
                    MakeAsciiPayload(size));
  }
  RunSplitterCase(options, "split.multiline.8192", MakeMultilinePayload(8192));
  RunSplitterCase(options, "split.utf8.8192", MakeUtf8Payload(8192));
}

std::uint64_t RunLogdCases(const Options& options) {
  const LogSeverity previous = ndksamples::base::GetMinimumLogSeverity();
  ndksamples::base::SetMinimumLogSeverity(DEBUG);
  std::uint64_t expected_calls = 0;
  constexpr std::size_t sizes[] = {64, 256, 1024, 4000};
  constexpr int thread_counts[] = {1, 2, 4};

  for (const std::size_t size : sizes) {
    const std::string payload = MakeAsciiPayload(size);
    for (const int thread_count : thread_counts) {
      std::vector<double> wall_samples;
      std::vector<double> cpu_samples;
      wall_samples.reserve(static_cast<std::size_t>(options.logd_rounds));
      cpu_samples.reserve(static_cast<std::size_t>(options.logd_rounds));
      const std::uint64_t calls_per_round =
          static_cast<std::uint64_t>(thread_count) *
          static_cast<std::uint64_t>(options.logd_iterations_per_thread);

      for (int round = 0; round < options.logd_rounds; ++round) {
        std::atomic<int> ready{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(thread_count));
        for (int thread_index = 0; thread_index < thread_count;
             ++thread_index) {
          workers.emplace_back([&, round, thread_index]() {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
              std::this_thread::yield();
            }
            for (int iteration = 0;
                 iteration < options.logd_iterations_per_thread; ++iteration) {
              const unsigned long long sequence =
                  static_cast<unsigned long long>(round) * calls_per_round +
                  static_cast<unsigned long long>(thread_index) *
                      static_cast<unsigned long long>(
                          options.logd_iterations_per_thread) +
                  static_cast<unsigned long long>(iteration);
              LOGI_TAG(kLogdTag,
                       "run=%s bytes=%zu workers=%d thread=%d seq=%llu "
                       "payload=%s",
                       options.run_id.c_str(), size, thread_count, thread_index,
                       sequence, payload.c_str());
            }
          });
        }

        while (ready.load(std::memory_order_acquire) != thread_count) {
          std::this_thread::yield();
        }
        const std::optional<std::uint64_t> cpu_start =
            ProcessCpuTimeNanoseconds();
        const auto wall_start = std::chrono::steady_clock::now();
        start.store(true, std::memory_order_release);
        for (std::thread& worker : workers) {
          worker.join();
        }
        const auto wall_end = std::chrono::steady_clock::now();
        const std::optional<std::uint64_t> cpu_end =
            ProcessCpuTimeNanoseconds();
        const auto wall = std::chrono::duration_cast<std::chrono::nanoseconds>(
            wall_end - wall_start);
        wall_samples.push_back(static_cast<double>(wall.count()) /
                               static_cast<double>(calls_per_round));
        if (cpu_start.has_value() && cpu_end.has_value() &&
            *cpu_end >= *cpu_start) {
          cpu_samples.push_back(static_cast<double>(*cpu_end - *cpu_start) /
                                static_cast<double>(calls_per_round));
        }
      }

      const Statistics wall = CalculateStatistics(std::move(wall_samples));
      std::sort(cpu_samples.begin(), cpu_samples.end());
      const double median_cpu =
          cpu_samples.empty() ? -1.0 : MedianOfSorted(cpu_samples);
      const std::string name = "logd.write_burst." + std::to_string(size) +
                               ".t" + std::to_string(thread_count);
      PrintResult(name, size, thread_count, calls_per_round,
                  options.logd_rounds, wall, median_cpu, 0.0);
      expected_calls +=
          calls_per_round * static_cast<std::uint64_t>(options.logd_rounds);
    }
  }

  ndksamples::base::SetMinimumLogSeverity(previous);
  return expected_calls;
}

bool ParsePositiveInt(std::string_view text, int* value) {
  if (text.empty()) {
    return false;
  }
  char* end = nullptr;
  const std::string owned(text);
  const long parsed = std::strtol(owned.c_str(), &end, 10);
  if (end == nullptr || *end != '\0' || parsed <= 0 ||
      parsed > std::numeric_limits<int>::max()) {
    return false;
  }
  *value = static_cast<int>(parsed);
  return true;
}

void PrintUsage(const char* program) {
  std::cout << "用法: " << program
            << " [--quick] [--logd] [--rounds=N] [--warmup=N]"
               " [--target-ms=N] [--commit=HASH] [--run-id=ID]"
               " [--capture-ready-marker-only] [--capture-marker-only]\n";
}

enum class ParseResult { kOk, kHelp, kError };

ParseResult ParseArguments(int argc, char** argv, Options* options) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--help") {
      return ParseResult::kHelp;
    }
    if (argument == "--quick") {
      options->rounds = 7;
      options->warmup_rounds = 1;
      options->target_milliseconds = 5;
      options->logd_rounds = 3;
      options->logd_iterations_per_thread = 10;
      continue;
    }
    if (argument == "--logd") {
      options->include_logd = true;
      continue;
    }
    if (argument == "--capture-marker-only") {
      options->capture_marker_only = true;
      continue;
    }
    if (argument == "--capture-ready-marker-only") {
      options->capture_ready_marker_only = true;
      continue;
    }

    const auto parse_number = [&](std::string_view prefix, int* target) {
      return StartsWith(argument, prefix) &&
             ParsePositiveInt(argument.substr(prefix.size()), target);
    };
    if (parse_number("--rounds=", &options->rounds) ||
        parse_number("--warmup=", &options->warmup_rounds) ||
        parse_number("--target-ms=", &options->target_milliseconds)) {
      continue;
    }
    if (StartsWith(argument, "--commit=")) {
      options->commit = std::string(argument.substr(9));
      continue;
    }
    if (StartsWith(argument, "--run-id=")) {
      options->run_id = std::string(argument.substr(9));
      continue;
    }

    std::cerr << "未知或无效参数: " << argument << '\n';
    return ParseResult::kError;
  }
  return ParseResult::kOk;
}

const char* CurrentAbi() {
#if defined(__aarch64__)
  return "arm64-v8a";
#elif defined(__arm__)
  return "armeabi-v7a";
#elif defined(__x86_64__)
  return "x86_64";
#elif defined(__i386__)
  return "x86";
#else
  return "unknown";
#endif
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  const ParseResult parse_result = ParseArguments(argc, argv, &options);
  if (parse_result == ParseResult::kHelp) {
    PrintUsage(argv[0]);
    return 0;
  }
  if (parse_result == ParseResult::kError) {
    PrintUsage(argv[0]);
    return 2;
  }

  if (options.capture_ready_marker_only) {
    LOGI_TAG(kLogdTag, "run=%s capture_ready=1", options.run_id.c_str());
    return 0;
  }
  if (options.capture_marker_only) {
    LOGI_TAG(kLogdTag, "run=%s capture_complete=1", options.run_id.c_str());
    return 0;
  }

  std::cout << "META,schema_version,1\n"
            << "META,commit," << options.commit << '\n'
            << "META,run_id," << options.run_id << '\n'
            << "META,abi," << CurrentAbi() << '\n'
            << "META,build_mode,optimized_NDEBUG\n"
            << "META,clock,steady_clock_and_process_cpu\n"
            << "META,warmup_rounds," << options.warmup_rounds << '\n'
            << "META,measurement_rounds," << options.rounds << '\n'
            << "META,target_milliseconds," << options.target_milliseconds
            << '\n'
            << "RESULT,name,payload_bytes,threads,iterations,rounds,"
               "median_wall_ns_per_op,p95_wall_ns_per_op,mad_wall_ns_per_op,"
               "median_process_cpu_ns_per_op,effective_payload_bytes_per_"
               "second,chunks_per_op\n";

  RunFilteredCase(options);
  RunFormatterCases(options);
  RunSplitterCases(options);
  if (options.include_logd) {
    const std::uint64_t expected_calls = RunLogdCases(options);
    std::cout << "META,logd_expected_calls," << expected_calls << '\n';
  }
  return 0;
}
