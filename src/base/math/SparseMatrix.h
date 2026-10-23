#ifndef OUTSHINE_BASE_MATH_SPARSEMATRIX_H
#define OUTSHINE_BASE_MATH_SPARSEMATRIX_H

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace outshine {
template <std::signed_integral Index> struct SparseEntry {
  Index Row, Column;
  double Value;
};

template <std::signed_integral Index> struct SparseMatrix {
  Index Rows = 0, Columns = 0;
  std::vector<Index> ColumnOffsets, RowIndices;
  std::vector<double> Values;

  [[nodiscard]] static SparseMatrix
  Assemble(Index rows, Index columns, std::span<SparseEntry<Index>> entries) {
    assert(rows >= 0 && columns >= 0);
    std::ranges::sort(entries, [](const auto &a, const auto &b) {
      return a.Column != b.Column ? a.Column < b.Column : a.Row < b.Row;
    });
    size_t count = 0;
    for (size_t at = 0; at < entries.size(); ++at) {
      const auto &entry = entries[at];
      assert(entry.Row >= 0 && entry.Row < rows && entry.Column >= 0 && entry.Column < columns);
      if (at == 0 || entry.Row != entries[at - 1].Row || entry.Column != entries[at - 1].Column) {
        ++count;
      }
    }
    assert(count <= static_cast<size_t>(std::numeric_limits<Index>::max()));
    SparseMatrix result{
        .Rows = rows, .Columns = columns, .ColumnOffsets = {}, .RowIndices = {}, .Values = {}};
    result.ColumnOffsets.assign(static_cast<size_t>(columns) + 1, 0);
    result.RowIndices.reserve(count);
    result.Values.reserve(count);
    for (size_t at = 0; at < entries.size(); ++at) {
      const auto &entry = entries[at];
      if (at != 0 && entry.Row == entries[at - 1].Row && entry.Column == entries[at - 1].Column) {
        result.Values.back() += entry.Value;
      } else {
        ++result.ColumnOffsets[static_cast<size_t>(entry.Column) + 1];
        result.RowIndices.push_back(entry.Row);
        result.Values.push_back(entry.Value);
      }
    }
    for (size_t at = 1; at < result.ColumnOffsets.size(); ++at) {
      result.ColumnOffsets[at] += result.ColumnOffsets[at - 1];
    }
    return result;
  }

  [[nodiscard]] std::vector<double> Multiply(std::span<const double> vector) const {
    assert(vector.size() == static_cast<size_t>(Columns));
    std::vector<double> result(static_cast<size_t>(Rows), 0);
    for (size_t column = 0; column < static_cast<size_t>(Columns); ++column) {
      for (Index at = ColumnOffsets[column]; at < ColumnOffsets[column + 1]; ++at) {
        const auto index = static_cast<size_t>(at);
        result[static_cast<size_t>(RowIndices[index])] += Values[index] * vector[column];
      }
    }
    return result;
  }

  [[nodiscard]] SparseMatrix Multiply(const SparseMatrix &right) const {
    assert(Rows >= 0 && Columns >= 0 && Columns == right.Rows && right.Columns >= 0);
    SparseMatrix result{.Rows = Rows,
                        .Columns = right.Columns,
                        .ColumnOffsets = {0},
                        .RowIndices = {},
                        .Values = {}};
    std::vector<double> sums(static_cast<size_t>(Rows));
    std::vector<Index> marks(sums.size(), -1);
    std::vector<Index> touched;
    for (Index column = 0; column < right.Columns; ++column) {
      touched.clear();
      const auto atColumn = static_cast<size_t>(column);
      for (Index item = right.ColumnOffsets[atColumn]; item < right.ColumnOffsets[atColumn + 1];
           ++item) {
        const auto entry = static_cast<size_t>(item);
        const auto source = static_cast<size_t>(right.RowIndices[entry]);
        for (Index at = ColumnOffsets[source]; at < ColumnOffsets[source + 1]; ++at) {
          const auto index = static_cast<size_t>(at);
          const Index row = RowIndices[index];
          const auto target = static_cast<size_t>(row);
          const double value = Values[index] * right.Values[entry];
          if (marks[target] != column) {
            marks[target] = column;
            sums[target] = value;
            touched.push_back(row);
          } else {
            sums[target] += value;
          }
        }
      }
      std::ranges::sort(touched);
      for (const Index row : touched) {
        const double value = sums[static_cast<size_t>(row)];
        if (value != 0) {
          result.RowIndices.push_back(row);
          result.Values.push_back(value);
        }
      }
      assert(result.Values.size() <= static_cast<size_t>(std::numeric_limits<Index>::max()));
      result.ColumnOffsets.push_back(static_cast<Index>(result.Values.size()));
    }
    return result;
  }

  [[nodiscard]] std::vector<double> TransposeMultiply(std::span<const double> vector) const {
    assert(vector.size() == static_cast<size_t>(Rows));
    std::vector<double> result(static_cast<size_t>(Columns), 0);
    for (size_t column = 0; column < static_cast<size_t>(Columns); ++column) {
      for (Index at = ColumnOffsets[column]; at < ColumnOffsets[column + 1]; ++at) {
        const auto index = static_cast<size_t>(at);
        result[column] += Values[index] * vector[static_cast<size_t>(RowIndices[index])];
      }
    }
    return result;
  }

  [[nodiscard]] SparseMatrix UpperTriangle() const {
    assert(Rows == Columns);
    size_t count = 0;
    for (Index column = 0; column < Columns; ++column) {
      for (Index at = ColumnOffsets[static_cast<size_t>(column)];
           at < ColumnOffsets[static_cast<size_t>(column) + 1];
           ++at) {
        if (RowIndices[static_cast<size_t>(at)] > column) { break; }
        ++count;
      }
    }
    SparseMatrix result{
        .Rows = Rows, .Columns = Columns, .ColumnOffsets = {}, .RowIndices = {}, .Values = {}};
    result.ColumnOffsets.reserve(ColumnOffsets.size());
    result.RowIndices.reserve(count);
    result.Values.reserve(count);
    result.ColumnOffsets.push_back(0);
    for (Index column = 0; column < Columns; ++column) {
      for (Index at = ColumnOffsets[static_cast<size_t>(column)];
           at < ColumnOffsets[static_cast<size_t>(column) + 1];
           ++at) {
        const auto index = static_cast<size_t>(at);
        if (RowIndices[index] > column) { break; }
        result.RowIndices.push_back(RowIndices[index]);
        result.Values.push_back(Values[index]);
      }
      result.ColumnOffsets.push_back(static_cast<Index>(result.Values.size()));
    }
    return result;
  }

  [[nodiscard]] size_t StorageBytes() const {
    return (ColumnOffsets.capacity() + RowIndices.capacity()) * sizeof(Index) +
           Values.capacity() * sizeof(double);
  }
};
}
#endif
