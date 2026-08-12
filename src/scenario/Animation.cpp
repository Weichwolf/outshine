#include "Animation.h"

#include "CatmullRom.h"

namespace outshine::Scenario {
namespace {

using Ref = Json::Ref;
using JKind = Json::Kind;

struct TargetName {
  const char *Text;
  Animation::Target What;
};

const TargetName kTargets[] = {
    {"camera.eastM", Animation::Target::CameraEastM},
    {"camera.northM", Animation::Target::CameraNorthM},
    {"camera.yawDeg", Animation::Target::CameraYawDeg},
    {"camera.pitchDeg", Animation::Target::CameraPitchDeg},
    {"camera.fovDeg", Animation::Target::CameraFovDeg},
    {"sky.clockS", Animation::Target::SkyClockS},
    {"wind.clockS", Animation::Target::WindClockS},
    {"exposure.compEv", Animation::Target::ExposureCompEv},
};

[[nodiscard]] bool Numbers(const Ref &node, std::vector<double> &out) {
  if (node.GetKind() != JKind::Array) return false;
  out.resize(node.Size());
  for (size_t i = 0; i < node.Size(); i++) {
    if (node[i].GetKind() != JKind::Number) return false;
    out[i] = node[i].Num();
  }
  return !out.empty();
}

[[nodiscard]] bool Increasing(const std::vector<double> &v) {
  for (size_t i = 1; i < v.size(); i++)
    if (!(v[i] > v[i - 1])) return false;
  return true;
}

struct Sampler {
  std::vector<double> Frames, Values;
  Keyframes::Interpolation How = Keyframes::Interpolation::Linear;
};

}  // namespace

const char *Animation::Name(Target t) {
  for (const TargetName &n : kTargets)
    if (n.What == t) return n.Text;
  return "?";
}

/* "animation": { "samplers": [{ "input": [...], "output": [...], "interpolation": "..." }],
 *                "channels": [{ "sampler": 0, "target": "camera.yawDeg" }] }
 * glTF's own two-table shape, so one sampler can drive several properties. `interpolation` is
 * REQUIRED: CUBICSPLINE carries three numbers per keyframe and the others carry one, so a default
 * would have to guess what the output array means. CATMULLROM is ours — glTF has no name for
 * "derive the tangents" because glTF is written by an exporter, and a hand-written scene is not. */
bool Animation::Read(const Ref &node, std::string &err) {
  const Ref samplers = node["samplers"], channels = node["channels"];
  if (samplers.GetKind() != JKind::Array || channels.GetKind() != JKind::Array) {
    err = "animation needs a samplers array and a channels array";
    return false;
  }
  std::vector<Sampler> tracks(samplers.Size());
  for (size_t i = 0; i < samplers.Size(); i++) {
    const Ref s = samplers[i];
    const Ref interp = s["interpolation"];
    bool derive = false;
    if (interp.StrEquals("STEP")) tracks[i].How = Keyframes::Interpolation::Step;
    else if (interp.StrEquals("LINEAR")) tracks[i].How = Keyframes::Interpolation::Linear;
    else if (interp.StrEquals("CUBICSPLINE")) tracks[i].How = Keyframes::Interpolation::CubicSpline;
    else if (interp.StrEquals("CATMULLROM")) {
      tracks[i].How = Keyframes::Interpolation::CubicSpline;
      derive = true;
    } else {
      err = "sampler " + std::to_string(i) +
            ": interpolation must be STEP, LINEAR, CATMULLROM or CUBICSPLINE";
      return false;
    }
    std::vector<double> values;
    if (!Numbers(s["input"], tracks[i].Frames) || !Numbers(s["output"], values) ||
        !Increasing(tracks[i].Frames)) {
      err = "sampler " + std::to_string(i) +
            ": input and output must be non-empty number arrays and input must strictly increase";
      return false;
    }
    const size_t want = tracks[i].Frames.size() *
                        (tracks[i].How == Keyframes::Interpolation::CubicSpline && !derive ? 3u : 1u);
    if (values.size() != want) {
      err = "sampler " + std::to_string(i) + ": output holds " + std::to_string(values.size()) +
            " values for " + std::to_string(tracks[i].Frames.size()) + " keyframes, expected " +
            std::to_string(want);
      return false;
    }
    if (derive) {
      /* The frames ARE the knots: a scalar track over a strictly increasing frame axis cannot loop,
       * so the centripetal reparameterisation that positions need would answer a question this
       * curve does not ask (core/CatmullRom.h). */
      tracks[i].Values.resize(values.size() * 3);
      CatmullRomTangents(tracks[i].Frames.data(), tracks[i].Frames.size(), values.data(), 1,
                         tracks[i].Values.data());
    } else {
      tracks[i].Values = values;
    }
  }

  Channels_.reserve(channels.Size());
  for (size_t i = 0; i < channels.Size(); i++) {
    const Ref c = channels[i];
    const Ref target = c["target"];
    Channel out;
    bool known = false;
    for (const TargetName &n : kTargets)
      if (target.StrEquals(n.Text)) { out.What = n.What; known = true; break; }
    if (!known) {
      err = "channel " + std::to_string(i) + ": unknown target " + target.Str("(missing)");
      return false;
    }
    const int si = c["sampler"].Int(-1);
    if (si < 0 || (size_t)si >= tracks.size()) {
      err = "channel " + std::to_string(i) + ": sampler index out of range";
      return false;
    }
    if (Driver_[(size_t)out.What] >= 0) {
      err = std::string("two channels drive ") + Name(out.What);
      return false;
    }
    out.Frames = tracks[(size_t)si].Frames;
    out.Values = tracks[(size_t)si].Values;
    out.How = tracks[(size_t)si].How;
    Driver_[(size_t)out.What] = (int)Channels_.size();
    Channels_.push_back(std::move(out));
  }
  if (Channels_.empty()) {
    err = "animation declares no channels";
    return false;
  }
  return true;
}

double Animation::At(Target t, double frame) const {
  const int i = Driver_[(size_t)t];
  if (i < 0) return 0.0;
  const Channel &c = Channels_[(size_t)i];
  return Keyframes(c.How, c.Frames.data(), c.Frames.size(), c.Values.data(), 1).AtScalar(frame);
}

}  // namespace outshine::Scenario
