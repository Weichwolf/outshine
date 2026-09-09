#ifndef OUTSHINE_RENDER_CAMERA_H
#define OUTSHINE_RENDER_CAMERA_H

#include <cmath>
#include "math/Mat4.h"
#include "math/Quat.h"
#include "math/Vec3.h"

namespace outshine {
/// Right-handed camera: local +X right, +Y up, -Z viewing direction, metres.
/// Projection matrices use OpenGL NDC depth [-1, 1]; GPU depth conversion is internal.
/// Self-contained value: copies own all data, no allocations or retained references.
/// No thread affinity; concurrent const queries are safe while the value is immutable.
/// Serialize mutation with all access. Matrix queries preserve the output on failure.
struct Camera {
  /// Default perspective near distance in metres when an Engine view declares zero.
  static constexpr double kNearestM = 0.05;

  /// Camera origin in the resolved local world frame, metres; finite coordinates required.
  Vec3 PositionM;
  /// Camera-to-world rotation; finite nonzero quaternion, normalized during matrix construction.
  Quat Orientation;
  /// Vertical perspective FOV in degrees, strictly between 0 and 180.
  /// Engine views resolve zero to kFovUnsaidDeg; explicit matrix queries require a valid FOV.
  double FovDeg = 0.0;

  /// Near plane in metres. Perspective requires positive depth; Engine views resolve zero
  /// to kNearestM. Orthographic near zero is valid; negative and nonfinite values are errors.
  double NearM = 0.0;
  /// Far plane in metres, greater than near. Zero or positive infinity selects infinite
  /// perspective; orthographic projection requires a finite far plane.
  double FarM = 0.0;

  /// Selects orthographic half extents instead of perspective FOV.
  bool Orthographic = false;
  /// Positive finite orthographic horizontal half extent in metres; no inferred default.
  double XMagM = 0.0;
  /// Positive finite orthographic vertical half extent in metres; independent of viewport aspect.
  double YMagM = 0.0;

  /// Perspective lens: a vertical field of view and a depth range.
  struct Perspective {
    /// The vertical field of view, in degrees.
    double FovDeg = 0.0;
    /// The nearest depth the camera keeps, in metres.
    double NearM = 0.0;
    /// The far plane in metres; zero or positive infinity requests infinite perspective.
    double FarM = 0.0;
  };

  /// Orthographic lens: HALF-EXTENTS, never frustum edges.
  ///
  /// The edges are not taken because this camera cannot hold an off-centre frustum, and a setter
  /// that accepted four edges and kept `0.5 * (right - left)` would be answering a different
  /// question than it was asked.
  struct Ortho {
    /// Half the width the camera sees, in metres.
    double XMagM = 0.0;
    /// Half the height, in metres.
    double YMagM = 0.0;
    /// The nearest depth the camera keeps, in metres.
    double NearM = 0.0;
    /// The furthest, in metres.
    double FarM = 0.0;
  };

  /// Store a perspective declaration. Does not validate or retain references.
  /// Engine preparation applies the documented zero defaults and rejects invalid or
  /// GPU-unrepresentable values; matrix queries require an explicit valid lens.
  /// @param sees Owned copy of the perspective lens parameters.
  void setProjection(Perspective sees) noexcept {
    Orthographic = false;
    FovDeg = sees.FovDeg;
    NearM = sees.NearM;
    FarM = sees.FarM;
  }

  /// Store orthographic half extents and depth planes, in metres. Does not validate
  /// or retain references. Both extents must be positive and finite, near >= 0, far > near
  /// and finite; Engine preparation additionally checks GPU representability.
  /// @param sees Owned copy of orthographic lens parameters.
  void setProjection(Ortho sees) noexcept {
    Orthographic = true;
    XMagM = sees.XMagM;
    YMagM = sees.YMagM;
    NearM = sees.NearM;
    FarM = sees.FarM;
  }

  /// Camera-to-world rigid transform; rejects invalid position, rotation or look-at basis.
  /// @param out Receives the camera-to-world matrix, column-major, only on success.
  /// @return False for invalid pose or a numerically unusable basis.
  [[nodiscard]] bool modelMatrix(Mat4 &out) const noexcept;
  /// World-to-camera inverse, including quaternion rotation and roll, or explicit look-at.
  /// @param out Receives a finite world-to-camera matrix only on success.
  /// @return False for invalid pose, singularity or unrepresentable coefficients.
  [[nodiscard]] bool viewMatrix(Mat4 &out) const noexcept;
  /// Lens-only projection; independent of placement and look-at target. Aspect is width/height.
  /// @param aspect Positive finite width/height for perspective; ignored for orthographic.
  /// @param out Receives finite projection coefficients only on success.
  /// @return False for invalid lens parameters or unrepresentable coefficients.
  [[nodiscard]] bool projectionMatrix(double aspect, Mat4 &out) const noexcept;
  /// Compose projection * view, mapping world coordinates to homogeneous clip coordinates.
  /// @param aspect Perspective viewport width/height; ignored for orthographic projection.
  /// @param out Receives the finite composed matrix only on success.
  /// @return False for invalid pose/lens or unrepresentable composition.
  [[nodiscard]] bool clipMatrix(double aspect, Mat4 &out) const noexcept;

  /// An explicit target overrides Orientation. UpM is the world-space look-at up vector.
  bool LooksAt = false;
  /// Finite world-space target in metres, distinct from PositionM when LooksAt is true.
  Vec3 LookAtM;
  /// Finite, nonzero world-space up direction, not parallel to the viewing direction.
  Vec3 UpM = {{0.0, 1.0, 0.0}};

  /// Positive finite aperture f-number; zero leaves photographic exposure unspecified.
  double ApertureFStops = 0.0;
  /// Positive finite shutter duration in seconds; zero leaves exposure unspecified.
  double ShutterS = 0.0;
  /// Positive finite ISO sensitivity; zero leaves exposure unspecified.
  double SensitivityIso = 0.0;

  /// Validate the three photographic inputs without calculating the resulting multiplier.
  /// @return True only when all three parameters are positive and finite.
  [[nodiscard]] bool exposed() const noexcept {
    return ApertureFStops > 0.0 && std::isfinite(ApertureFStops) && ShutterS > 0.0 &&
           std::isfinite(ShutterS) && SensitivityIso > 0.0 && std::isfinite(SensitivityIso);
  }

  /// Photographic exposure parameters.
  struct Exposure {
    /// The aperture, in f-stops.
    double ApertureFStops = 0.0;
    /// The shutter, in seconds.
    double ShutterS = 0.0;
    /// The sensitivity, in ISO.
    double SensitivityIso = 0.0;
  };

  /// Store exposure parameters without retaining references.
  /// @param by Owned copy; validation is deferred to exposed()/exposureScale().
  void setExposure(Exposure by) noexcept {
    ApertureFStops = by.ApertureFStops;
    ShutterS = by.ShutterS;
    SensitivityIso = by.SensitivityIso;
  }

  /// Linear photographic multiplier: shutter * ISO / (120 * aperture squared).
  /// Computes in logarithmic space to avoid intermediate product overflow.
  /// @return Positive finite multiplier; zero for unspecified/invalid inputs or an
  /// unrepresentable result. No clamping and no mutation of the camera.
  [[nodiscard]] double exposureScale() const noexcept;
};

}
#endif
