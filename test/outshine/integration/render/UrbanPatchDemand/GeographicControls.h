#ifndef OUTSHINE_TEST_GEOGRAPHICCONTROLS_H
#define OUTSHINE_TEST_GEOGRAPHICCONTROLS_H

#include "UrbanArea.h"
#include "Geodesy.h"
#include "FrameContext.h"
#include "math/Units.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace UrbanDemandControl {
using namespace outshine;
namespace Osm = Generators::Osm;
constexpr uint32_t kSide = 35, kPatch = 8;
constexpr double kPitchM = 512;

struct LocalFrame {
  Vec3 Origin;
  EnuAxes Axes;
  Vec3 Offset;

  [[nodiscard]] Vec3 Direction(const Vec3 &ecef) const {
    return {{Dot(ecef, Axes.East), Dot(ecef, Axes.Up), -Dot(ecef, Axes.North)}};
  }

  [[nodiscard]] Vec3 Point(const Vec3 &ecef) const { return Direction(ecef - Origin) + Offset; }
};

inline LocalFrame FrameAt(LongitudeLatitude focus, const Vec3 &offset) {
  const LongitudeLatitudeHeight origin{.LongitudeDeg = focus.LongitudeDeg,
                                       .LatitudeDeg = focus.LatitudeDeg};
  LocalFrame frame{.Origin = {}, .Axes = EnuAxesEcef(origin), .Offset = offset};
  GeoToEcef(origin, frame.Origin);
  return frame;
}

inline Osm::UrbanArea FieldAt(LongitudeLatitude focus) {
  Osm::UrbanArea area;
  area.Columns = area.Rows = kSide;
  area.Recipe.ProjectedPitchM = kPitchM;
  area.FirstCell = {
      static_cast<int64_t>(std::floor(kWgs84A * focus.LongitudeDeg * kDeg2Rad / kPitchM)) -
          kSide / 2,
      static_cast<int64_t>(
          std::floor(kWgs84A * std::asinh(std::tan(focus.LatitudeDeg * kDeg2Rad)) / kPitchM)) +
          kSide / 2};
  area.Tiers.resize(static_cast<size_t>(kSide) * kSide);
  constexpr size_t tierStride = 17;
  for (size_t cell = 0; cell < area.Tiers.size(); ++cell) {
    area.Tiers[cell] =
        static_cast<uint8_t>((cell * tierStride + cell / kSide) % Osm::UrbanArea::HeightsM.size());
  }
  return area;
}

struct ViewPose {
  double YawRad = 0;
  double AltitudeM = 0;
};

inline Render::FrameContext ViewAt(ViewPose pose, const Vec3 &offset) {
  Render::FrameContext frame;
  frame.Mvp.Column.fill(0);
  frame.PreViewTranslation = offset * -1.0;
  const auto sine = static_cast<float>(std::sin(pose.YawRad));
  const auto cosine = static_cast<float>(std::cos(pose.YawRad));
  constexpr float focal = 1.5f;
  constexpr float nearM = .1f;
  frame.Mvp.At(0, 0) = focal * cosine;
  frame.Mvp.At(0, 2) = focal * sine;
  frame.Mvp.At(1, 1) = focal;
  frame.Mvp.At(1, 3) = -focal * static_cast<float>(pose.AltitudeM);
  frame.Mvp.At(2, 3) = nearM;
  frame.Mvp.At(3, 0) = sine;
  frame.Mvp.At(3, 2) = -cosine;
  return frame;
}

inline std::vector<Vec3> Corners(const Osm::UrbanArea &area,
                                 Osm::UrbanPatchRange range,
                                 std::array<double, 2> ground,
                                 const LocalFrame &frame) {
  std::vector<Vec3> points;
  constexpr size_t cornersPerGround = 8;
  points.reserve(static_cast<size_t>(range.Columns) * range.Rows * ground.size() *
                 cornersPerGround);
  for (uint32_t row = range.Row; row < range.Row + range.Rows; ++row) {
    for (uint32_t column = range.Column; column < range.Column + range.Columns; ++column) {
      const auto lot = area.LotAt(column, row);
      if (!lot) { continue; }
      const auto axes = EnuAxesEcef(
          {.LongitudeDeg = lot->Centre.LongitudeDeg, .LatitudeDeg = lot->Centre.LatitudeDeg});
      for (const double groundM : ground) {
        Vec3 base;
        GeoToEcef({.LongitudeDeg = lot->Centre.LongitudeDeg,
                   .LatitudeDeg = lot->Centre.LatitudeDeg,
                   .HeightM = groundM},
                  base);
        for (const double east : {-lot->EastSpanM / 2, lot->EastSpanM / 2}) {
          for (const double north : {-lot->NorthSpanM / 2, lot->NorthSpanM / 2}) {
            for (const double up : {0., static_cast<double>(lot->HeightM)}) {
              points.push_back(
                  frame.Point(base + axes.East * east + axes.North * north + axes.Up * up));
            }
          }
        }
      }
    }
  }
  return points;
}
}
#endif
