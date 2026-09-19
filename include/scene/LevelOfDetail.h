#ifndef OUTSHINE_SCENE_LEVELOFDETAIL_H
#define OUTSHINE_SCENE_LEVELOFDETAIL_H

#include <cstdint>

namespace outshine {

/// Ordered representation classes shared by admission, generation and residency.
enum class LevelOfDetail : uint8_t {
  Fine,    ///< Full geometric detail.
  Shell,   ///< Exterior shell without secondary surface geometry.
  Massed,  ///< Aggregated groups preserving coarse volume.
  Skyline, ///< Coarse silhouette representation.
};

/// Map a relative spatial rung to a bounded representation class.
/// @param rungsCoarser Rungs relative to the finest tile; nonpositive selects Fine.
/// @return A class saturating at Skyline from rung three onward.
[[nodiscard]] constexpr LevelOfDetail LevelOfDetailForRung(int rungsCoarser) noexcept {
  if (rungsCoarser <= 0) { return LevelOfDetail::Fine; }
  if (rungsCoarser == 1) { return LevelOfDetail::Shell; }
  if (rungsCoarser == 2) { return LevelOfDetail::Massed; }
  return LevelOfDetail::Skyline;
}

/// Select the coarser of two valid representation classes.
[[nodiscard]] constexpr LevelOfDetail Coarser(LevelOfDetail one, LevelOfDetail two) noexcept {
  return static_cast<uint8_t>(one) > static_cast<uint8_t>(two) ? one : two;
}

static_assert(LevelOfDetailForRung(-1) == LevelOfDetail::Fine);
static_assert(LevelOfDetailForRung(0) == LevelOfDetail::Fine);
static_assert(LevelOfDetailForRung(1) == LevelOfDetail::Shell);
static_assert(LevelOfDetailForRung(2) == LevelOfDetail::Massed);
static_assert(LevelOfDetailForRung(9) == LevelOfDetail::Skyline);
static_assert(Coarser(LevelOfDetail::Fine, LevelOfDetail::Massed) == LevelOfDetail::Massed);
static_assert(Coarser(LevelOfDetail::Skyline, LevelOfDetail::Shell) == LevelOfDetail::Skyline);

}

#endif
