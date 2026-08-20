/* THE COMPOSITION ORDER AND THE ROTATION SIGN, HELD AGAINST THE EXTENSION AND NOT AGAINST A SECOND
 * COPY OF THE FORMULA (board:1177).
 *
 * `KHR_texture_transform` composes `translation * rotation * scale`, and there are two ways to get it
 * wrong that a picture does not obviously accuse anyone of: the order reversed, and the rotation
 * turned the other way. A test that restated `T*R*S` in C++ and compared it to `UvTransformOf` would
 * agree with BOTH mistakes, because it would be the same sentence twice. So every claim below is a
 * MAPPED POINT, chosen so that the two readings give different answers:
 *
 *   - THE ORDER. At `offset (0.5, 0)` and `scale (2, 2)`, `T*R*S` sends the uv origin to `(0.5, 0)`
 *     -- the offset is applied last and is untouched by the scale. `S*R*T` sends it to `(1.0, 0)`,
 *     because the scale then multiplies the offset. One point separates them by a factor of two.
 *   - THE SIGN, AND IT IS STATED ON THE EXTENSION'S OWN WORKED EXAMPLE RATHER THAN ON THE WORD
 *     "counter-clockwise", because the extension's GLSL snippet and the example three paragraphs
 *     below it disagree and the example is the one that is checkable. `offset [0, 1]`, `rotation
 *     pi/2`, `scale [0.5, 0.5]` is declared to "utilize only the lower left quadrant of the source
 *     image, rotated clockwise 90 degrees", and with the uv origin at the image's UPPER left -- the
 *     extension's own Implementation Note -- the lower left quadrant is `u in [0, 0.5]` and
 *     `v in [0.5, 1]`. The snippet's sign sends the unit square to `u in [-0.5, 0]`, `v in
 *     [1, 1.5]`, which is off the image entirely. THE ASSET IS THE THIRD WITNESS and agrees with the
 *     example: `TextureTransformTest` puts our arrow on its RED marker -- "the rotation was applied
 *     in the opposite direction" -- under the snippet's sign. All of this is stated at a quarter turn
 *     and at 22.5 degrees rather than at 0 or pi, where the two readings AGREE and no claim about a
 *     sign has any power.
 *
 * AND THE IDENTITY IS CHECKED AS A COMPUTATION AND NOT AS AN ABSENCE, because the whole reason the
 * extension needs no branch anywhere is that a reference declaring the extension's own defaults and a
 * reference declaring nothing at all produce the same six numbers. */
#include <cmath>

#include "Check.h"
#include "UvTransform.h"

namespace {

using outshine::UvPoint;
using outshine::UvTransform;
using outshine::UvTransformOf;

/* HALF AN f32 ULP AT 1.0. The composition is one `sin`, one `cos` and two products in double, so
 * anything this test can see at f32's resolution is a wrong FORMULA and not a rounding. */
constexpr double kTolerance = 5.9604644775390625e-08;

void MapsTo(const UvTransform &transform, UvPoint from, double wantU, double wantV,
            const char *claim) {
  const UvPoint got = transform.Apply(from);
  CHECK_NEAR(got.U, wantU, kTolerance, "uv", claim);
  CHECK_NEAR(got.V, wantV, kTolerance, "uv", claim);
}

} // namespace

int main() {
  using namespace outshine::Test;
  Covers("board:1177");

  /* THE DEFAULTS ARE THE IDENTITY, and a default-constructed transform is the same six numbers. */
  const UvTransform absent;
  const UvTransform declaredDefaults = UvTransformOf(outshine::UvTransformProperties{});
  for (size_t element = 0; element < 6; ++element) {
    CHECK(absent.M[element] == declaredDefaults.M[element],
          "a reference that declares no transform and one that declares the extension's own "
          "defaults are the SAME six numbers, which is what makes absence and presence one "
          "computation with no branch");
  }
  MapsTo(absent, UvPoint{0.3, 0.7}, 0.3, 0.7, "the identity moves no coordinate");

  /* THE ORDER. `translation * rotation * scale` leaves the offset unscaled; the reversed product
   * multiplies it by the scale, and the uv origin is where the two part company. */
  outshine::UvTransformProperties offsetAndScale;
  offsetAndScale.OffsetUv[0] = 0.5;
  offsetAndScale.ScaleUv[0] = 2.0;
  offsetAndScale.ScaleUv[1] = 2.0;
  MapsTo(UvTransformOf(offsetAndScale), UvPoint{0.0, 0.0}, 0.5, 0.0,
         "translation is applied LAST, so the uv origin lands on the declared offset itself -- the "
         "reversed product S*R*T would put it at 1.0, the offset times the scale");
  MapsTo(UvTransformOf(offsetAndScale), UvPoint{0.25, 0.5}, 1.0, 1.0,
         "and everything else is scaled first and then shifted by the same unscaled offset");

  /* THE SIGN, ON THE EXTENSION'S OWN WORKED EXAMPLE: the unit square onto the LOWER LEFT quadrant,
   * which with the uv origin at the image's upper left is u in [0, 0.5] and v in [0.5, 1]. This is
   * the claim the extension's GLSL snippet fails, and the only one of the two readings that puts the
   * result inside the image at all. */
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

  /* THE TURN ALONE, SO THE SIGN IS ALSO STATED WITHOUT AN OFFSET OR A SCALE IN THE WAY. */
  outshine::UvTransformProperties quarterTurn;
  quarterTurn.RotationRad = std::acos(-1.0) / 2.0;
  MapsTo(UvTransformOf(quarterTurn), UvPoint{1.0, 0.0}, 0.0, -1.0,
         "a quarter turn sends (1, 0) to (0, -1) -- with v downward that is the coordinate turning "
         "counter-clockwise on screen, and the image it samples turning clockwise, which is the "
         "extension's sentence in its own frame");
  MapsTo(UvTransformOf(quarterTurn), UvPoint{0.0, 1.0}, 1.0, 0.0,
         "and sends (0, 1) to (1, 0), which is the same turn read on the other axis");

  /* AND THE EXTENSION'S SECOND EXAMPLE, WHICH HAS NO ROTATION AT ALL and therefore pins the ORDER
   * alone: "inverts the T axis, effectively defining a bottom-left origin" at offset (0,1), scale
   * (1,-1). Under the reversed product the offset would be scaled too and v' would be -v - 1. */
  outshine::UvTransformProperties invertedT;
  invertedT.OffsetUv[1] = 1.0;
  invertedT.ScaleUv[1] = -1.0;
  MapsTo(UvTransformOf(invertedT), UvPoint{0.25, 0.25}, 0.25, 0.75,
         "offset (0,1) with scale (1,-1) inverts the T axis, which is the extension's own second "
         "example and is v' = 1 - v -- the reversed product gives -v - 1 and leaves the image");

  /* THE ROTATION IS ABOUT THE ORIGIN AND NOT ABOUT THE CENTRE OF THE IMAGE, which is the third
   * plausible reading and the one that leaves (0.5, 0.5) fixed. */
  MapsTo(UvTransformOf(quarterTurn), UvPoint{0.0, 0.0}, 0.0, 0.0,
         "the uv ORIGIN is the fixed point of a pure rotation, so an implementation that turned "
         "about the image's centre would move it");

  /* AND THE ASSET'S OWN NUMBERS, so the claim is stated on the values a corpus case will carry
   * rather than only on the ones this test chose. `TextureTransformTest`'s "All" plate declares
   * offset (-0.2, -0.1), rotation 0.3 rad and scale (1.5, 1.5). */
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
