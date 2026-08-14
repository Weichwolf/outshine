/* WHERE ONE TEXTURE REFERENCE READS ITS IMAGE: the affine map applied to the COORDINATE, carried as
 * a matrix and never as the three numbers a file states it with.
 *
 * IT IS COMPOSED ONCE, WHERE THE FILE IS READ, AND THAT IS THE WHOLE DESIGN. glTF's
 * `KHR_texture_transform` composes translation x rotation x scale in that order; a consumer that
 * carried offset, rotation and scale would compose them again at every site that samples, and the
 * reversed order is a picture whose offset has been scaled and rotated. Composed here, THE ORDER HAS
 * NO SPELLING DOWNSTREAM -- there is no sequence in a shader to get wrong -- and it costs the
 * fragment a per-texel `sin` and `cos` it would otherwise pay.
 *
 * THE DEFAULT IS THE IDENTITY, so a reference that declares no transform and one that declares the
 * extension's own defaults are ONE COMPUTATION: no flag, no branch, no second shader arm and no new
 * pipeline permutation. A `bool HasTransform` beside these numbers would be a field that can
 * disagree with them.
 *
 * ROW-MAJOR, TWO ROWS OF THREE, and the third row of the 3x3 is `(0, 0, 1)` for every affine map and
 * is not stored: `u' = M[0]*u + M[1]*v + M[2]` and `v' = M[3]*u + M[4]*v + M[5]`.
 *
 * IN DOUBLE, because it is composed where the reader works and narrowed once at the device boundary
 * -- the same shape a light's ECEF position already has. */
#ifndef UVTRANSFORM_H
#define UVTRANSFORM_H

#include <cmath>

namespace outshine {

/* A TEXTURE COORDINATE AS A VALUE, so that a map from one to another cannot be spelled with its two
 * components swapped (`F.21`). It is `UvPoint` and not `Uv` because three types in this tree already
 * carry a member of that name and a reader would have to know which one a bare `Uv` meant (`NL.19`). */
struct UvPoint {
  double U = 0.0;
  double V = 0.0;
};

/* board:1177 */
struct UvTransform {
  double M[6] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};

  [[nodiscard]] UvPoint Apply(UvPoint uv) const {
    return UvPoint{M[0] * uv.U + M[1] * uv.V + M[2], M[3] * uv.U + M[4] * uv.V + M[5]};
  }
};

/* glTF's `KHR_texture_transform` AS THE FILE STATES IT, at the extension's own defaults, so that
 * absence is spelled by this object rather than by a sentinel. It is a parameter object because
 * `OffsetUv` and `ScaleUv` are the same type and mean opposite things (`I.24`): passed positionally
 * they could be swapped, and the compiler would have nothing to say. */
struct UvTransformProperties {
  double OffsetUv[2] = {0.0, 0.0}; /* factors of the texture's own dimensions */
  double RotationRad = 0.0;        /* counter-clockwise about the uv origin */
  double ScaleUv[2] = {1.0, 1.0};  /* dimensionless */
};

/* TRANSLATION x ROTATION x SCALE, in the extension's own order: the coordinate is scaled first, then
 * turned about the uv origin, then shifted -- so the offset is NOT scaled and NOT rotated, which is
 * the half the reversed product gets wrong.
 *
 * THE ROTATION'S SIGN IS TAKEN FROM THE EXTENSION'S WORKED EXAMPLE AND NOT FROM ITS GLSL SNIPPET,
 * AND THE TWO DISAGREE. The snippet spells `rotation = mat3(cos r, sin r, 0, -sin r, cos r, 0,
 * 0,0,1)`, which in GLSL's column-major constructor is the operator `[[cos, -sin], [sin, cos]]`.
 * Applied to the example three paragraphs below it in the same document -- offset `[0, 1]`, rotation
 * `pi/2`, scale `[0.5, 0.5]`, which the extension says "utilizes only the lower left quadrant of the
 * source image, rotated clockwise 90 degrees" -- that operator sends the unit square to
 * `u in [-0.5, 0]`, `v in [1, 1.5]`, which is not the lower left quadrant and is not inside the
 * image at all. The OTHER sign sends it to `u in [0, 0.5]`, `v in [0.5, 1]`, which IS the lower left
 * quadrant under the extension's own Implementation Note that "(0, 0) corresponds to the upper left
 * corner of a texture image". The prose agrees with the example and not with the snippet, once it is
 * read in that frame: v points DOWN, so a coordinate turned counter-clockwise ON SCREEN is
 * `[[cos, sin], [-sin, cos]]`, and the image it samples then turns clockwise -- "Rotate the UVs by
 * this many radians counter-clockwise around the origin. This is equivalent to a similar rotation of
 * the image clockwise", verbatim.
 *
 * AND THE ASSET IS THE THIRD WITNESS, which is why this is a measurement rather than a reading.
 * `TextureTransformTest` draws a green marker where the arrow lands under the correct rotation and a
 * RED one where it lands under the opposite; the snippet's sign puts our arrow on the red marker,
 * MEASURED, 25 715 pixels apart from the oracle. A sign flip is invisible at rotation 0 and at pi,
 * which is why that asset declares 22.5 degrees. */
[[nodiscard]] inline UvTransform UvTransformOf(const UvTransformProperties &declared) {
  const double turn = std::cos(declared.RotationRad);
  const double lift = std::sin(declared.RotationRad);
  UvTransform composed;
  composed.M[0] = declared.ScaleUv[0] * turn;
  composed.M[1] = declared.ScaleUv[1] * lift;
  composed.M[2] = declared.OffsetUv[0];
  composed.M[3] = -declared.ScaleUv[0] * lift;
  composed.M[4] = declared.ScaleUv[1] * turn;
  composed.M[5] = declared.OffsetUv[1];
  return composed;
}

} // namespace outshine
#endif
