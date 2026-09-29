#ifndef OUTSHINE_CLIENT_PLACETURN_H
#define OUTSHINE_CLIENT_PLACETURN_H

#include "TangentFrame.h"
#include "math/RenderFrame.h"
#include <scenario/Scenario.h>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
#include <utility>

namespace outshine::Shots {

inline constexpr std::size_t kTurnFrames = 60;

[[nodiscard]] inline std::vector<Scenario::View> PlaceTurn(const Scenario::View &initial,
                                                           LongitudeLatitude origin) {
  std::vector<Scenario::View> views;
  views.reserve(kTurnFrames);
  views.push_back(initial);
  const auto frame = TangentFrame::At(origin);
  const auto point = frame.ToLocalPosition(initial.Geographic.Geodetic);
  const Vec3 centre =
      Vec3{{point.EastM, point.UpM, RenderFrame::ZOfNorth(point.NorthM)}} + initial.OffsetM;
  const auto vertical = frame.ToLocalDirection(EnuAxesEcef(initial.Geographic.Geodetic).Up);
  const Vec3 up{{vertical.EastM, vertical.UpM, RenderFrame::ZOfNorth(vertical.NorthM)}};
  for (std::size_t index = 1; index < kTurnFrames; ++index) {
    auto view = initial;
    view.Id += "/turn/" + std::to_string(index);
    const double degrees =
        2.0 * kDegPerHalfTurn * static_cast<double>(index) / static_cast<double>(kTurnFrames);
    if (initial.Sees.LooksAt) {
      const double angle = -degrees * kDeg2Rad;
      const auto rotate = [&](Vec3 value) {
        return value * std::cos(angle) + Cross(up, value) * std::sin(angle) +
               up * (Dot(up, value) * (1.0 - std::cos(angle)));
      };
      view.Sees.LookAtM = centre + rotate(initial.Sees.LookAtM - centre);
      view.Sees.UpM = rotate(initial.Sees.UpM);
    } else {
      view.Geographic.BearingDeg =
          std::remainder(initial.Geographic.BearingDeg + degrees, 2.0 * kDegPerHalfTurn);
    }
    views.push_back(std::move(view));
  }
  return views;
}

}
#endif
