#ifndef OUTSHINE_RENDER_RENDERCONFIGURATION_H
#define OUTSHINE_RENDER_RENDERCONFIGURATION_H

#include <string>
#include <vector>

#include <Extent.h>

namespace outshine::Render {

inline constexpr double kDefaultFrameRateHz = 60.0;
inline constexpr double kDefaultCameraFill = 0.9;

/// Normalized image region measured from the target's upper-left corner.
struct ImageRegion {
  double LeftFrac = 0.0;   ///< Left edge divided by target width.
  double TopFrac = 0.0;    ///< Top edge divided by target height, increasing downward.
  double WidthFrac = 1.0;  ///< Width divided by target width.
  double HeightFrac = 1.0; ///< Height divided by target height.

  /// @return Whether the region exactly spans the target.
  [[nodiscard]] constexpr bool whole() const noexcept {
    return LeftFrac == 0.0 && TopFrac == 0.0 && WidthFrac == 1.0 && HeightFrac == 1.0;
  }
};

/// Owned render request shared by direct clients and scenario import.
struct Configuration {
  bool Declared = false; ///< Whether this request participates in declaration merging.
  Extent Frame;          ///< Requested image dimensions; the host supplies the actual target.
  ImageRegion Picture;   ///< Region of the target occupied by the rendered picture.
  double FrameRateHz = kDefaultFrameRateHz; ///< Sampling rate, not a pacing guarantee.
  double CameraFill = kDefaultCameraFill;   ///< Framing fill; nonpositive selects engine defaults.
  double OrbitDegreesPerFrame = 0.0;        ///< Automatic orbit increment; zero disables orbiting.
  std::vector<std::string> Outputs;         ///< Additional native render resources to retain.
  std::vector<std::string> Stages; ///< Explicit native stages replacing automatic selection.
  std::string Transfer;            ///< Empty selects the default; otherwise `linear` or `filmic`.
  double Exposure = 0.0;           ///< Positive linear exposure; nonpositive selects metering.
  std::string Precision;           ///< Empty selects the default; otherwise `half` or `float`.
  bool Audits = false;             ///< Enable CPU mesh-quality diagnostics.
};

}

#endif
