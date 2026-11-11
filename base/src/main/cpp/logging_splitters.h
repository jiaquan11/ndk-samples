/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "base/logging.h"

namespace ndksamples::base {

inline constexpr std::size_t kLoggerEntryMaxPayload = 4068;

// Splits newline-delimited messages into payloads accepted by logd. To retain
// the existing AOSP-compatible behavior, an oversized line followed by a
// newline is truncated in this helper. A non-fatal message with no newline is
// passed directly to liblog, which applies the final payload limit.
template <typename F>
void SplitByLogdChunks(LogSeverity severity, const char* tag, const char* file,
                       unsigned int line, const char* msg,
                       const F& log_function) {
  // Subtract priority, the tag and both null terminators, plus 32 bytes of
  // slack as used by android.util.Log.
  const std::ptrdiff_t max_size =
      static_cast<std::ptrdiff_t>(kLoggerEntryMaxPayload) -
      static_cast<std::ptrdiff_t>(std::strlen(tag)) - 35;
  if (max_size <= 0) {
    std::abort();
  }

  const bool add_file =
      file != nullptr && (severity == FATAL || severity == FATAL_WITHOUT_ABORT);
  const std::string file_header =
      add_file ? std::string(file) + ":" + std::to_string(line) + "] " : "";
  const std::ptrdiff_t file_header_size =
      static_cast<std::ptrdiff_t>(file_header.size());

  // The backing storage is fixed-size standard C++; the active capacity still
  // uses max_size so space remains reserved for the current log tag.
  std::array<char, kLoggerEntryMaxPayload + 1> logd_chunk;
  std::ptrdiff_t chunk_position = 0;
  bool chunk_has_line = false;

  auto call_log_function = [&]() {
    log_function(severity, tag, logd_chunk.data());
    chunk_position = 0;
    chunk_has_line = false;
  };

  auto write_to_logd_chunk = [&](const char* message, int length) {
    const std::size_t capacity =
        static_cast<std::size_t>(max_size - chunk_position + 1);
    const char* new_line = chunk_has_line ? "\n" : "";
    const int size_written =
        add_file ? std::snprintf(logd_chunk.data() + chunk_position, capacity,
                                 "%s%s%.*s", new_line, file_header.c_str(),
                                 length, message)
                 : std::snprintf(logd_chunk.data() + chunk_position, capacity,
                                 "%s%.*s", new_line, length, message);

    if (size_written < 0) {
      return;
    }

    // snprintf returns the size it wanted to write. Clamp to the bytes that
    // actually fit so subsequent capacity calculations stay in range.
    const std::ptrdiff_t available = max_size - chunk_position;
    chunk_position += size_written > available ? available : size_written;
    // An empty logical line writes no payload bytes, so track it separately
    // from chunk_position to preserve leading and repeated empty lines.
    chunk_has_line = true;
  };

  const char* newline = std::strchr(msg, '\n');
  while (newline != nullptr) {
    if (chunk_has_line &&
        chunk_position + (newline - msg) + 1 + file_header_size > max_size) {
      call_log_function();
    }

    write_to_logd_chunk(msg, static_cast<int>(newline - msg));
    msg = newline + 1;
    newline = std::strchr(msg, '\n');
  }

  if (chunk_has_line &&
      chunk_position + static_cast<std::ptrdiff_t>(std::strlen(msg)) + 1 +
              file_header_size <=
          max_size) {
    write_to_logd_chunk(msg, -1);
    call_log_function();
  } else {
    if (chunk_has_line) {
      call_log_function();
    }

    if (add_file) {
      std::snprintf(logd_chunk.data(), static_cast<std::size_t>(max_size + 1),
                    "%s%s", file_header.c_str(), msg);
      log_function(severity, tag, logd_chunk.data());
    } else {
      log_function(severity, tag, msg);
    }
  }
}

}  // namespace ndksamples::base
