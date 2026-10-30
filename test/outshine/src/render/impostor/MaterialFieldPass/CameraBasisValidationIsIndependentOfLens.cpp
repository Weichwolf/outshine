#include "Check.h"
#include "MaterialFieldPass.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  Viewpoint camera;
  CHECK(ValidMaterialFieldCameraBasis(camera), "default basis is valid without a capture lens");
  camera.Kind = CameraKind::Orthographic;
  camera.EyeM = {{1e10, -2e10, 3e10}};
  camera.Right = {{.8, 0, -.6}};
  camera.Up = {{0, 1, 0}};
  camera.Forward = {{-.6, 0, -.8}};
  CHECK(ValidMaterialFieldCameraBasis(camera), "rotated orthographic basis accepts a finite eye");
  camera.YfovRad = std::numeric_limits<double>::quiet_NaN();
  camera.XMagM = -1;
  CHECK(ValidMaterialFieldCameraBasis(camera), "lighting basis does not validate unused optics");
  std::array scaled{camera, camera, camera};
  scaled[0].Right = scaled[0].Right * 2.0;
  scaled[1].Up = scaled[1].Up * 2.0;
  scaled[2].Forward = scaled[2].Forward * 2.0;
  for (const auto &candidate : scaled) {
    CHECK(!ValidMaterialFieldCameraBasis(candidate),
          "scaled basis axes refuse before any GPU work");
  }
  for (const double value :
       {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
    std::array invalid{camera, camera, camera, camera};
    invalid[0].EyeM[0] = value;
    invalid[1].Right[0] = value;
    invalid[2].Up[0] = value;
    invalid[3].Forward[0] = value;
    for (const auto &candidate : invalid) {
      CHECK(!ValidMaterialFieldCameraBasis(candidate), "every camera vector must be finite");
    }
  }
  auto mirrored = camera;
  mirrored.Right = mirrored.Right * -1.0;
  CHECK(!ValidMaterialFieldCameraBasis(mirrored), "opposite handedness refuses");
  Viewpoint sheared;
  sheared.Up = {{.5, 1, 0}};
  CHECK(Normalise(sheared.Up), "shear control has independently unit length axes");
  CHECK(!ValidMaterialFieldCameraBasis(sheared), "unit lengths do not excuse nonorthogonal axes");
  auto unknown = camera;
  unknown.Kind = static_cast<CameraKind>(99);
  CHECK(!ValidMaterialFieldCameraBasis(unknown), "unknown camera kinds refuse");
  return Report();
}
