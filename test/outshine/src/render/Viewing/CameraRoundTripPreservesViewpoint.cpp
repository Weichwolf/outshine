#include "Viewing.h"
#include "Check.h"
#include <array>
#include <type_traits>

static_assert(noexcept(outshine::Render::ViewpointOf(std::declval<const outshine::Camera &>())));

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  Viewpoint perspective;
  perspective.EyeM = {{3, -2, 7}};
  perspective.Forward = {{0, 0, -1}};
  perspective.Right = {{1, 0, 0}};
  perspective.Up = {{0, 1, 0}};
  perspective.Kind = CameraKind::Perspective;
  perspective.YfovRad = 1.2;
  perspective.ZNearM = 0.125;
  perspective.ZFarM = 400;
  Viewpoint orthographic;
  orthographic.EyeM = {{-4, 5, 6}};
  orthographic.Forward = {{-1, 0, 0}};
  orthographic.Right = {{0, 0, -1}};
  orthographic.Up = {{0, 1, 0}};
  orthographic.Kind = CameraKind::Orthographic;
  orthographic.XMagM = 17;
  orthographic.YMagM = 9;
  orthographic.ZNearM = 0;
  orthographic.ZFarM = 80;
  const std::array views{perspective, orthographic};
  for (const Viewpoint &source : views) {
    Camera camera;
    CameraOf(source, camera);
    const auto restored = ViewpointOf(camera);
    CHECK(restored.has_value(), "a native camera reconstructs its renderer view");
    if (!restored) { continue; }
    CHECK(restored->EyeM == source.EyeM && restored->Forward == source.Forward &&
              restored->Right == source.Right && restored->Up == source.Up,
          "camera placement and orthonormal basis survive the native round trip");
    CHECK(restored->Kind == source.Kind && restored->YfovRad == source.YfovRad &&
              restored->XMagM == source.XMagM && restored->YMagM == source.YMagM &&
              restored->ZNearM == source.ZNearM && restored->ZFarM == source.ZFarM,
          "perspective and orthographic lens values survive the native round trip");
  }
  Camera invalid;
  invalid.LooksAt = true;
  invalid.PositionM = {{1, 2, 3}};
  invalid.LookAtM = invalid.PositionM;
  CHECK(!ViewpointOf(invalid), "a collapsed native look-at pose cannot become a renderer view");
  return Report();
}
