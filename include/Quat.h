#ifndef OUTSHINE_QUAT_H
#define OUTSHINE_QUAT_H

namespace outshine {

/// A rotation, held as a unit quaternion.
///
/// The components are NAMED rather than indexed, because the two orders in use -- `xyzw` at the
/// door and `wxyz` in the solver -- are indistinguishable as an array and a swapped index is a
/// silent wrong rotation. Naming them makes the translation between the two orders readable at
/// the one place it happens.
struct Quat {
  /// The x component of the vector part.
  double X = 0.0;
  /// The y component of the vector part.
  double Y = 0.0;
  /// The z component of the vector part.
  double Z = 0.0;
  /// The scalar part; a quaternion that turns nothing is (0, 0, 0, 1).
  double W = 1.0;
};

} // namespace outshine

#endif
