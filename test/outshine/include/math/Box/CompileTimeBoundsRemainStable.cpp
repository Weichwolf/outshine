#include "math/Box.h"
#include "Check.h"

namespace outshine {
constexpr Vec3 kBoxFirstPoint{{1.0, 2.0, 3.0}};
constexpr Vec3 kBoxSecondPoint{{-1.0, 5.0, 0.0}};
constexpr Vec3 kBoxBothMin{{-1.0, 2.0, 0.0}};
constexpr Vec3 kBoxBothMax{{1.0, 5.0, 3.0}};
constexpr Vec3 kBoxBothMiddle{{0.0, 3.5, 1.5}};
constexpr Vec3 kBoxBothSpan{{2.0, 3.0, 3.0}};

constexpr Box BoxOverOnePoint() {
  Box grown;
  grown.Cover(kBoxFirstPoint);
  return grown;
}

constexpr Box BoxOverTwoPoints() {
  Box grown = BoxOverOnePoint();
  grown.Cover(kBoxSecondPoint);
  return grown;
}

static_assert(Box{}.Empty(), "a box nothing has covered is empty");
static_assert(!Box{}.Holds(Vec3{}),
              "an empty box holds nothing -- the infinities must not read as a box over "
              "everything, which is what an unguarded min/max comparison would say");
static_assert(Box{}.Span() == Vec3{} && Box{}.Middle() == Vec3{},
              "an empty box has no span and no middle, and answering with infinity would put a "
              "NaN into whatever divides by it");
static_assert(!BoxOverOnePoint().Empty() && BoxOverOnePoint().Span() == Vec3{},
              "one point makes a box of no span");
static_assert(BoxOverOnePoint().Holds(kBoxFirstPoint), "a box holds the point it covered");
static_assert(BoxOverTwoPoints().Min == kBoxBothMin && BoxOverTwoPoints().Max == kBoxBothMax,
              "a box grows per axis and never as a whole");
static_assert(BoxOverTwoPoints().Middle() == kBoxBothMiddle, "the middle is per axis too");
static_assert(BoxOverTwoPoints().Span() == kBoxBothSpan, "and so is the span");
static_assert(Box{}.HalfArea() == 0.0 && BoxOverOnePoint().HalfArea() == 0.0,
              "a box of no span encloses no area, and an empty one must not answer with infinity "
              "-- a surface-area heuristic divides by this");

constexpr Box BoxThroughShift() {
  Mat4 shifted;
  shifted.SetTranslation(kBoxFirstPoint);
  return BoxOverTwoPoints().Through(shifted);
}

static_assert(Box{}.Through(Mat4{}).Empty(), "an empty box stays empty through any transform");
static_assert(BoxOverTwoPoints().Through(Mat4{}) == BoxOverTwoPoints(),
              "and the identity leaves a box where it stood");
static_assert(BoxThroughShift().Span() == BoxOverTwoPoints().Span(),
              "a pure translation moves a box without growing it -- the growth an axis-aligned box "
              "pays for is ROTATION, and a translation that grew one would mean the corners were "
              "not all transformed the same way");
static_assert(BoxOverTwoPoints().HalfArea() == kBoxBothSpan[0] * kBoxBothSpan[1] +
                                                   kBoxBothSpan[1] * kBoxBothSpan[2] +
                                                   kBoxBothSpan[2] * kBoxBothSpan[0],
              "half the surface area is the sum of the three distinct faces");

}

int main() {
  using namespace outshine::Test;
  CHECK(outshine::Box{}.Empty(), "default bounds retain their empty sentinel");
  return Report();
}
