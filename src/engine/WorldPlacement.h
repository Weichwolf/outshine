#ifndef OUTSHINE_ENGINE_WORLDPLACEMENT_H
#define OUTSHINE_ENGINE_WORLDPLACEMENT_H

#include "ClusterId.h"
#include "Tile.h"
#include "TangentFrame.h"
#include "math/Mat4.h"

namespace outshine {

struct WorldPlacement {
  LongitudeLatitude Position;
  double AslM = 0.0;
  float YawRad = 0.0f;
  float Scale = 1.0f;

  [[nodiscard]] Mat4 ModelIn(const TangentFrame &frame) const {
    const auto local = TangentFrame::At(Position);
    const double sine = std::sin(static_cast<double>(YawRad));
    const double cosine = std::cos(static_cast<double>(YawRad));
    const std::array<Vec3, 3> axes{local.EastEcef() * cosine + local.NorthEcef() * sine,
                                   local.UpEcef(),
                                   local.EastEcef() * sine - local.NorthEcef() * cosine};
    Mat4 result;
    for (size_t column = 0; column < axes.size(); ++column) {
      const auto direction = RenderFrame::Of(frame.Turn(axes[column]));
      for (size_t row = 0; row < 3; ++row) { result.At(row, column) = direction[row] * Scale; }
    }
    const auto position = RenderFrame::Of(frame.Place({.LongitudeDeg = Position.LongitudeDeg,
                                                       .LatitudeDeg = Position.LatitudeDeg,
                                                       .HeightM = AslM}));
    for (size_t row = 0; row < 3; ++row) { result.At(row, 3) = position[row]; }
    return result;
  }

  static WorldPlacement From(const Generators::Tile &region, const Generators::Scattered &local) {
    return {.Position = region.Geo({.EastM = local.Em, .NorthM = local.Nm}),
            .AslM = local.AslM,
            .YawRad = local.YawRad,
            .Scale = local.Scale};
  }
};

struct WorldInstance {
  uint32_t Body = 0;
  uint32_t Cluster = 0;
  WorldPlacement Where;
};

}
#endif
