#include <cmath>

#include "Check.h"
#include "UvTransform.h"

namespace {

using outshine::UvPoint;
using outshine::UvTransform;
using outshine::UvTransformOf;

constexpr double kTolerance = 5.9604644775390625e-08;

void MapsTo(const UvTransform &transform, UvPoint from, double wantU, double wantV,
            const char *claim) {
  const UvPoint got = transform.Apply(from);
  CHECK_NEAR(got.U, wantU, kTolerance, "uv", claim);
  CHECK_NEAR(got.V, wantV, kTolerance, "uv", claim);
}

}

int main() {
  using namespace outshine::Test;
  Covers("board:1177");

  const UvTransform absent;
  const UvTransform declaredDefaults = UvTransformOf(outshine::UvTransformProperties{});
  for (size_t element = 0; element < 6; ++element) {
    CHECK(absent.M[element] == declaredDefaults.M[element],
          "a reference that declares no transform and one that declares the extension's own "
          "defaults are the SAME six numbers, which is what makes absence and presence one "
          "computation with no branch");
  }
  MapsTo(absent, UvPoint{0.3, 0.7}, 0.3, 0.7, "the identity moves no coordinate");

  outshine::UvTransformProperties offsetAndScale;
  offsetAndScale.OffsetUv[0] = 0.5;
  offsetAndScale.ScaleUv[0] = 2.0;
  offsetAndScale.ScaleUv[1] = 2.0;
  MapsTo(UvTransformOf(offsetAndScale), UvPoint{0.0, 0.0}, 0.5, 0.0,
         "translation is applied LAST, so the uv origin lands on the declared offset itself -- the "
         "reversed product S*R*T would put it at 1.0, the offset times the scale");
  MapsTo(UvTransformOf(offsetAndScale), UvPoint{0.25, 0.5}, 1.0, 1.0,
         "and everything else is scaled first and then shifted by the same unscaled offset");

  outshine::UvTransformProperties lowerLeftQuadrant;
  lowerLeftQuadrant.OffsetUv[1] = 1.0;
  lowerLeftQuadrant.RotationRad = std::acos(-1.0) / 2.0;
  lowerLeftQuadrant.ScaleUv[0] = 0.5;
  lowerLeftQuadrant.ScaleUv[1] = 0.5;
  const UvTransform quadrant = UvTransformOf(lowerLeftQuadrant);
  MapsTo(quadrant, UvPoint{0.0, 0.0}, 0.0, 1.0,
         "offset (0,1), rotation pi/2, scale (0.5,0.5): the unit square's upper-left corner lands "
         "on the LOWER left of the image, which is the corner the extension's own example names");
  MapsTo(quadrant, UvPoint{1.0, 0.0}, 0.0, 0.5,
         "and its upper-right corner on the quadrant's upper-left, which is the quarter turn");
  MapsTo(quadrant, UvPoint{0.0, 1.0}, 0.5, 1.0,
         "and its lower-left corner on the quadrant's lower-right");
  MapsTo(quadrant, UvPoint{1.0, 1.0}, 0.5, 0.5,
         "and its lower-right corner on the quadrant's upper-right -- so the four corners cover "
         "exactly u in [0, 0.5] and v in [0.5, 1]. The GLSL snippet's sign covers u in [-0.5, 0] "
         "and v in [1, 1.5], which is off the image and not a quadrant of anything");

  outshine::UvTransformProperties quarterTurn;
  quarterTurn.RotationRad = std::acos(-1.0) / 2.0;
  MapsTo(UvTransformOf(quarterTurn), UvPoint{1.0, 0.0}, 0.0, -1.0,
         "a quarter turn sends (1, 0) to (0, -1) -- with v downward that is the coordinate turning "
         "counter-clockwise on screen, and the image it samples turning clockwise, which is the "
         "extension's sentence in its own frame");
  MapsTo(UvTransformOf(quarterTurn), UvPoint{0.0, 1.0}, 1.0, 0.0,
         "and sends (0, 1) to (1, 0), which is the same turn read on the other axis");

  outshine::UvTransformProperties invertedT;
  invertedT.OffsetUv[1] = 1.0;
  invertedT.ScaleUv[1] = -1.0;
  MapsTo(UvTransformOf(invertedT), UvPoint{0.25, 0.25}, 0.25, 0.75,
         "offset (0,1) with scale (1,-1) inverts the T axis, which is the extension's own second "
         "example and is v' = 1 - v -- the reversed product gives -v - 1 and leaves the image");

  MapsTo(UvTransformOf(quarterTurn), UvPoint{0.0, 0.0}, 0.0, 0.0,
         "the uv ORIGIN is the fixed point of a pure rotation, so an implementation that turned "
         "about the image's centre would move it");

  outshine::UvTransformProperties all;
  all.OffsetUv[0] = -0.2;
  all.OffsetUv[1] = -0.1;
  all.RotationRad = 0.3;
  all.ScaleUv[0] = 1.5;
  all.ScaleUv[1] = 1.5;
  const UvTransform composed = UvTransformOf(all);
  MapsTo(composed, UvPoint{0.0, 0.0}, -0.2, -0.1,
         "TextureTransformTest's 'All' plate sends the uv origin to its declared offset alone");
  MapsTo(composed, UvPoint{1.0, 0.0}, 1.5 * std::cos(0.3) - 0.2, -1.5 * std::sin(0.3) - 0.1,
         "and (1, 0) to the scaled unit vector turned by 0.3 rad and then shifted, which is the "
         "only one of the six orderings that produces this pair");

  Note("the composed row of TextureTransformTest's 'All' plate, m00", composed.M[0], "uv per uv");
  Note("the composed row of TextureTransformTest's 'All' plate, m01", composed.M[1], "uv per uv");
  Note("the composed row of TextureTransformTest's 'All' plate, m10", composed.M[3], "uv per uv");
  Note("the composed row of TextureTransformTest's 'All' plate, m11", composed.M[4], "uv per uv");
  return Report();
}
