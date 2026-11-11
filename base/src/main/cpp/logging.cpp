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

#include "base/logging.h"

#include <android/log.h>
#include <android/set_abort_message.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include "logging_splitters.h"

namespace ndksamples::base {
namespace {

std::string StringVPrintf(const char* format, va_list args) {
  if (format == nullptr) {
    return "<null log format>";
  }

  char* buffer = nullptr;
  va_list format_args;
  va_copy(format_args, args);
  const int written = vasprintf(&buffer, format, format_args);
  va_end(format_args);
  if (written < 0 || buffer == nullptr) {
    free(buffer);
    return "<log formatting failed>";
  }

  std::string result(buffer, static_cast<size_t>(written));
  free(buffer);
  return result;
}

const char* GetFileBasename(const char* file) {
  if (file == nullptr) {
    return nullptr;
  }
  const char* last_slash = strrchr(file, '/');
  return last_slash == nullptr ? file : last_slash + 1;
}

int32_t LogSeverityToPriority(LogSeverity severity) {
  switch (severity) {
    case VERBOSE:
      return ANDROID_LOG_VERBOSE;
    case DEBUG:
      return ANDROID_LOG_DEBUG;
    case INFO:
      return ANDROID_LOG_INFO;
    case WARNING:
      return ANDROID_LOG_WARN;
    case ERROR:
      return ANDROID_LOG_ERROR;
    case FATAL_WITHOUT_ABORT:
    case FATAL:
    default:
      return ANDROID_LOG_FATAL;
  }
}

struct DefaultTagState {
  std::mutex mutex;
  std::string tag;
};

DefaultTagState& GetDefaultTagState() {
  // Logging can be called by other translation units during static teardown.
  // Keep this small state alive for the process lifetime to avoid destructor
  // ordering hazards in that phase.
  static auto& state = *new DefaultTagState;
  return state;
}

std::string ResolveDefaultTag() {
  auto& state = GetDefaultTagState();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (!state.tag.empty()) {
    return state.tag;
  }

  const char* process_name = getprogname();
  return process_name == nullptr ? "ndk-samples" : process_name;
}

void LogdLogChunk(LogSeverity severity, const char* tag, const char* message) {
  __android_log_buf_print(LOG_ID_MAIN, LogSeverityToPriority(severity), tag,
                          "%s", message);
}

void EmitLog(LogSeverity severity, const char* tag, const char* file,
             unsigned int line, std::string message) {
  if (severity == FATAL) {
    // Set the complete message before logd receives individual chunks.
    android_set_abort_message(message.c_str());
  }

  const char* basename = GetFileBasename(file);
  if (tag != nullptr) {
    SplitByLogdChunks(severity, tag, basename, line, message.c_str(),
                      LogdLogChunk);
  } else {
    const std::string default_tag = ResolveDefaultTag();
    SplitByLogdChunks(severity, default_tag.c_str(), basename, line,
                      message.c_str(), LogdLogChunk);
  }

  if (severity == FATAL) {
    abort();
  }
}

}  // namespace

void SetDefaultTag(const std::string_view tag) {
  auto& state = GetDefaultTagState();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.tag.assign(tag);
}

void LogPrint(LogSeverity severity, const char* tag, const char* file,
              unsigned int line, const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::string message = StringVPrintf(format, args);
  va_end(args);
  EmitLog(severity, tag, file, line, std::move(message));
}

void LogPrintErrno(LogSeverity severity, const char* tag, const char* file,
                   unsigned int line, int error, const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::string message = StringVPrintf(format, args);
  va_end(args);
  message.append(": ");
  message.append(strerror(error));
  EmitLog(severity, tag, file, line, std::move(message));
}

[[noreturn]] void CheckFailed(const char* condition, const char* tag,
                              const char* file, unsigned int line,
                              const char* format, ...) {
  va_list args;
  va_start(args, format);
  const std::string detail = StringVPrintf(format, args);
  va_end(args);

  std::string message = "Check failed: ";
  message.append(condition == nullptr ? "<null condition>" : condition);
  message.append(": ");
  message.append(detail);
  EmitLog(FATAL, tag, file, line, std::move(message));

  // EmitLog(FATAL, ...) terminates. Preserve the noreturn contract if that
  // implementation is ever changed incorrectly.
  abort();
}

}  // namespace ndksamples::base
