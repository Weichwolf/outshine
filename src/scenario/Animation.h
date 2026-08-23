#ifndef ANIMATION_H
#define ANIMATION_H

#include <string>
#include <vector>

#include "Json.h"
#include "Keyframes.h"

namespace outshine::SceneLegacy {

class Animation {
public:
  enum class Target { CameraEastM, CameraNorthM, CameraYawDeg, CameraPitchDeg, CameraFovDeg,
                      SkyClockS, WindClockS, ExposureCompEv, kCount };

  [[nodiscard]] bool Read(const Json::Ref &node, std::string &err);

  [[nodiscard]] bool Drives(Target t) const { return Driver_[(size_t)t] >= 0; }
  double At(Target t, double frame) const;

  size_t ChannelCount() const { return Channels_.size(); }
  static const char *Name(Target t);

private:

  struct Channel {
    std::vector<double> Frames, Values;
    Keyframes::Interpolation How = Keyframes::Interpolation::Linear;
    Target What = Target::CameraYawDeg;
  };

  std::vector<Channel> Channels_;
  int Driver_[(size_t)Target::kCount] = {-1, -1, -1, -1, -1, -1, -1, -1};
};

}
#endif
