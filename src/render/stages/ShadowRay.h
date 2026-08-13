/* THE DEVICE HALF OF `core/TriangleBvh.h`: the same stackless traversal and the same
 * Moller-Trumbore test, emitted as MSL. There are two implementations of one predicate here and
 * that is deliberate for the reason the BRDF states beside its own twin -- the processor half is
 * what a device answer can be checked against on a machine with no device, and
 * `test/shader/AnExactRayAgreesOnBothSides.cpp` runs the two over one subject and one ray set.
 *
 * THE PACKING IS SPLICED AND NEVER RESTATED. The leaf word's shift and mask, the escape sentinel
 * and the node's own field order come out of the C++ constants below, so a change to the structure
 * cannot leave the shader reading the old layout -- there is no second place for the numbers to be
 * written down in.
 *
 * WHY A SHADOW RAY AND NOT A MAP: the oracle's visibility predicate for a delta light is an exact
 * ray, so an exact ray is the only estimator whose disagreement with it is zero rather than bounded
 * (board:0089). What that costs is measured, not assumed. */
#ifndef SHADOWRAY_H
#define SHADOWRAY_H

#include <cstdio>
#include <string>

#include "TriangleBvh.h"

namespace outshine::Render {

/* THE RAY'S OWN START, AS A FRACTION OF THE SUBJECT'S DIAGONAL, so a surface does not shadow
 * itself. DERIVED, not chosen: a coordinate of magnitude `d` carries an f32 rounding of `d * 2^-24`,
 * and the shading point reaches the fragment through an interpolation and a normalisation, so the
 * ray's origin sits some tens of those away from the triangle it left. 2^-14 of the diagonal is
 * 1024 of those roundings -- three orders above the noise, and at a 0.3 m subject it is 18 microns,
 * which is four orders below one shadow-map texel of the map this replaces. */
constexpr float kShadowRayNearFraction = 1.0f / 16384.0f; /* 2^-14 */

[[nodiscard]] inline std::string ShadowRayMsl(void) {
  char constants[512];
  std::snprintf(constants, sizeof constants,
                "constant uint kBvhNoEscape = %uu;\n"
                "constant uint kBvhLeafFirstBits = %uu;\n"
                "constant uint kBvhLeafFirstMask = %uu;\n"
                "constant uint kBvhInterior = %uu;\n",
                kBvhNoEscape, kBvhLeafFirstBits, kBvhLeafFirstMask, kBvhInterior);
  return std::string(constants) + R"(
/* `packed_float3` and not `float3`: a Metal `float3` occupies sixteen bytes and would put every
 * field of the node four bytes past where the host wrote it. */
struct BvhNode { packed_float3 lo; uint escape; packed_float3 hi; uint leaf; };
struct BvhTri { packed_float3 v0; packed_float3 e1; packed_float3 e2; };

/* IS ANYTHING BETWEEN `nearM` AND `farM` ALONG THE RAY. Any hit ends it, which is what makes a
 * shadow query cheaper than a visibility one: there is no nearest to keep and no ordering to keep
 * it in, and that is why the traversal can afford to have no stack.
 *
 * THE SLAB TEST IS THE NaN-TOLERANT ARRANGEMENT. A ray parallel to an axis divides by zero and
 * gives an infinity, and an origin exactly on the slab turns that into a NaN; `min` and `max`
 * return the other operand when one is NaN, so writing the compare this way round makes the
 * degenerate ray miss rather than behave unpredictably. */
static inline bool bvhOccludes(device const BvhNode *nodes, device const BvhTri *tris,
                               float3 originM, float3 direction, float nearM, float farM) {
  float3 inverse = 1.0 / direction;
  uint at = 0u;
  while (at != kBvhNoEscape) {
    device const BvhNode &node = nodes[at];
    float3 first = (float3(node.lo) - originM) * inverse;
    float3 second = (float3(node.hi) - originM) * inverse;
    float3 low = min(first, second);
    float3 high = max(first, second);
    float enter = max(nearM, max(low.x, max(low.y, low.z)));
    float leave = min(farM, min(high.x, min(high.y, high.z)));
    if (enter > leave) { at = node.escape; continue; }
    if (node.leaf == kBvhInterior) { at = at + 1u; continue; }
    uint firstTriangle = node.leaf & kBvhLeafFirstMask;
    uint count = node.leaf >> kBvhLeafFirstBits;
    for (uint which = 0u; which < count; which = which + 1u) {
      device const BvhTri &tri = tris[firstTriangle + which];
      float3 e1 = float3(tri.e1);
      float3 e2 = float3(tri.e2);
      float3 pvec = cross(direction, e2);
      float determinant = dot(e1, pvec);
      /* TWO-SIDED, and a shadow is why: the query asks whether anything is in the way, not which
       * face of it was met, so culling by winding here would let a body wound the other way stop
       * casting. */
      if (fabs(determinant) < 1.0e-20) { continue; }
      float reciprocal = 1.0 / determinant;
      float3 tvec = originM - float3(tri.v0);
      float u = dot(tvec, pvec) * reciprocal;
      if (u < 0.0 || u > 1.0) { continue; }
      float3 qvec = cross(tvec, e1);
      float v = dot(direction, qvec) * reciprocal;
      if (v < 0.0 || u + v > 1.0) { continue; }
      float hit = dot(e2, qvec) * reciprocal;
      if (hit > nearM && hit < farM) { return true; }
    }
    at = node.escape;
  }
  return false;
}
)";
}

} // namespace outshine::Render
#endif
