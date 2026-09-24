#ifndef OUTSHINE_GENERATORS_ROAD_ROADTERRAINCONTACT_H
#define OUTSHINE_GENERATORS_ROAD_ROADTERRAINCONTACT_H

#include <numbers>

namespace outshine::Generators {

struct RoadTerrainContact {
  static constexpr double MaximumPostingM = 3.0;
  static constexpr double VergeM = 5.0;
};

static_assert(RoadTerrainContact::VergeM >
              RoadTerrainContact::MaximumPostingM * std::numbers::sqrt2_v<double>);

}

#endif
