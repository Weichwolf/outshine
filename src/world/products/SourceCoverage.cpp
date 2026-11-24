#include "SourceCoverage.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <numeric>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
namespace {
struct ProductRange {
  uint64_t Begin, End;
  size_t Product;
};

std::expected<std::vector<ProductRange>, SourceCoverageError>
RangesOf(const SourceCoverage &domain, std::span<const SourceCoverage *const> cut) {
  std::vector<ProductRange> result;
  for (size_t i = 0; i < cut.size(); ++i) {
    const auto *sources = cut[i];
    if (sources == nullptr) { return std::unexpected(SourceCoverageError::InvalidInput); }
    if (sources->Revision() != domain.Revision() ||
        sources->SourceCount() != domain.SourceCount()) {
      return std::unexpected(SourceCoverageError::ForeignDomain);
    }
    if (!domain.Contains(*sources)) {
      return std::unexpected(SourceCoverageError::DifferentSources);
    }
    for (const auto range : sources->Ranges()) {
      result.push_back({.Begin = range.First, .End = range.First + range.Count, .Product = i});
    }
  }
  std::ranges::sort(result, {}, &ProductRange::Begin);
  if (std::ranges::adjacent_find(result, [](const ProductRange &a, const ProductRange &b) {
        return a.End > b.Begin;
      }) != result.end()) {
    return std::unexpected(SourceCoverageError::Overlap);
  }
  return result;
}

class ConnectedProducts {
public:
  explicit ConnectedProducts(size_t count) : Parents_(count), Sizes_(count, 1) {
    std::ranges::iota(Parents_, size_t{0});
  }

  size_t Root(size_t product) {
    while (Parents_[product] != product) {
      Parents_[product] = Parents_[Parents_[product]];
      product = Parents_[product];
    }
    return product;
  }

  void Join(size_t a, size_t b) {
    a = Root(a);
    b = Root(b);
    if (a == b) { return; }
    if (Sizes_[a] < Sizes_[b]) { std::swap(a, b); }
    Parents_[b] = a;
    Sizes_[a] += Sizes_[b];
  }

private:
  std::vector<size_t> Parents_, Sizes_;
};
}

SourceCoverage::SourceCoverage(std::string revision,
                               uint64_t sourceCount,
                               std::vector<Range> ranges)
    : Revision_(std::move(revision)), SourceCount_(sourceCount), Ranges_(std::move(ranges)) {}

std::expected<SourceCoverage, SourceCoverageError> SourceCoverage::FromRanges(
    std::string revision, uint64_t sourceCount, std::span<const Range> ranges) {
  if (revision.empty()) { return std::unexpected(SourceCoverageError::InvalidInput); }
  std::vector<Range> canonical;
  canonical.reserve(ranges.size());
  for (const auto range : ranges) {
    if (range.Count == 0 || range.First >= sourceCount || range.Count > sourceCount - range.First) {
      return std::unexpected(SourceCoverageError::InvalidInput);
    }
    if (!canonical.empty()) {
      auto &last = canonical.back();
      const uint64_t end = last.First + last.Count;
      if (range.First < end) { return std::unexpected(SourceCoverageError::Overlap); }
      if (range.First == end) {
        last.Count += range.Count;
        continue;
      }
    }
    canonical.push_back(range);
  }
  return SourceCoverage(std::move(revision), sourceCount, std::move(canonical));
}

std::expected<SourceCoverage, SourceCoverageError> SourceCoverage::FromSources(
    std::string revision, uint64_t sourceCount, std::span<const uint32_t> sources) {
  std::vector<Range> ranges;
  for (const uint32_t source : sources) {
    if (source >= sourceCount) { return std::unexpected(SourceCoverageError::InvalidInput); }
    if (!ranges.empty()) {
      auto &last = ranges.back();
      const uint64_t end = last.First + last.Count;
      if (source < end) { return std::unexpected(SourceCoverageError::Overlap); }
      if (source == end) {
        ++last.Count;
        continue;
      }
    }
    ranges.push_back({.First = source, .Count = 1});
  }
  return FromRanges(std::move(revision), sourceCount, ranges);
}

bool SourceCoverage::Contains(const SourceCoverage &other) const noexcept {
  if (Revision_ != other.Revision_ || SourceCount_ != other.SourceCount_) { return false; }
  auto parent = Ranges_.begin();
  for (const auto child : other.Ranges_) {
    while (parent != Ranges_.end() && parent->First + parent->Count <= child.First) { ++parent; }
    if (parent == Ranges_.end() || parent->First > child.First ||
        parent->First + parent->Count < child.First + child.Count) {
      return false;
    }
  }
  return true;
}

std::expected<void, SourceCoverageError>
SourceCoverage::Partition(const SourceCoverage &expected,
                          std::span<const SourceCoverage *const> products) {
  std::vector<Range> ranges;
  for (const auto *product : products) {
    if (product == nullptr) { return std::unexpected(SourceCoverageError::InvalidInput); }
    if (product->Revision_ != expected.Revision_ ||
        product->SourceCount_ != expected.SourceCount_) {
      return std::unexpected(SourceCoverageError::ForeignDomain);
    }
    ranges.insert(ranges.end(), product->Ranges_.begin(), product->Ranges_.end());
  }
  std::ranges::sort(ranges, {}, &Range::First);
  const auto combined = FromRanges(expected.Revision_, expected.SourceCount_, ranges);
  if (!combined) { return std::unexpected(combined.error()); }
  if (*combined != expected) { return std::unexpected(SourceCoverageError::DifferentSources); }
  return {};
}

std::expected<SourceCoverage::Cut, SourceCoverageError>
SourceCoverage::Prefer(const SourceCoverage &domain,
                       std::span<const SourceCoverage *const> fallback,
                       std::span<const SourceCoverage *const> ready) {
  const auto older = RangesOf(domain, fallback);
  if (!older) { return std::unexpected(older.error()); }
  const auto newer = RangesOf(domain, ready);
  if (!newer) { return std::unexpected(newer.error()); }
  const size_t count = fallback.size() + ready.size();
  ConnectedProducts connected(count);
  std::vector<uint64_t> uncovered(fallback.size(), 0);
  for (const auto &range : *older) { uncovered[range.Product] += range.End - range.Begin; }
  size_t i = 0;
  size_t j = 0;
  while (i < older->size() && j < newer->size()) {
    const auto &a = (*older)[i];
    const auto &b = (*newer)[j];
    const uint64_t begin = std::max(a.Begin, b.Begin);
    const uint64_t end = std::min(a.End, b.End);
    if (begin < end) {
      uncovered[a.Product] -= end - begin;
      connected.Join(a.Product, fallback.size() + b.Product);
    }
    if (a.End == end) { ++i; }
    if (b.End == end) { ++j; }
  }
  std::vector<uint8_t> keep(count, 0);
  for (size_t at = 0; at < uncovered.size(); ++at) {
    if (uncovered[at] != 0) { keep[connected.Root(at)] = 1; }
  }
  Cut cut;
  for (size_t at = 0; at < fallback.size(); ++at) {
    if (keep[connected.Root(at)] != 0) { cut.Retained.push_back(at); }
  }
  for (size_t at = 0; at < ready.size(); ++at) {
    if (keep[connected.Root(fallback.size() + at)] == 0) { cut.Preferred.push_back(at); }
  }
  return cut;
}
}
