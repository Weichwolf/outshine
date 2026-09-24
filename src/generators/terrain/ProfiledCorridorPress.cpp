#include "ProfiledCorridorPress.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <tuple>

namespace outshine::ProfiledCorridor {
namespace {

struct CubicSample {
  double Value = 0.0;
  double First = 0.0;
  double Second = 0.0;
};

struct HermiteCurve {
  double Begin;
  double End;
  double BeginDerivative;
  double EndDerivative;
};

constexpr double kCubicSecondDerivative = 6.0;
constexpr double kSmoothstepSix = 6.0;
constexpr double kSmoothstepFifteen = 15.0;
constexpr double kSmoothstepTen = 10.0;
constexpr double kMinimumCurvature = 1e-12;

CubicSample Hermite(HermiteCurve curve, double t) {
  const double squared = t * t;
  const double cubed = squared * t;
  const double delta = curve.End - curve.Begin;
  const double bend = curve.BeginDerivative + curve.EndDerivative - 2.0 * delta;
  const double slope = 3.0 * delta - 2.0 * curve.BeginDerivative - curve.EndDerivative;
  return {.Value = curve.Begin + curve.BeginDerivative * t + slope * squared + bend * cubed,
          .First = curve.BeginDerivative + 2.0 * slope * t + 3.0 * bend * squared,
          .Second = 2.0 * slope + kCubicSecondDerivative * bend * t};
}

}

std::optional<Offer> OfferAt(const ProfiledCorridorSpan &profile, double apronM, EastNorth at) {
  const double runE = profile.EndM.EastM - profile.BeginM.EastM;
  const double runN = profile.EndM.NorthM - profile.BeginM.NorthM;
  const double lengthSquared = runE * runE + runN * runN;
  if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0) { return std::nullopt; }
  double along = std::clamp(
      ((at.EastM - profile.BeginM.EastM) * runE + (at.NorthM - profile.BeginM.NorthM) * runN) /
          lengthSquared,
      0.0,
      1.0);
  const auto eastAt = [&](double t) {
    return Hermite({.Begin = profile.BeginM.EastM,
                    .End = profile.EndM.EastM,
                    .BeginDerivative = profile.BeginDerivativeM.EastM,
                    .EndDerivative = profile.EndDerivativeM.EastM},
                   t);
  };
  const auto northAt = [&](double t) {
    return Hermite({.Begin = profile.BeginM.NorthM,
                    .End = profile.EndM.NorthM,
                    .BeginDerivative = profile.BeginDerivativeM.NorthM,
                    .EndDerivative = profile.EndDerivativeM.NorthM},
                   t);
  };
  for (int iteration = 0; iteration < 6; ++iteration) {
    const CubicSample east = eastAt(along);
    const CubicSample north = northAt(along);
    const double offE = east.Value - at.EastM;
    const double offN = north.Value - at.NorthM;
    const double gradient = offE * east.First + offN * north.First;
    const double curvature = east.First * east.First + north.First * north.First +
                             offE * east.Second + offN * north.Second;
    if (!std::isfinite(curvature) || curvature <= kMinimumCurvature) { break; }
    along = std::clamp(along - gradient / curvature, 0.0, 1.0);
  }
  const auto distanceSquaredAt = [&](double t) {
    const double east = eastAt(t).Value - at.EastM;
    const double north = northAt(t).Value - at.NorthM;
    return east * east + north * north;
  };
  double distanceSquared = distanceSquaredAt(along);
  for (const double endpoint : {0.0, 1.0}) {
    const double candidate = distanceSquaredAt(endpoint);
    if (candidate < distanceSquared) {
      distanceSquared = candidate;
      along = endpoint;
    }
  }
  const double halfWidthM = std::lerp(profile.BeginHalfWidthM, profile.EndHalfWidthM, along);
  const double pavementM =
      std::lerp(profile.BeginPavementHalfWidthM, profile.EndPavementHalfWidthM, along);
  const double outsideM = std::sqrt(distanceSquared) - halfWidthM;
  const double bedM = Hermite({.Begin = profile.BeginBedM,
                               .End = profile.EndBedM,
                               .BeginDerivative = profile.BeginDerivativeM.UpM,
                               .EndDerivative = profile.EndDerivativeM.UpM},
                              along)
                          .Value;
  if (!std::isfinite(outsideM) || !std::isfinite(bedM) || outsideM > apronM) {
    return std::nullopt;
  }
  return Offer{.DistanceSquared = distanceSquared,
               .OutsideM = outsideM,
               .BedM = bedM,
               .PavementHalfWidthM = pavementM,
               .FullHalfWidthM = halfWidthM};
}

double Smoothstep(double t) {
  return t * t * t * (t * (t * kSmoothstepSix - kSmoothstepFifteen) + kSmoothstepTen);
}

void Accumulate(const ProfiledCorridorSpan &profile,
                double apronM,
                EastNorth at,
                double nearestSquared,
                Average &average) {
  if (profile.StationLengthM <= 0.0 || apronM <= 0.0) { return; }
  constexpr std::array samples{0.2113248654051871, 0.7886751345948129};
  const double sigma = apronM * (2.0 / 3.0);
  const double denominator = 2.0 * sigma * sigma;
  for (const double t : samples) {
    const double east = Hermite({.Begin = profile.BeginM.EastM,
                                 .End = profile.EndM.EastM,
                                 .BeginDerivative = profile.BeginDerivativeM.EastM,
                                 .EndDerivative = profile.EndDerivativeM.EastM},
                                t)
                            .Value;
    const double north = Hermite({.Begin = profile.BeginM.NorthM,
                                  .End = profile.EndM.NorthM,
                                  .BeginDerivative = profile.BeginDerivativeM.NorthM,
                                  .EndDerivative = profile.EndDerivativeM.NorthM},
                                 t)
                             .Value;
    const double bed = Hermite({.Begin = profile.BeginBedM,
                                .End = profile.EndBedM,
                                .BeginDerivative = profile.BeginDerivativeM.UpM,
                                .EndDerivative = profile.EndDerivativeM.UpM},
                               t)
                           .Value;
    const double distanceSquared =
        (east - at.EastM) * (east - at.EastM) + (north - at.NorthM) * (north - at.NorthM);
    const double supportM = std::max(profile.BeginHalfWidthM, profile.EndHalfWidthM) + apronM;
    const double fade = std::clamp((supportM - std::sqrt(distanceSquared)) / apronM, 0.0, 1.0);
    const double taper = Smoothstep(fade);
    const double weight = profile.StationLengthM * 0.5 * taper *
                          std::exp(-std::max(0.0, distanceSquared - nearestSquared) / denominator);
    average.Weight += weight;
    average.HeightM += weight * bed;
  }
}

bool Earlier(const ProfiledCorridorSpan &a, const ProfiledCorridorSpan &b) {
  return std::tie(
             a.BeginM.EastM, a.BeginM.NorthM, a.EndM.EastM, a.EndM.NorthM, a.BeginBedM, a.EndBedM) <
         std::tie(
             b.BeginM.EastM, b.BeginM.NorthM, b.EndM.EastM, b.EndM.NorthM, b.BeginBedM, b.EndBedM);
}

}
