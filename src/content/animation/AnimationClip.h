#ifndef OUTSHINE_CONTENT_ANIMATION_ANIMATIONCLIP_H
#define OUTSHINE_CONTENT_ANIMATION_ANIMATIONCLIP_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "AffineTransform.h"
#include "AnimationCurve.h"
#include "math/Mat4.h"
#include "math/Quat.h"
#include "math/Vec3.h"
#include "math/Vec4.h"

namespace outshine {

enum class AnimationTarget : uint8_t {
  Translation,
  Rotation,
  Scale,
  MorphWeights,
  BaseColour,
  Metalness,
  Roughness,
  Emission
};

struct AnimationRestPose {
  Vec3 Translation;
  Quat Rotation;
  Vec3 Scale = {{1, 1, 1}};
  bool HasMatrix = false;
  Mat4 Matrix;
  size_t WeightFirst = 0;
  size_t WeightCount = 0;
};

struct AnimationTrack {
  int Target = -1;
  AnimationTarget Property = AnimationTarget::Translation;
  std::vector<double> Times;
  std::vector<double> Values;
  AnimationCurve Curve;
  double EmissionScale = 1.0;
};

struct AnimatedMaterialSample {
  int Material = -1;
  AnimationTarget Property = AnimationTarget::BaseColour;
  Vec4 Values;
};

struct AnimationTimeRange {
  double StartS = 0;
  double EndS = 0;
};

class AnimationClip {
public:
  [[nodiscard]] bool Valid() const { return !Nodes_.empty(); }

  [[nodiscard]] double StartS() const { return StartS_; }

  [[nodiscard]] double EndS() const { return EndS_; }

  [[nodiscard]] size_t TrackCount() const { return Tracks_.size(); }

  [[nodiscard]] size_t NodeCount() const { return Nodes_.size(); }

  [[nodiscard]] size_t WeightCount() const { return RestWeights_.size(); }

  void Adopt(std::vector<AnimationRestPose> &&nodes,
             std::vector<double> &&weights,
             std::vector<AnimationTrack> &&tracks,
             AnimationTimeRange range);
  void SamplePose(double seconds,
                  std::vector<AffineTransform> &locals,
                  std::vector<double> &weights) const;
  void SampleMaterials(double seconds, std::vector<AnimatedMaterialSample> &samples) const;

private:
  struct TrackRange {
    size_t First = 0;
    size_t Count = 0;
  };

  std::vector<AnimationTrack> Tracks_;
  std::vector<AnimationRestPose> Nodes_;
  std::vector<TrackRange> NodeTracks_;
  std::vector<double> RestWeights_;
  double StartS_ = 0;
  double EndS_ = 0;
};

}
#endif
