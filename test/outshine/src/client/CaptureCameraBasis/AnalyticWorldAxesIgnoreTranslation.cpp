#include "src/client/CaptureCameraBasis.h"
#include "Check.h"
#include <cmath>
#include <limits>
#include <type_traits>

static_assert(
    noexcept(outshine::Client::CaptureCameraBasis::Of(std::declval<const outshine::Camera &>())));

int main() {
  using namespace outshine;
  using namespace outshine::Client;
  using namespace outshine::Test;
  constexpr double tolerance = 1e-10;
  for (int sample = 0; sample < 360; ++sample) {
    const double angle = static_cast<double>(sample) * std::acos(-1.0) / 180.0;
    const Vec3 forward{{std::sin(angle), 0, -std::cos(angle)}};
    const Vec3 up{{0, 1, 0}};
    Camera camera;
    camera.PositionM = {{1000, -2000, 3000}};
    camera.LooksAt = true;
    camera.LookAtM = camera.PositionM + forward;
    camera.UpM = up;
    const auto basis = CaptureCameraBasis::Of(camera);
    CHECK(basis.has_value(), "translated analytic circular turn has a valid basis");
    if (!basis) { continue; }
    for (size_t axis = 0; axis < 3; ++axis) {
      CHECK(std::abs(basis->Forward[axis] - forward[axis]) < tolerance,
            "world forward agrees with analytic heading, including wraparound");
      CHECK(std::abs(basis->Up[axis] - up[axis]) < tolerance,
            "translation does not change world up");
    }
  }
  Camera rolled;
  rolled.LooksAt = true;
  rolled.LookAtM = {{0, 0, -1}};
  rolled.UpM = {{1, 0, 0}};
  const auto basis = CaptureCameraBasis::Of(rolled);
  CHECK((basis && basis->Forward == Vec3{{0, 0, -1}} && basis->Up == Vec3{{1, 0, 0}}),
        "roll is preserved independently of forward heading");
  rolled.UpM = {{0, 0, -1}};
  CHECK(!CaptureCameraBasis::Of(rolled), "collinear look-at basis is rejected");
  rolled.LookAtM[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!CaptureCameraBasis::Of(rolled), "nonfinite presented camera is rejected");
  Camera quaternion;
  const auto canonical = CaptureCameraBasis::Of(quaternion);
  CHECK((canonical && canonical->Forward == Vec3{{0, 0, -1}} && canonical->Up == Vec3{{0, 1, 0}}),
        "quaternion camera has the same native world-axis convention");
  return Report();
}
