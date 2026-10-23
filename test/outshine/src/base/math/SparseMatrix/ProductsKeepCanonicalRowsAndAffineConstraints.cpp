#include "Check.h"
#include "SparseMatrix.h"
#include <array>
#include <cstdint>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

template <class Index> void RectangularProduct() {
  std::array left{SparseEntry<Index>{.Row = 0, .Column = 0, .Value = 1},
                  SparseEntry<Index>{.Row = 0, .Column = 1, .Value = 2},
                  SparseEntry<Index>{.Row = 1, .Column = 1, .Value = 3},
                  SparseEntry<Index>{.Row = 1, .Column = 2, .Value = 4}};
  std::array right{SparseEntry<Index>{.Row = 0, .Column = 0, .Value = 5},
                   SparseEntry<Index>{.Row = 1, .Column = 0, .Value = 6},
                   SparseEntry<Index>{.Row = 1, .Column = 1, .Value = 7},
                   SparseEntry<Index>{.Row = 2, .Column = 1, .Value = 8}};
  const auto a = SparseMatrix<Index>::Assemble(2, 3, left);
  const auto b = SparseMatrix<Index>::Assemble(3, 2, right);
  const auto product = a.Multiply(b);
  CHECK(product.Rows == 2 && product.Columns == 2 &&
            product.ColumnOffsets == std::vector<Index>({0, 2, 4}) &&
            product.RowIndices == std::vector<Index>({0, 1, 0, 1}) &&
            product.Values == std::vector<double>({17, 18, 14, 53}),
        "rectangular products accumulate repeated row contributions in canonical CSC order");
  const std::array values{2., 3.};
  CHECK(product.Multiply(values) == a.Multiply(b.Multiply(values)),
        "the matrix product has the same action as the two separate linear maps");
}

void AffineHeightProjection() {
  constexpr double kStartWeight = .25;
  constexpr double kEndWeight = 1 - kStartWeight;
  std::array constraints{SparseEntry<int>{.Row = 0, .Column = 0, .Value = 1},
                         SparseEntry<int>{.Row = 0, .Column = 1, .Value = -1},
                         SparseEntry<int>{.Row = 1, .Column = 1, .Value = 1},
                         SparseEntry<int>{.Row = 1, .Column = 3, .Value = -1},
                         SparseEntry<int>{.Row = 2, .Column = 2, .Value = 1},
                         SparseEntry<int>{.Row = 2, .Column = 3, .Value = -1}};
  std::array projection{SparseEntry<int>{.Row = 0, .Column = 0, .Value = kStartWeight},
                        SparseEntry<int>{.Row = 0, .Column = 1, .Value = kEndWeight},
                        SparseEntry<int>{.Row = 1, .Column = 0, .Value = 1},
                        SparseEntry<int>{.Row = 2, .Column = 1, .Value = 1},
                        SparseEntry<int>{.Row = 3, .Column = 1, .Value = 1}};
  const auto a = SparseMatrix<int>::Assemble(3, 4, constraints);
  const auto p = SparseMatrix<int>::Assemble(4, 2, projection);
  const auto reduced = a.Multiply(p);
  CHECK(reduced.ColumnOffsets == std::vector<int>({0, 2, 4}) &&
            reduced.RowIndices == std::vector<int>({0, 1, 0, 1}) &&
            reduced.Values == std::vector<double>({-kEndWeight, 1, kEndWeight, -1}),
        "derived heights substitute into every constraint while exact zero rows disappear");
  const std::array endpoints{4., 8.};
  CHECK(reduced.Multiply(endpoints) == a.Multiply(p.Multiply(endpoints)),
        "affine source heights preserve the original complete constraint values");
}

void EmptyShapes() {
  std::array<SparseEntry<int>, 0> entries{};
  const auto left = SparseMatrix<int>::Assemble(0, 3, entries);
  const auto right = SparseMatrix<int>::Assemble(3, 2, entries);
  const auto product = left.Multiply(right);
  CHECK(product.Rows == 0 && product.Columns == 2 &&
            product.ColumnOffsets == std::vector<int>({0, 0, 0}) && product.Values.empty(),
        "empty row spaces retain all empty output columns");
  const auto zero = right.Multiply(SparseMatrix<int>::Assemble(2, 0, entries));
  CHECK(zero.Rows == 3 && zero.Columns == 0 && zero.ColumnOffsets == std::vector<int>({0}),
        "an empty right column space has one terminal CSC offset");
}
}

int main() {
  RectangularProduct<int32_t>();
  RectangularProduct<int64_t>();
  AffineHeightProjection();
  EmptyShapes();
  return Report();
}
