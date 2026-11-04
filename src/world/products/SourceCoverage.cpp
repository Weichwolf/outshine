#include "SourceCoverage.h"
#include <algorithm>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
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
}
