/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

/**
 * @file logging.h
 * @brief Project-wide printf-style Android logging and assertions.
 *
 * Define LOG_TAG before including this header to use a fixed tag for the
 * translation unit. Without LOG_TAG, logging uses the tag configured by
 * SetDefaultTag(), or the process name when no custom tag has been set.
 *
 * LOGF writes at Android's fatal priority without terminating. CHECKF writes a
 * fatal assertion message and terminates the process.
 *
 * The public logging interface requires C++17 or newer.
 */

#include <cerrno>
#include <string_view>

// Note: DO NOT USE DIRECTLY. Use LOG_TAG instead.
#ifdef _LOG_TAG_INTERNAL
#error "_LOG_TAG_INTERNAL must not be defined"
#endif
#ifdef LOG_TAG
#define _LOG_TAG_INTERNAL LOG_TAG
#else
#define _LOG_TAG_INTERNAL nullptr
#endif

namespace ndksamples::base {

enum LogSeverity {
  VERBOSE,
  DEBUG,
  INFO,
  WARNING,
  ERROR,
  FATAL_WITHOUT_ABORT,
  FATAL,
};

// An empty tag clears the custom value and restores the process-name fallback.
void SetDefaultTag(std::string_view tag);

#if defined(__clang__) || defined(__GNUC__)
#define NDKSAMPLES_PRINTF_FORMAT(format_index, first_argument) \
  __attribute__((format(printf, format_index, first_argument)))
#else
#define NDKSAMPLES_PRINTF_FORMAT(format_index, first_argument)
#endif

// Callers should normally use the macros below so file, line and LOG_TAG are
// captured automatically.
void LogPrint(LogSeverity severity, const char* tag, const char* file,
              unsigned int line, const char* format, ...)
    NDKSAMPLES_PRINTF_FORMAT(5, 6);
void LogPrintErrno(LogSeverity severity, const char* tag, const char* file,
                   unsigned int line, int error, const char* format, ...)
    NDKSAMPLES_PRINTF_FORMAT(6, 7);

// Use CHECKF rather than calling this function directly.
[[noreturn]] void CheckFailed(const char* condition, const char* tag,
                              const char* file, unsigned int line,
                              const char* format, ...)
    NDKSAMPLES_PRINTF_FORMAT(5, 6);

#undef NDKSAMPLES_PRINTF_FORMAT

namespace detail {

// Logging must not unexpectedly change errno in the caller. PLOG macros also
// capture this saved value and append its text to the message.
class ErrnoRestorer {
 public:
  ErrnoRestorer() : saved_errno_(errno) {}
  ~ErrnoRestorer() { errno = saved_errno_; }

  ErrnoRestorer(const ErrnoRestorer&) = delete;
  ErrnoRestorer& operator=(const ErrnoRestorer&) = delete;

  int saved_errno() const { return saved_errno_; }

 private:
  const int saved_errno_;
};

}  // namespace detail

}  // namespace ndksamples::base

#define NDKSAMPLES_LOG_PRINT(severity_value, tag_value, ...)              \
  do {                                                                    \
    [[maybe_unused]] ::ndksamples::base::detail::ErrnoRestorer            \
        ndksamples_errno_restorer_;                                       \
    const auto ndksamples_log_severity_ = (severity_value);               \
    const char* const ndksamples_log_tag_ = (tag_value);                  \
    ::ndksamples::base::LogPrint(ndksamples_log_severity_,                \
                                 ndksamples_log_tag_, __FILE__, __LINE__, \
                                 __VA_ARGS__);                            \
  } while (false)

#define NDKSAMPLES_PLOG_PRINT(severity_value, tag_value, ...)              \
  do {                                                                     \
    [[maybe_unused]] ::ndksamples::base::detail::ErrnoRestorer             \
        ndksamples_errno_restorer_;                                        \
    const auto ndksamples_log_severity_ = (severity_value);                \
    const char* const ndksamples_log_tag_ = (tag_value);                   \
    ::ndksamples::base::LogPrintErrno(                                     \
        ndksamples_log_severity_, ndksamples_log_tag_, __FILE__, __LINE__, \
        ndksamples_errno_restorer_.saved_errno(), __VA_ARGS__);            \
  } while (false)

#define LOGV(...)                                                      \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::VERBOSE, _LOG_TAG_INTERNAL, \
                       __VA_ARGS__)
#define LOGD(...)                                                    \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::DEBUG, _LOG_TAG_INTERNAL, \
                       __VA_ARGS__)
#define LOGI(...) \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::INFO, _LOG_TAG_INTERNAL, __VA_ARGS__)
#define LOGW(...)                                                      \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::WARNING, _LOG_TAG_INTERNAL, \
                       __VA_ARGS__)
#define LOGE(...)                                                    \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::ERROR, _LOG_TAG_INTERNAL, \
                       __VA_ARGS__)
#define LOGF(...)                                               \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::FATAL_WITHOUT_ABORT, \
                       _LOG_TAG_INTERNAL, __VA_ARGS__)

#define LOGV_TAG(tag, ...) \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::VERBOSE, tag, __VA_ARGS__)
#define LOGD_TAG(tag, ...) \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::DEBUG, tag, __VA_ARGS__)
#define LOGI_TAG(tag, ...) \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::INFO, tag, __VA_ARGS__)
#define LOGW_TAG(tag, ...) \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::WARNING, tag, __VA_ARGS__)
#define LOGE_TAG(tag, ...) \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::ERROR, tag, __VA_ARGS__)
#define LOGF_TAG(tag, ...)                                           \
  NDKSAMPLES_LOG_PRINT(::ndksamples::base::FATAL_WITHOUT_ABORT, tag, \
                       __VA_ARGS__)

#define PLOGV(...)                                                      \
  NDKSAMPLES_PLOG_PRINT(::ndksamples::base::VERBOSE, _LOG_TAG_INTERNAL, \
                        __VA_ARGS__)
#define PLOGD(...)                                                    \
  NDKSAMPLES_PLOG_PRINT(::ndksamples::base::DEBUG, _LOG_TAG_INTERNAL, \
                        __VA_ARGS__)
#define PLOGI(...)                                                   \
  NDKSAMPLES_PLOG_PRINT(::ndksamples::base::INFO, _LOG_TAG_INTERNAL, \
                        __VA_ARGS__)
#define PLOGW(...)                                                      \
  NDKSAMPLES_PLOG_PRINT(::ndksamples::base::WARNING, _LOG_TAG_INTERNAL, \
                        __VA_ARGS__)
#define PLOGE(...)                                                    \
  NDKSAMPLES_PLOG_PRINT(::ndksamples::base::ERROR, _LOG_TAG_INTERNAL, \
                        __VA_ARGS__)
#define PLOGF(...)                                               \
  NDKSAMPLES_PLOG_PRINT(::ndksamples::base::FATAL_WITHOUT_ABORT, \
                        _LOG_TAG_INTERNAL, __VA_ARGS__)

#define CHECKF(condition, ...)                                                 \
  do {                                                                         \
    if (!(condition)) {                                                        \
      ::ndksamples::base::CheckFailed(#condition, _LOG_TAG_INTERNAL, __FILE__, \
                                      __LINE__, __VA_ARGS__);                  \
    }                                                                          \
  } while (false)
