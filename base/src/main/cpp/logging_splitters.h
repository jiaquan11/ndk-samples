// Copyright (C) 2020 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "base/logging.h"

namespace ndksamples::base {

inline constexpr std::size_t kLoggerEntryMaxPayload = 4068;
inline constexpr std::size_t kLoggerEntryReservedBytes = 35;
inline constexpr std::size_t kLoggerMinimumTextPayload = 64;
inline constexpr std::size_t kLoggerSourcePrefixBufferSize = 256;
inline constexpr char kLoggerFallbackTag[] = "ndk-samples";

static_assert(kLoggerEntryMaxPayload >
              kLoggerEntryReservedBytes + kLoggerMinimumTextPayload);
static_assert(kLoggerMinimumTextPayload >= 4);

// 对合法 UTF-8 返回不超过 max_bytes 的码点边界，避免把一个码点拆到两片；
// 非法序列无法找到边界时仍按 max_bytes 前进，保证不死循环且不丢原始字节。
inline std::size_t FindUtf8PrefixLength(std::string_view text,
                                        std::size_t max_bytes) {
  if (text.size() <= max_bytes) {
    return text.size();
  }

  // cut 表示前一片包含的字节数，因此 text[cut] 是下一片准备使用的首字节。
  std::size_t cut = max_bytes;
  while (cut > 0 &&
         (static_cast<unsigned char>(text[cut]) & 0xc0U) == 0x80U) {
    --cut;
  }

  // 非法 UTF-8 也必须保证分片继续前进，同时不能丢失原始字节。
  return cut == 0 ? max_bytes : cut;
}

// 按逻辑行和 logd 负载上限组织正文，再把每个以 \0 结尾的片段同步交给回调。
// 回调中的 message 指向函数内复用的栈缓冲区，必须在返回前消费或复制，不能保存
// 该指针供异步任务稍后使用。
template <typename F>
void SplitByLogdChunks(LogSeverity severity, const char* tag, const char* file,
                       unsigned int line, const char* msg,
                       const F& log_function) {
  const std::size_t maximum_tag_size =
      kLoggerEntryMaxPayload - kLoggerEntryReservedBytes -
      kLoggerMinimumTextPayload;
  const char* effective_tag = tag == nullptr ? kLoggerFallbackTag : tag;
  std::size_t tag_size = std::strlen(effective_tag);
  // 异常超长 Tag 不应终止进程，也不能把正文预算压缩成不可用的小片段。
  if (tag_size > maximum_tag_size) {
    effective_tag = kLoggerFallbackTag;
    tag_size = std::strlen(effective_tag);
  }

  // 为优先级、Tag、字符串终止符和 liblog 内部开销预留空间。
  const std::size_t max_size =
      kLoggerEntryMaxPayload - tag_size - kLoggerEntryReservedBytes;

  std::array<char, kLoggerSourcePrefixBufferSize> source_prefix_buffer;
  std::string_view source_prefix;
  if (file != nullptr) {
    // 常规来源前缀在固定栈缓冲区内生成；异常文件名过长时仍保留行号。
    int prefix_size = std::snprintf(source_prefix_buffer.data(),
                                    source_prefix_buffer.size(), "[%s:%u] ",
                                    file, line);
    if (prefix_size < 0 ||
        static_cast<std::size_t>(prefix_size) >=
            source_prefix_buffer.size() ||
        static_cast<std::size_t>(prefix_size) + 4 > max_size) {
      prefix_size =
          std::snprintf(source_prefix_buffer.data(),
                        source_prefix_buffer.size(), "[...:%u] ", line);
    }
    if (prefix_size >= 0 &&
        static_cast<std::size_t>(prefix_size) < source_prefix_buffer.size() &&
        static_cast<std::size_t>(prefix_size) + 4 <= max_size) {
      source_prefix = std::string_view(
          source_prefix_buffer.data(), static_cast<std::size_t>(prefix_size));
    }
  }

  std::array<char, kLoggerEntryMaxPayload + 1> logd_chunk;
  std::size_t chunk_position = 0;
  bool chunk_has_line = false;

  auto call_log_function = [&]() {
    logd_chunk[chunk_position] = '\0';
    log_function(severity, effective_tag, logd_chunk.data());
    chunk_position = 0;
    chunk_has_line = false;
  };

  auto append_bytes = [&](std::string_view bytes) {
    if (bytes.empty()) {
      return;
    }
    std::memcpy(logd_chunk.data() + chunk_position, bytes.data(), bytes.size());
    chunk_position += bytes.size();
    logd_chunk[chunk_position] = '\0';
  };

  auto write_logical_line = [&](std::string_view logical_line) {
    const std::size_t separator_size = chunk_has_line ? 1 : 0;
    const std::size_t fixed_size = separator_size + source_prefix.size();
    const std::size_t available = max_size - chunk_position;
    if (fixed_size <= available &&
        logical_line.size() <= available - fixed_size) {
      if (chunk_has_line) {
        append_bytes("\n");
      }
      append_bytes(source_prefix);
      append_bytes(logical_line);
      chunk_has_line = true;
      return;
    }

    if (chunk_has_line) {
      call_log_function();
    }

    const std::size_t fragment_capacity = max_size - source_prefix.size();
    // 单个逻辑行超过一条 logd 负载时直接分片，不额外插入换行；最后一个片段
    // 暂不提交，使下一逻辑行在空间允许时可以复用同一条日志。
    while (logical_line.size() > fragment_capacity) {
      const std::size_t fragment_size =
          FindUtf8PrefixLength(logical_line, fragment_capacity);
      append_bytes(source_prefix);
      append_bytes(logical_line.substr(0, fragment_size));
      chunk_has_line = true;
      call_log_function();
      logical_line.remove_prefix(fragment_size);
    }

    append_bytes(source_prefix);
    append_bytes(logical_line);
    chunk_has_line = true;
  };

  std::string_view remaining =
      msg == nullptr ? std::string_view("<null log message>")
                     : std::string_view(msg);
  // 只把 \n 作为逻辑行分隔符；CRLF 中的 \r 属于上一行正文并原样保留。
  while (true) {
    const std::size_t newline = remaining.find('\n');
    if (newline == std::string_view::npos) {
      write_logical_line(remaining);
      break;
    }

    write_logical_line(remaining.substr(0, newline));
    remaining.remove_prefix(newline + 1);
  }

  if (chunk_has_line) {
    call_log_function();
  }
}

}  // namespace ndksamples::base
