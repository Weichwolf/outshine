#ifndef OUTSHINE_RENDER_CAMERA_H
#define OUTSHINE_RENDER_CAMERA_H

#include "math/Mat4.h"
#include "math/Quat.h"
#include "math/Vec3.h"

namespace outshine {
/// Right-handed camera: local +X right, +Y up, -Z viewing direction, metres.
/// Projection matrices use OpenGL NDC depth [-1, 1]; GPU depth conversion is internal.
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
  void setProjection(Perspective sees) noexcept {
    Orthographic = false;
    FovDeg = sees.FovDeg;
    NearM = sees.NearM;
    FarM = sees.FarM;
  }

  /// Store orthographic half extents and depth planes, in metres. Does not validate
  /// or retain references. Both extents must be positive and finite, near >= 0, far > near
  /// and finite; Engine preparation additionally checks GPU representability.
  void setProjection(Ortho sees) noexcept {
    Orthographic = true;
    XMagM = sees.XMagM;
    YMagM = sees.YMagM;
    NearM = sees.NearM;
    FarM = sees.FarM;
  }

  /// Camera-to-world rigid transform; rejects invalid position, rotation or look-at basis.
  [[nodiscard]] bool modelMatrix(Mat4 &out) const;
  /// World-to-camera inverse, including quaternion rotation and roll, or explicit look-at.
  [[nodiscard]] bool viewMatrix(Mat4 &out) const;
  /// Lens-only projection; independent of placement and look-at target. Aspect is width/height.
  [[nodiscard]] bool projectionMatrix(double aspect, Mat4 &out) const;
  [[nodiscard]] bool clipMatrix(double aspect, Mat4 &out) const;

  /// An explicit target overrides Orientation. UpM is the world-space look-at up vector.
  bool LooksAt = false;
  Vec3 LookAtM;
  Vec3 UpM = {{0.0, 1.0, 0.0}};

  double ApertureFStops = 0.0;
  double ShutterS = 0.0;
  double SensitivityIso = 0.0;

  [[nodiscard]] bool exposed() const {
    return ApertureFStops > 0.0 && ShutterS > 0.0 && SensitivityIso > 0.0;
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
  void setExposure(Exposure by) {
    ApertureFStops = by.ApertureFStops;
    ShutterS = by.ShutterS;
    SensitivityIso = by.SensitivityIso;
  }

  [[nodiscard]] double exposureScale() const;
};

}
#endif
