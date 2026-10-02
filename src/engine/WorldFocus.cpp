#include "EngineHeld.h"
#include "GeodeticCamera.h"
#include "TangentFrame.h"
#include "math/Vec3.h"

namespace outshine {

LongitudeLatitude Engine::State::CurrentGeographicFocus() const {
  const double anchorLat = Session.Declared.Ground.Origin.LatitudeDeg;
  const double anchorLon = Session.Declared.Ground.Origin.LongitudeDeg;
  LongitudeLatitude stands{.LongitudeDeg = anchorLon, .LatitudeDeg = anchorLat};
  Vec3 eye;
  if (Session.Views && Session.Views->Active().Placement == Scenario::CameraPlacement::Geodetic) {
    const auto &camera = Session.Views->Active();
    const auto position = ResolveGeodeticCamera(
        camera, stands, World.Stack.Opened() ? &World.Stack.Ground() : nullptr);
    if (!position || !*position) {
      return {.LongitudeDeg = camera.Geographic.Geodetic.LongitudeDeg,
              .LatitudeDeg = camera.Geographic.Geodetic.LatitudeDeg};
    }
    eye = **position;
  } else {
    if (Picture.Standing == nullptr || !Picture.Standing->Watched()) { return stands; }
    eye = Picture.Standing->Watching().EyeM;
  }
  return GeographicFocusFor(eye);
}

LongitudeLatitude Engine::State::GeographicFocusFor(const Vec3 &eye) const {
  return outshine::GeographicFocusFor(eye,
                                      {.LongitudeDeg = Session.Declared.Ground.Origin.LongitudeDeg,
                                       .LatitudeDeg = Session.Declared.Ground.Origin.LatitudeDeg});
}

}
