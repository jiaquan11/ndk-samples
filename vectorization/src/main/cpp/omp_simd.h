// Copyright (C) 2024 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

#include "matrix.h"

namespace samples::vectorization {

/**
 * Multiplies two compatible matrices and returns the result.
 *
 * @tparam T The type of each matrix cell.
 * @tparam M The number of rows in the left operand and the result.
 * @tparam N The number of columns in the left operand, and the rows in the
 * right operand.
 * @tparam P The number of columns in the right operand and the result.
 * @param lhs The left operand.
 * @param rhs The right operand.
 * @return The result of lhs * rhs.
 */
template <typename T, size_t M, size_t N, size_t P>
Matrix<M, P, T> MultiplyWithOpenMP(const Matrix<M, N, T>& lhs,
                                   const Matrix<N, P, T>& rhs) {
  Matrix<M, P, T> result;
#pragma omp simd
  for (auto result_column_index = 0U; result_column_index < P;
       result_column_index++) {
    for (auto lhs_column_index = 0U; lhs_column_index < N; lhs_column_index++) {
      auto lhs_column = lhs.column(lhs_column_index);
      const T& scalar = rhs[lhs_column_index, result_column_index];
      for (auto row = 0U; row < lhs_column.size(); row++) {
        result[row, result_column_index] += lhs_column[row] * scalar;
      }
    }
  }
  return result;
}

}  // namespace samples::vectorization
