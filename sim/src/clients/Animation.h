#ifndef ANIMATION_H
#define ANIMATION_H

#include <string>
#include <vector>

#include "Json.h"
#include "Keyframes.h"

namespace outshine::Clients {

/* MOVEMENT IN A SCENE, in glTF's shape: a channel points a sampler at a property, a sampler is
 * keyframes plus values plus an interpolation. Khronos already solved "how does a human author a
 * curve and a machine read it", and a bespoke format here would be the parser nobody ordered
 * (Prinzip 1). This class is the READER; the evaluation is core/Keyframes.h, because a joint, a
 * door and an entity on a path need the same arithmetic and none of them will read JSON.
 *
 * TWO DELIBERATE DEVIATIONS FROM glTF:
 *
 *  1. KEYFRAMES ARE FRAMES, NOT SECONDS (core/Keyframes.h says why). The run declares `fps` and
 *     seconds are derived, so "a full turn in six seconds" is 360 frames at 60 fps.
 *  2. THE TARGETS ARE OURS. glTF animates a node's local translation/rotation/scale; our camera is
 *     latitude/longitude/height plus azimuth/elevation/field of view on a sphere, where a local TRS
 *     would be a lie. A channel names one of OUR properties, in that property's own unit and frame
 *     of reference:
 *
 *       camera.eastM / camera.northM   metres from the scene's declared standpoint
 *       camera.yawDeg / camera.pitchDeg / camera.fovDeg   absolute degrees
 *       sky.clockS                     seconds from the scene's declared utc — the sun moves
 *       wind.clockS                    absolute seconds of the flow clock
 *       exposure.compEv                absolute stops of compensation
 *
 * The channel/sampler pair being the ground form is the point: driving the time of day is the same
 * mechanism as turning the camera, so the next animatable quantity needs no second mechanism. */
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
  /* The arrays are OWNED here and the evaluator is constructed over them at the point of use:
   * core/Keyframes is a view, and a view stored beside a vector that may reallocate is a dangling
   * pointer waiting for the next channel to be pushed. */
  struct Channel {
    std::vector<double> Frames, Values;
    Keyframes::Interpolation How = Keyframes::Interpolation::Linear;
    Target What = Target::CameraYawDeg;
  };

  std::vector<Channel> Channels_;
  int Driver_[(size_t)Target::kCount] = {-1, -1, -1, -1, -1, -1, -1, -1};
};

} // namespace outshine::Clients
#endif
