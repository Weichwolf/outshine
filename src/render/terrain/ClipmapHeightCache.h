#ifndef OUTSHINE_RENDER_TERRAIN_CLIPMAPHEIGHTCACHE_H
#define OUTSHINE_RENDER_TERRAIN_CLIPMAPHEIGHTCACHE_H

#include "Vec2.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {
using ClipmapNode = std::array<int64_t, 2>;

struct ClipmapGrid {
  uint32_t HalfCells = 0;
  uint32_t Levels = 0;
  double CellM = 1;
};

struct ClipmapHeightRegion {
  ClipmapNode Low{};
  uint32_t Level = 0;
  uint32_t Width = 0;
  uint32_t Height = 0;
};

enum class ClipmapHeightError {
  InvalidGrid,
  InvalidCoordinate,
  OverBudget,
  StaleUpdate,
  InvalidSamples
};

class ClipmapHeightCache;

class ClipmapHeightUpdate {
public:
  ClipmapHeightUpdate(const ClipmapHeightUpdate &) = delete;
  ClipmapHeightUpdate &operator=(const ClipmapHeightUpdate &) = delete;

  ClipmapHeightUpdate(ClipmapHeightUpdate &&other) noexcept
      : Owner_(std::exchange(other.Owner_, nullptr)),
        Generation_(other.Generation_),
        Camera_(other.Camera_),
        Positions_(std::move(other.Positions_)),
        Samples_(std::move(other.Samples_)),
        Regions_(std::move(other.Regions_)) {}

  ClipmapHeightUpdate &operator=(ClipmapHeightUpdate &&) noexcept = delete;

  [[nodiscard]] std::span<const Vec2> Positions() const noexcept { return Positions_; }

  [[nodiscard]] std::span<float> Samples() noexcept { return Samples_; }

  [[nodiscard]] std::span<const ClipmapHeightRegion> Regions() const noexcept { return Regions_; }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return Positions_.capacity() * sizeof(Vec2) + Samples_.capacity() * sizeof(float) +
           Regions_.capacity() * sizeof(ClipmapHeightRegion);
  }

private:
  friend class ClipmapHeightCache;
  ClipmapHeightUpdate() = default;
  const ClipmapHeightCache *Owner_ = nullptr;
  uint64_t Generation_ = 0;
  Vec2 Camera_{};
  std::vector<Vec2> Positions_;
  std::vector<float> Samples_;
  std::vector<ClipmapHeightRegion> Regions_;
};

class ClipmapHeightCache {
public:
  ClipmapHeightCache(const ClipmapHeightCache &) = delete;
  ClipmapHeightCache &operator=(const ClipmapHeightCache &) = delete;
  ClipmapHeightCache(ClipmapHeightCache &&) noexcept = default;
  ClipmapHeightCache &operator=(ClipmapHeightCache &&) noexcept = delete;

  [[nodiscard]] static std::expected<ClipmapHeightCache, ClipmapHeightError> Make(ClipmapGrid grid,
                                                                                  size_t bytesMost);

  // Plan/commit on the owner; workers only fill their owned update's Samples.
  [[nodiscard]] std::expected<ClipmapHeightUpdate, ClipmapHeightError> Plan(Vec2 camera,
                                                                            size_t bytesMost) const;
  [[nodiscard]] std::expected<void, ClipmapHeightError>
  Commit(const ClipmapHeightUpdate &update) noexcept;

  [[nodiscard]] std::optional<float> Height(uint32_t level, ClipmapNode node) const noexcept;
  [[nodiscard]] std::span<const float> Values(uint32_t level) const noexcept;
  [[nodiscard]] std::optional<ClipmapNode> Origin(uint32_t level) const noexcept;

  [[nodiscard]] uint32_t Side() const noexcept { return Side_; }

  [[nodiscard]] const ClipmapGrid &Grid() const noexcept { return Grid_; }

  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  struct Level {
    std::vector<float> Values;
    std::optional<ClipmapNode> Low = std::nullopt;
    double Step = 0;
  };

  struct WorkSize {
    size_t Samples = 0;
    size_t Regions = 0;
  };

  ClipmapHeightCache() = default;
  [[nodiscard]] size_t Slot(ClipmapNode node) const noexcept;
  [[nodiscard]] std::expected<ClipmapNode, ClipmapHeightError> Low(Vec2 camera, double step) const;
  [[nodiscard]] std::expected<WorkSize, ClipmapHeightError> Measure(Vec2 camera,
                                                                    size_t bytesMost) const;

  ClipmapGrid Grid_{};
  uint32_t Side_ = 0;
  uint64_t Generation_ = 0;
  std::vector<Level> Levels_;
};
}
#endif
