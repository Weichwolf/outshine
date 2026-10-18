#include "PlantShoot.h"
#include "PlantArc.h"
#include "PlantFrame.h"
#include "math/Vec3.h"
#include <cmath>
#include <optional>

namespace outshine::Generators {

std::optional<PlantLeafPose> PlantShoot::LeafAt(Placement placement) const noexcept {
  const auto arc = PlantArc{.LengthM = LengthM, .BendRad = BendRad}.FrameAt(placement.Parameter);
  const Vec3 radial =
      arc.Lateral * std::cos(placement.AzimuthRad) + arc.Normal * std::sin(placement.AzimuthRad);
  Vec3 direction = arc.Tangent * std::cos(LeafAngleRad) + radial * std::sin(LeafAngleRad);
  const Vec3 hint{NormalHint};
  if (NormalAlignment > 0) {
    const Vec3 normal = hint * (1.0 / Length(hint));
    direction = direction - normal * (NormalAlignment * Dot(direction, normal));
    direction = direction * (1.0 / Length(direction));
  }
  const auto frame = PlantNormalFrame(direction, hint);
  if (!frame) { return std::nullopt; }
  return PlantLeafPose{.OriginM = (arc.Centre + direction * PetioleM).Axis,
                       .NodeM = arc.Centre.Axis,
                       .Frame = *frame,
                       .StemParameter = placement.Parameter};
}
}
