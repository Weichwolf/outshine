#include "Check.h"
#include "Viewport.h"

#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Gltf;
  using namespace outshine::Test;

  Camera camera;
  camera.Kind = CameraKind::Perspective;
  camera.YfovRad = 1.0;
  camera.ZNearM = 0.5;
  camera.ZFarM = 50.0;

  Mat4 projection;
  CHECK(camera.Projection(2.0, projection), "a finite perspective camera produces a projection");

  const Vec3 point{{1.0, 1.0, -2.0}};
  const double w = projection[3] * point[0] + projection[7] * point[1] + projection[11] * point[2] +
                   projection[15];
  CHECK(std::fabs(w - 2.0) < 1e-12, "a perspective projection retains the homogeneous divisor");
  CHECK(projection[15] == 0.0, "a perspective projection cannot masquerade as affine");
  return Report();
}
