#include "EngineHeld.h"
#include <optional>
#include "GeodeticCamera.h"
#include "Geodesy.h"
#include "TangentFrame.h"
#include "math/Vec3.h"

namespace outshine {

std::optional<LongitudeLatitudeHeight> Engine::State::CurrentGeographicPosition() const {
  const LongitudeLatitude origin{.LongitudeDeg = Session.Declared.Ground.Origin.LongitudeDeg,
                                 .LatitudeDeg = Session.Declared.Ground.Origin.LatitudeDeg};
  Vec3 eye;
  if (Session.Views && Session.Views->Active().Placement == Scenario::CameraPlacement::Geodetic) {
    const auto position = ResolveGeodeticCamera(
        Session.Views->Active(), origin, World.Stack.Opened() ? &World.Stack.Ground() : nullptr);
    if (!position || !*position) { return std::nullopt; }
    eye = **position;
  } else {
    if (Picture.Standing == nullptr || !Picture.Standing->Watched()) { return std::nullopt; }
    eye = Picture.Standing->Watching().EyeM;
  }
  return outshine::GeographicPositionFor(eye, origin);
}

std::optional<Vec3> Engine::State::CurrentGeographicEyeEcef() const {
  const auto position = CurrentGeographicPosition();
  if (!position) { return std::nullopt; }
  Vec3 eye;
  GeoToEcef(*position, eye);
  return eye;
}

LongitudeLatitude Engine::State::CurrentGeographicFocus() const {
  if (const auto position = CurrentGeographicPosition()) {
    return {.LongitudeDeg = position->LongitudeDeg, .LatitudeDeg = position->LatitudeDeg};
  }
  if (Session.Views && Session.Views->Active().Placement == Scenario::CameraPlacement::Geodetic) {
    const auto &camera = Session.Views->Active();
    return {.LongitudeDeg = camera.Geographic.Geodetic.LongitudeDeg,
            .LatitudeDeg = camera.Geographic.Geodetic.LatitudeDeg};
  }
  return {.LongitudeDeg = Session.Declared.Ground.Origin.LongitudeDeg,
          .LatitudeDeg = Session.Declared.Ground.Origin.LatitudeDeg};
}

LongitudeLatitude Engine::State::GeographicFocusFor(const Vec3 &eye) const {
  return outshine::GeographicFocusFor(eye,
                                      {.LongitudeDeg = Session.Declared.Ground.Origin.LongitudeDeg,
                                       .LatitudeDeg = Session.Declared.Ground.Origin.LatitudeDeg});
}

}
