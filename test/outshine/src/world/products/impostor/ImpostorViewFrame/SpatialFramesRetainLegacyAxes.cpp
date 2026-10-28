#include "ImpostorViewFrame.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double diagonal = 1 / std::numbers::sqrt3;
  const std::array directions{Vec3{{0, 1, 0}},
                              Vec3{{0, -1, 0}},
                              Vec3{{1, 0, 0}},
                              Vec3{{-1, 0, 0}},
                              Vec3{{0, 0, 1}},
                              Vec3{{0, 0, -1}},
                              Vec3{{diagonal, diagonal, diagonal}},
                              Vec3{{1e-100, 1, 0}}};
  for (const auto &direction : directions) {
    const auto frame = ImpostorViewFrame::From(direction);
    CHECK(frame.has_value(), "unit directions include poles and sloped views");
    if (!frame) { continue; }
    CHECK(std::abs(Dot(frame->Right, frame->Right) - 1) < 2e-12 &&
              std::abs(Dot(frame->Up, frame->Up) - 1) < 2e-12 &&
              std::abs(Dot(frame->Right, frame->Up)) < 2e-12 &&
              std::abs(Dot(frame->Right, direction)) < 2e-12 &&
              std::abs(Dot(frame->Up, direction)) < 2e-12,
          "screen axes are finite orthonormal axes");
    const auto normal = Cross(frame->Right, frame->Up);
    CHECK(Dot(normal, direction) > 1 - 2e-12, "card winding faces the capture eye");
    if (direction[1] == 0) {
      CHECK(frame->Right == Vec3({{direction[2], 0, -direction[0]}}) &&
                frame->Up == Vec3({{0, 1, 0}}),
            "horizontal frames retain their exact old axes");
    }
  }
  for (const auto &bad : {Vec3{},
                          Vec3{{0, 2, 0}},
                          Vec3{{0, std::numeric_limits<double>::infinity(), 0}},
                          Vec3{{std::numeric_limits<double>::quiet_NaN(), 0, 1}}}) {
    CHECK(!ImpostorViewFrame::From(bad), "invalid direction has no frame");
  }
  return Report();
}
