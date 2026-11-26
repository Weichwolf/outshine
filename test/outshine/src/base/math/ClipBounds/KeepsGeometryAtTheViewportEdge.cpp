#include "Box.h"
#include "Check.h"
#include "ClipBounds.h"
#include "Mat4.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Box outside{.Min = {{1.25, -0.5, 0}}, .Max = {{2, 0.5, 1}}};
  CHECK(OutsideViewport(Mat4f{}, outside), "a bound entirely beyond an edge can be skipped");

  const Box touching{.Min = {{1, -0.5, 0}}, .Max = {{2, 0.5, 1}}};
  CHECK(!OutsideViewport(Mat4f{}, touching), "geometry touching the viewport can still contribute");
  const Box wide{.Min = {{-2, -2, 0}}, .Max = {{2, 2, 1}}};
  CHECK(!OutsideViewport(Mat4f{}, wide), "a surrounding bound can cover the full image");

  Mat4f translated;
  translated.At(0, 3) = -6000000.0f;
  const Box uncertain{.Min = {{6000001.2, -0.5, 0}}, .Max = {{6000001.5, 0.5, 1}}};
  CHECK(!OutsideViewport(translated, uncertain), "floating-point uncertainty keeps the draw");
  const Box disjoint{.Min = {{6000100, -0.5, 0}}, .Max = {{6000110, 0.5, 1}}};
  CHECK(OutsideViewport(translated, disjoint), "large coordinates still allow definite rejection");
  const double unknown = std::numeric_limits<double>::infinity();
  const Box unbounded{.Min = {{-unknown, -1, 0}}, .Max = {{unknown, 1, 1}}};
  CHECK(!OutsideViewport(Mat4f{}, unbounded), "unknown bounds remain drawable");
  return Report();
}
