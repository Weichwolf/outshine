#ifndef OUTSHINE_QUAT_H
#define OUTSHINE_QUAT_H

namespace outshine {

/// A rotation, held as a unit quaternion.
///
/// The components are NAMED rather than indexed. As arrays these were two incompatible layouts --
/// `xyzw` at the door, `wxyz` in the solver -- and the engine copied a rotation across that gap by
/// shuffling four indices, which is a silent wrong rotation waiting for a typo. Named, there is no
/// gap to cross: the same four components, and a scenario's facing assigns straight onto a body.
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
