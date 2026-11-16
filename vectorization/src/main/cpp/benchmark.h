// Copyright (C) 2024 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <expected>
#include <optional>

namespace samples::vectorization {

/**
 * The available backends for matrix multiplication shown in this sample.
 */
enum class Backend : uint8_t {
  /// Auto-vectorization only.
  kAutoVectorization = 0,

  /// C++ std::simd.
  kCxxSimd = 1,

  /// Clang's arch-generic vector types.
  kClangVector = 2,

  /// Clang's built-in matrix type.
  kClangMatrix = 3,

  /// OpenMP SIMD.
  kOpenMp = 4,
};

/// Errors returned by BenchmarkMatrixMultiplication.
enum class BenchmarkError : int8_t {
  /// Indicates that the requested backend has not yet been implemented.
  kNotImplemented = -1,
  /// Indicates that the requested backend isn't supported for the device.
  kNotSupported = -2,
  /// Indicates that an unknown backend was requested.
  kUnknownBackend = -3,
};

/**
 * Benchmarks the given matrix multiply backend.
 *
 * The chosen backend will run a predetermined number of times and return the
 * average execution time.
 *
 * @param backend The backend to benchmark.
 * @return The average execution time, or an error code.
 */
[[nodiscard]] std::expected<std::chrono::nanoseconds, BenchmarkError>
BenchmarkMatrixMultiplication(Backend backend);

}  // namespace samples::vectorization
