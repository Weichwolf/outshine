#ifndef OUTSHINE_GENERATORS_TERRAIN_INPUTS_GROUNDQUERY_H
#define OUTSHINE_GENERATORS_TERRAIN_INPUTS_GROUNDQUERY_H

#include "Earth.h"
#include "GroundSample.h"
#include <cstdint>

namespace outshine::Ground {
class GroundBlock;
class TerrainCertificate;
}

namespace outshine {

namespace Ground {

struct TileAt {
  int X = 0;
  int Y = 0;
};

struct TileSpot {
  int Zoom = 0;
  long X = 0;
  long Y = 0;
};

}

class GroundQuery {
public:
  struct HeightReading {
    GroundSample Sample;
    // Borrowed until the next query on this ground.
    const Ground::TerrainCertificate *Certificate = nullptr;
    uint64_t ResidentRevision = 0;
  };

  virtual ~GroundQuery() = default;

  [[nodiscard]] virtual GroundSample At(LongitudeLatitude at) const = 0;

  [[nodiscard]] virtual HeightReading ReadHeight(LongitudeLatitude at) const {
    return {.Sample = At(at)};
  }

  [[nodiscard]] virtual uint64_t TerrainScopeRevision() const noexcept { return 0; }

  [[nodiscard]] virtual bool
  HeightInputsCurrent([[maybe_unused]] const Ground::TerrainCertificate &certificate) const {
    return false;
  }

  [[nodiscard]] virtual GroundSample Resident(LongitudeLatitude at) const = 0;
  [[nodiscard]] virtual Ground::GroundBlock BlockAt(Ground::TileSpot at) const = 0;
  [[nodiscard]] virtual double PostM(double latDeg) const = 0;

  [[nodiscard]] virtual int BlockZoom() const { return 0; }
};

}
#endif
