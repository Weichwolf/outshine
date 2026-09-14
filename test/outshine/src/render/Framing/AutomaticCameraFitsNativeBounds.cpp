#include "Framing.h"
#include "Check.h"
#include <cmath>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  static_assert(noexcept(FrameBounds(Box{})));
  const Box bounds{.Min = {{-3, -1, -2}}, .Max = {{3, 2, 4}}};
  for (const Vec3 origin : {Vec3{}, Vec3{{6378000, -4200000, 3200000}}}) {
    const Box translated{.Min = bounds.Min + origin, .Max = bounds.Max + origin};
    for (double aspect : {0.2, 0.5, 1.0, 2.0, 5.0}) {
      const auto camera = FrameBounds(translated, {.Aspect = aspect});
      CHECK(camera.has_value(), "native bounds produce a camera in portrait and landscape formats");
      if (!camera) { continue; }
      CHECK(camera->ZNearM > 0 && camera->ZFarM > camera->ZNearM,
            "automatic clipping interval is positive and forward");
      const double halfHeight = std::tan(camera->YfovRad * 0.5);
      for (unsigned corner = 0; corner < 8; ++corner) {
        const Vec3 offset = translated.Corner(corner) - camera->EyeM;
        const double depth = Dot(offset, camera->Forward);
        const double x = Dot(offset, camera->Right) / (depth * halfHeight * aspect);
        const double y = Dot(offset, camera->Up) / (depth * halfHeight);
        CHECK(depth >= camera->ZNearM && depth <= camera->ZFarM && std::abs(x) <= 1 &&
                  std::abs(y) <= 1,
              "analytic perspective projection keeps every corner inside all six planes");
      }
      const auto defaultFill = FrameBounds(translated, {.Fill = 0, .Aspect = aspect});
      CHECK(defaultFill && defaultFill->EyeM == camera->EyeM,
            "zero fill retains the declared default framing convention");
    }
  }
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  for (const auto options : {FramingOptions{.Aspect = 0},
                             FramingOptions{.Aspect = -1},
                             FramingOptions{.Aspect = nan},
                             FramingOptions{.Aspect = infinity},
                             FramingOptions{.Fill = -1},
                             FramingOptions{.Fill = nan},
                             FramingOptions{.Fill = infinity}}) {
    const auto result = FrameBounds(bounds, options);
    CHECK(!result && !result.error().empty(),
          "invalid lens/framing input reports a recoverable error");
  }
  for (const auto invalid : {Box{},
                             Box{.Min = {}, .Max = {}},
                             Box{.Min = {{0, 2, 0}}, .Max = {{1, 1, 1}}},
                             Box{.Min = {{nan, 0, 0}}, .Max = {{1, 1, 1}}},
                             Box{.Min = {{-1e308, 0, 0}}, .Max = {{1e308, 1, 1}}}}) {
    CHECK(!FrameBounds(invalid),
          "empty unordered nonfinite or unrepresentable bounds cannot publish a camera");
  }
  return Report();
}
