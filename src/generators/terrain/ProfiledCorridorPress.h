#ifndef OUTSHINE_GENERATORS_TERRAIN_PROFILEDCORRIDORPRESS_H
#define OUTSHINE_GENERATORS_TERRAIN_PROFILEDCORRIDORPRESS_H

#include <optional>

#include "GroundMesher.h"

namespace outshine::ProfiledCorridor {

struct Offer {
  double DistanceSquared = 0.0;
  double OutsideM = 0.0;
  double BedM = 0.0;
  double PavementHalfWidthM = 0.0;
  double FullHalfWidthM = 0.0;
};

struct Average {
  double Weight = 0.0;
  double HeightM = 0.0;
};

[[nodiscard]] std::optional<Offer>
OfferAt(const ProfiledCorridorSpan &profile, double apronM, EastNorth at);

void Accumulate(const ProfiledCorridorSpan &profile,
                double apronM,
                EastNorth at,
                double nearestSquared,
                Average &average);

[[nodiscard]] bool Earlier(const ProfiledCorridorSpan &a, const ProfiledCorridorSpan &b);

[[nodiscard]] double Smoothstep(double t);

}

#endif
