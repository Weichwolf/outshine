#ifndef OUTSHINE_WORLD_PRODUCTS_SOURCECOVERAGE_H
#define OUTSHINE_WORLD_PRODUCTS_SOURCECOVERAGE_H
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {
enum class SourceCoverageError { InvalidInput, Overlap, ForeignDomain, DifferentSources };

class SourceCoverage {
public:
  struct Range {
    uint64_t First = 0, Count = 0;
    bool operator==(const Range &other) const = default;
  };

  [[nodiscard]] static std::expected<SourceCoverage, SourceCoverageError>
  FromRanges(std::string revision, uint64_t sourceCount, std::span<const Range> ranges);
  [[nodiscard]] static std::expected<SourceCoverage, SourceCoverageError>
  FromSources(std::string revision, uint64_t sourceCount, std::span<const uint32_t> sources);
  [[nodiscard]] static std::expected<void, SourceCoverageError>
  Partition(const SourceCoverage &expected, std::span<const SourceCoverage *const> products);

  [[nodiscard]] std::string_view Revision() const noexcept { return Revision_; }

  [[nodiscard]] uint64_t SourceCount() const noexcept { return SourceCount_; }

  [[nodiscard]] std::span<const Range> Ranges() const noexcept { return Ranges_; }

  [[nodiscard]] size_t CapacityBytes() const noexcept {
    return Revision_.capacity() + 1 + Ranges_.capacity() * sizeof(Range);
  }

  bool operator==(const SourceCoverage &other) const = default;

private:
  SourceCoverage(std::string revision, uint64_t sourceCount, std::vector<Range> ranges);
  std::string Revision_;
  uint64_t SourceCount_;
  std::vector<Range> Ranges_;
};
}
#endif
