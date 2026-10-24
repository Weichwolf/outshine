#ifndef OUTSHINE_RENDER_RENDERCONFIGURATION_H
#define OUTSHINE_RENDER_RENDERCONFIGURATION_H

#include <string>
#include <vector>

#include <Extent.h>

namespace outshine::Render {

inline constexpr double kDefaultFrameRateHz = 60.0;
inline constexpr double kDefaultCameraFill = 0.9;

struct ImageRegion {
  double LeftFrac = 0.0;
  double TopFrac = 0.0;
  double WidthFrac = 1.0;
  double HeightFrac = 1.0;

  [[nodiscard]] constexpr bool whole() const noexcept {
    return LeftFrac == 0.0 && TopFrac == 0.0 && WidthFrac == 1.0 && HeightFrac == 1.0;
  }
};

struct ContentSelection {
  std::vector<std::string> MeshParts;
  bool HeightLattice = true;
  bool Instances = true;

  [[nodiscard]] bool operator==(const ContentSelection &) const noexcept = default;
};

struct Configuration {
  bool Declared = false;
  Extent Frame;
  ImageRegion Picture;
  double FrameRateHz = kDefaultFrameRateHz;
  double CameraFill = kDefaultCameraFill;
  double OrbitDegreesPerFrame = 0.0;
  std::vector<std::string> Outputs;
  std::vector<std::string> Stages;
  ContentSelection Content;
  std::string Transfer;
  double Exposure = 0.0;
  std::string Antialiasing;

  std::string Precision;
  bool Audits = false;
};

}

#endif
