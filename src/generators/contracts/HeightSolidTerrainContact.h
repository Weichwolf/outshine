#ifndef OUTSHINE_GENERATORS_CONTRACTS_HEIGHTSOLIDTERRAINCONTACT_H
#define OUTSHINE_GENERATORS_CONTRACTS_HEIGHTSOLIDTERRAINCONTACT_H

#include "EarthworkStamp.h"
#include "HeightSolid.h"
#include <cstddef>
#include <cstdint>
#include <expected>

namespace outshine::Generators {
enum class SurfaceGroundEffect : uint8_t {
  Unspecified,
  GroundContact,
  OverheadClearance,
  NoEarthwork
};

struct SurfaceGroundSource {
  SurfaceGroundEffect Effect = SurfaceGroundEffect::Unspecified;
  uint64_t ContactKey = 0;
  double ClearanceM = 0;
  double ReliefM = 0;
};

enum class SurfaceEarthworkError : uint8_t {
  InvalidInput,
  ConflictingContactKeys,
  CapacityExceeded,
  Canceled
};

[[nodiscard]] bool ValidSurfaceGroundSource(const SurfaceGroundSource &source) noexcept;

[[nodiscard]] std::expected<EarthworkStamp, SurfaceEarthworkError> BuildHeightSolidTerrainContact(
    const HeightSolid &body, size_t face, const SurfaceGroundSource &source, double apronM);
}
#endif
