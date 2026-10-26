#ifndef OUTSHINE_RENDER_TERRAIN_TERRAINPATCHBOUNDS_H
#define OUTSHINE_RENDER_TERRAIN_TERRAINPATCHBOUNDS_H
#include "GroundTile.h"
#include "TerrainLattice.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine::Render {
class TerrainPatchBounds {
public:
  TerrainPatchBounds();

  struct alignas(16) Bounds {
    std::array<float, 4> Low{};
    std::array<float, 4> High{};
    std::array<float, 4> Errors{};
  };

  static constexpr size_t kBoundsBytes = 48;
  static_assert(sizeof(Bounds) == kBoundsBytes && alignof(Bounds) == 16);
  static_assert(offsetof(Bounds, High) == 16 && offsetof(Bounds, Errors) == 32);

  void Fractions(std::span<const float> fractions);

  [[nodiscard]] std::span<const float> Fractions() const noexcept { return Fractions_; }

  void Page(PageId page, std::span<const float> heights);
  void Sources(std::span<const TerrainInstance> tiles, uint32_t real);
  void Include(size_t tile, std::vector<Bounds> &into) const;

  [[nodiscard]] uint64_t Generation() const noexcept { return Generation_; }

private:
  static constexpr size_t kPatchCount = TerrainLattice::kPatchCount;

  struct PageRange {
    std::array<std::array<float, 2>, TerrainLattice::kPatchCount> Patches{};
    std::array<float, 2> Whole{};
    std::array<std::array<float, 4>, TerrainLattice::kPatchCount> Errors{};
    uint64_t Revision = 0;
    bool Finite = false;
  };

  struct TileRange {
    std::array<Bounds, TerrainLattice::kPatchCount> Patches{};
    PageId Page = kNoPage;
    uint64_t Revision = 0;
  };

  [[nodiscard]] TileRange BoundsFor(const TerrainInstance &tile, bool uniform) const;
  std::array<float, TerrainLattice::kGroundSide> Fractions_{};
  std::vector<PageRange> Pages_;
  std::vector<TileRange> Tiles_;
  uint64_t Revision_ = 0;
  uint64_t Generation_ = 0;
  bool AlignedFractions_ = true;
  bool UniformFractions_ = true;
  bool FiniteFractions_ = true;
};
}
#endif
