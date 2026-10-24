#ifndef OUTSHINE_SCENE_CAMERA_H
#define OUTSHINE_SCENE_CAMERA_H

#include <cmath>
#include <expected>
#include "Mat4.h"
#include "Quat.h"
#include "Vec3.h"

namespace outshine {

enum class CameraMatrixError { InvalidPose, InvalidLens, Unrepresentable };

struct Camera {

  static constexpr double kNearestM = 0.05;

  Vec3 PositionM;

  Quat Orientation;

  double FovDeg = 0.0;

  double NearM = 0.0;

  double FarM = 0.0;

  bool Orthographic = false;

  double XMagM = 0.0;

  double YMagM = 0.0;

  struct Perspective {

    double FovDeg = 0.0;

    double NearM = 0.0;

    double FarM = 0.0;
  };

  struct Ortho {

    double XMagM = 0.0;

    double YMagM = 0.0;

    double NearM = 0.0;

    double FarM = 0.0;
  };

  void setProjection(Perspective sees) noexcept {
    Orthographic = false;
    FovDeg = sees.FovDeg;
    NearM = sees.NearM;
    FarM = sees.FarM;
  }

  void setProjection(Ortho sees) noexcept {
    Orthographic = true;
    XMagM = sees.XMagM;
    YMagM = sees.YMagM;
    NearM = sees.NearM;
    FarM = sees.FarM;
  }

  [[nodiscard]] std::expected<void, CameraMatrixError> modelMatrix(Mat4 &out) const noexcept;

  [[nodiscard]] std::expected<void, CameraMatrixError> viewMatrix(Mat4 &out) const noexcept;

  [[nodiscard]] std::expected<void, CameraMatrixError> projectionMatrix(double aspect,
                                                                        Mat4 &out) const noexcept;

  [[nodiscard]] std::expected<void, CameraMatrixError> clipMatrix(double aspect,
                                                                  Mat4 &out) const noexcept;

  bool LooksAt = false;

  Vec3 LookAtM;

  Vec3 UpM = {{0.0, 1.0, 0.0}};

  double ApertureFStops = 0.0;

  double ShutterS = 0.0;

  double SensitivityIso = 0.0;

  [[nodiscard]] bool exposed() const noexcept {
    return ApertureFStops > 0.0 && std::isfinite(ApertureFStops) && ShutterS > 0.0 &&
           std::isfinite(ShutterS) && SensitivityIso > 0.0 && std::isfinite(SensitivityIso);
  }

  struct Exposure {

    double ApertureFStops = 0.0;

    double ShutterS = 0.0;

    double SensitivityIso = 0.0;
  };

  void setExposure(Exposure by) noexcept {
    ApertureFStops = by.ApertureFStops;
    ShutterS = by.ShutterS;
    SensitivityIso = by.SensitivityIso;
  }

  [[nodiscard]] double exposureScale() const noexcept;
};

}
#endif
