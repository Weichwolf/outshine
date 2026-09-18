#include "CameraFraming.h"
#include "Check.h"
#include "math/Units.h"
#include <cmath>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  static_assert(noexcept(FrameCamera(Box{})));
  const Box bounds{.Min = {{-3, -1, -2}}, .Max = {{3, 2, 4}}};
  for (const Vec3 origin : {Vec3{}, Vec3{{6378000, -4200000, 3200000}}}) {
    const Box translated{.Min = bounds.Min + origin, .Max = bounds.Max + origin};
    for (double aspect : {0.2, 0.5, 1.0, 2.0, 5.0}) {
      const auto camera = FrameCamera(translated, {.Aspect = aspect});
      CHECK(camera.has_value(), "native bounds produce a camera in portrait and landscape formats");
      if (!camera) { continue; }
      CHECK(camera->NearM > 0 && camera->FarM > camera->NearM,
            "automatic clipping interval is positive and forward");
      Vec3 forward = camera->LookAtM - camera->PositionM;
      CHECK(camera->LooksAt && Normalise(forward),
            "automatic camera has a valid native look-at pose");
      Vec3 right = Cross(forward, camera->UpM);
      CHECK(Normalise(right), "automatic camera has a valid horizontal basis");
      const Vec3 up = Cross(right, forward);
      const double halfHeight = std::tan(camera->FovDeg * kDeg2Rad * 0.5);
      for (unsigned corner = 0; corner < 8; ++corner) {
        const Vec3 offset = translated.Corner(corner) - camera->PositionM;
        const double depth = Dot(offset, forward);
        const double x = Dot(offset, right) / (depth * halfHeight * aspect);
        const double y = Dot(offset, up) / (depth * halfHeight);
        CHECK(depth >= camera->NearM && depth <= camera->FarM && std::abs(x) <= 1 &&
                  std::abs(y) <= 1,
              "analytic perspective projection keeps every corner inside all six planes");
      }
      const auto defaultFill = FrameCamera(translated, {.Fill = 0, .Aspect = aspect});
      CHECK(defaultFill && defaultFill->PositionM == camera->PositionM,
            "zero fill retains the declared default framing convention");
    }
  }
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  for (const auto options : {CameraFramingOptions{.Aspect = 0},
                             CameraFramingOptions{.Aspect = -1},
                             CameraFramingOptions{.Aspect = nan},
                             CameraFramingOptions{.Aspect = infinity},
                             CameraFramingOptions{.Fill = -1},
                             CameraFramingOptions{.Fill = nan},
                             CameraFramingOptions{.Fill = infinity}}) {
    const auto result = FrameCamera(bounds, options);
    CHECK(!result && !result.error().empty(),
          "invalid lens/framing input reports a recoverable error");
  }
  for (const auto invalid : {Box{},
                             Box{.Min = {}, .Max = {}},
                             Box{.Min = {{0, 2, 0}}, .Max = {{1, 1, 1}}},
                             Box{.Min = {{nan, 0, 0}}, .Max = {{1, 1, 1}}},
                             Box{.Min = {{-1e308, 0, 0}}, .Max = {{1e308, 1, 1}}}}) {
    CHECK(!FrameCamera(invalid),
          "empty unordered nonfinite or unrepresentable bounds cannot publish a camera");
  }
  return Report();
}
