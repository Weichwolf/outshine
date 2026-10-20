#include "SparseMatrix.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <vector>

namespace {
template <class Index> void CheckOperators() {
  using namespace outshine;
  using namespace outshine::Test;
  std::vector<SparseEntry<Index>> entries{
      {2, 3, 7}, {0, 0, 2}, {1, 2, 5}, {0, 0, -1}, {1, 3, 3}, {1, 3, -3}};
  const auto matrix = SparseMatrix<Index>::Assemble(3, 5, entries);
  CHECK(matrix.ColumnOffsets == std::vector<Index>({0, 1, 1, 2, 4, 4}),
        "empty columns and duplicate coordinates preserve exact CSC offsets");
  CHECK(matrix.RowIndices == std::vector<Index>({0, 1, 1, 2}) &&
            matrix.Values == std::vector<double>({1, 5, 0, 7}),
        "duplicates sum without dropping structural zero entries");
  const std::array x{2., 3., 4., 5., 6.};
  CHECK(matrix.Multiply(x) == std::vector<double>({2, 20, 35}),
        "rectangular sparse products preserve empty columns");
  const std::array y{2., 3., 4.};
  CHECK(matrix.TransposeMultiply(y) == std::vector<double>({2, 0, 15, 28, 0}),
        "transpose products keep the independent row and column dimensions");
  entries = {{2, 0, 3}, {0, 0, 1}, {0, 2, 3}, {2, 2, 4}};
  const auto square = SparseMatrix<Index>::Assemble(3, 3, entries);
  const auto upper = square.UpperTriangle();
  CHECK(upper.ColumnOffsets == std::vector<Index>({0, 1, 1, 3}) &&
            upper.RowIndices == std::vector<Index>({0, 0, 2}) &&
            upper.Values == std::vector<double>({1, 3, 4}),
        "the solver upper triangle keeps its original coefficients and empty column");
  entries.clear();
  const auto empty = SparseMatrix<Index>::Assemble(2, 3, entries);
  CHECK(empty.Multiply(y) == std::vector<double>({0, 0}) &&
            empty.ColumnOffsets == std::vector<Index>({0, 0, 0, 0}),
        "an empty sparse operator remains dimensionally valid");
}
}

int main() {
  CheckOperators<int32_t>();
  CheckOperators<int64_t>();
  return outshine::Test::Report();
}
