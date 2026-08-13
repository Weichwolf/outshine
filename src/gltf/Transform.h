/* 4x4 TRANSFORMS IN DOUBLE, COLUMN-MAJOR -- the format's own storage order, so a node's `matrix`
 * array is copied and not transposed.
 *
 * NOT core/Mat4.h, AND THE REASON IS NOT PRECISION ALONE. That one is float and reversed-Z: it
 * encodes what this engine's depth buffer wants. This one encodes what glTF says, which is where a
 * file's meaning is decided, and the two must be free to move apart. The doubles are the second
 * reason: a camera check against an oracle's eighth significant digit has no float answer. */
#ifndef GLTF_TRANSFORM_H
#define GLTF_TRANSFORM_H

namespace outshine::Gltf {

struct Transform {
  /* Column-major: M[column * 4 + row], and M * v takes a column vector. */
  double M[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

  static Transform Identity() { return {}; }
  /* The format's composition, in the format's order: a node's local transform is T * R * S.
   *
   * `rotation` IS NORMALISED FIRST, because the format defines it as a unit quaternion
   * (`Specification.adoc`, "Transformations": *"`rotation` is a unit quaternion value, XYZW"*) and
   * the matrix below is the unit-quaternion formula. Fed a quaternion of norm n the same formula
   * yields R times n^2 -- a scale the node never declared. MEASURED on Khronos's `Cameras`, whose
   * rotation has |q|^2 = 1.0000030625: 3.06 ppm of silent scale, 0.001 px over the subject, and one
   * pixel of the frame decided by it. A quaternion of zero length is left as it arrived, so nothing
   * is invented for a file that cannot mean anything. */
  static Transform FromTrs(const double translation[3], const double rotation[4],
                           const double scale[3]);
  static Transform FromColumnMajor(const double m[16]);

  Transform operator*(const Transform &after) const;

  void Point(const double point[3], double out[3]) const;
  void Direction(const double direction[3], double out[3]) const;

  /* A NORMAL IS NOT A DIRECTION UNDER A NON-UNIFORM SCALE, and it has its own transform because of
   * it: the surface stays perpendicular to `inverse(linear)^T * n` and not to `linear * n`. glTF
   * says the same in its own words (`Specification.adoc`, "Instantiation": normals are transformed
   * by the inverse transpose). Refuses a singular linear part, which is the caller's to name --
   * substituting `Direction` there is exactly the shortcut a flattened sphere renders lit wrong by.
   * NOT NORMALISED: a caller that renormalises anyway would pay for it twice. */
  [[nodiscard]] bool Normal(const double normal[3], double out[3]) const;

  /* The determinant of the linear part, which is the quantity the format's winding rule is stated
   * over: glTF 2.0 Specification.adoc:1734 reverses a triangle's winding where a node's GLOBAL
   * transform has a negative one. The translation row cannot enter it, so this is the 3x3 and not
   * the 4x4 -- for an affine transform the two agree, and for a projective one they do not. */
  [[nodiscard]] double LinearDeterminant() const;

  /* A transform with no inverse is a degenerate node -- a zero scale is the usual cause -- and that
   * is a refusal for the caller to name, never a silently substituted identity. */
  [[nodiscard]] bool Inverse(Transform &out) const;
};

} // namespace outshine::Gltf
#endif
