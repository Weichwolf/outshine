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
  /* The format's composition, in the format's order: a node's local transform is T * R * S. */
  static Transform FromTrs(const double translation[3], const double rotation[4],
                           const double scale[3]);
  static Transform FromColumnMajor(const double m[16]);

  Transform operator*(const Transform &after) const;

  void Point(const double point[3], double out[3]) const;
  void Direction(const double direction[3], double out[3]) const;

  /* A transform with no inverse is a degenerate node -- a zero scale is the usual cause -- and that
   * is a refusal for the caller to name, never a silently substituted identity. */
  [[nodiscard]] bool Inverse(Transform &out) const;
};

} // namespace outshine::Gltf
#endif
