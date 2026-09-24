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
  const TangentFrame anchored = TangentFrame::At(stands);
  Vec3 held;
  for (int axis = 0; axis < 3; ++axis) {
    held[axis] = anchored.OriginEcef()[axis] + eye[0] * anchored.EastEcef()[axis] +
                 eye[1] * anchored.UpEcef()[axis] - eye[2] * anchored.NorthEcef()[axis];
  }
  const Ground::Geo above =
      Ground::EcefToGeoWgs84(Ground::Ecef{.X = held[0], .Y = held[1], .Z = held[2]});
  stands.LatitudeDeg = above.LatitudeDeg;
  stands.LongitudeDeg = above.LongitudeDeg;
  return stands;
}

}
