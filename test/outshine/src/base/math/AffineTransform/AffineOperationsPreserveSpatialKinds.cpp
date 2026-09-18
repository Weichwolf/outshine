#include "AffineTransform.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  const AffineTransform translated = AffineTransform::FromTrs({{10, 20, 30}}, Quat{}, {{1, 1, 1}});
  const AffineTransform scaled = AffineTransform::FromTrs({}, Quat{}, {{2, 3, 4}});
  const AffineTransform composed = translated * scaled;
  Vec3 point;
  Vec3 direction;
  composed.Point({{1, 1, 1}}, point);
  composed.Direction({{1, 1, 1}}, direction);
  CHECK((point == Vec3{{12, 23, 34}}), "composition applies scale before translation to a point");
  CHECK((direction == Vec3{{2, 3, 4}}), "a direction excludes translation");
  CHECK(composed.LinearDeterminant() == 24, "linear determinant carries the composed scale");

  AffineTransform inverse;
  CHECK(composed.Inverse(inverse), "a nonsingular affine transform has an inverse");
  Vec3 restored;
  inverse.Point(point, restored);
  CHECK((restored == Vec3{{1, 1, 1}}), "inverse restores a transformed point");

  Vec3 normal;
  CHECK((scaled.Normal({{1, 0, 0}}, normal) && normal == Vec3{{0.5, 0, 0}}),
        "normal transformation uses inverse transpose without translation");
  const AffineTransform singular = AffineTransform::FromTrs({}, Quat{}, {{1, 0, 1}});
  CHECK(!singular.Inverse(inverse) && !singular.Normal({{0, 1, 0}}, normal),
        "singular transforms reject inverse and normal publication");
  return Report();
}
