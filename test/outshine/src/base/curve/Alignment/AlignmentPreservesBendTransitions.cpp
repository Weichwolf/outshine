#include "src/base/curve/Alignment.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (double direction : {-1.0, 1.0}) {
    const std::array<double, 6> points{0, 0, 100, 0, 100, direction * 100};
    auto fitted = Align(points, 10, 1);
    CHECK(fitted && fitted->Bends.size() == 1, "right-angle route fits one bend");
    if (!fitted || fitted->Bends.size() != 1) { continue; }
    ReferenceLine line;
    const auto laid = LayAligned(points, *fitted, line);
    CHECK(laid.has_value(), "transitioned bend joins its straight approaches");
    if (!laid) { continue; }
    Placed start, end;
    CHECK(line.At(0, start) && line.At(line.LengthM(), end), "route endpoints evaluate");
    if (std::hypot(end.EastM - 100, end.NorthM - direction * 100) >= 1.0e-3) {
      const Bend &bend = fitted->Bends[0];
      std::fprintf(stderr,
                   "endpoint %.12g %.12g; radius %.12g spiral %.12g arc %.12g tangent %.12g\n",
                   end.EastM,
                   end.NorthM,
                   bend.RadiusM,
                   bend.SpiralM,
                   bend.ArcM,
                   bend.TangentM);
    }
    CHECK(std::hypot(end.EastM - 100, end.NorthM - direction * 100) < 1.0e-3,
          "assembled bend reaches the declared endpoint within one millimetre");
    CHECK(std::abs(start.HeadingRad) < 1.0e-10 &&
              std::abs(end.HeadingRad - direction * std::numbers::pi / 2) < 1.0e-10,
          "both turn directions preserve approach headings");
    const double length = line.LengthM();
    fitted->Bends[0].SpiralM = 0;
    CHECK(!LayAligned(points, *fitted, line), "missing transition is refused");
    CHECK(line.LengthM() == length, "refused transition preserves the prior route");
  }
  return Report();
}
