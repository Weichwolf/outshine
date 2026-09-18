#include "AnimationClip.h"

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine {

namespace {

bool MaterialTarget(AnimationTarget target) {
  return target == AnimationTarget::BaseColour || target == AnimationTarget::Metalness ||
         target == AnimationTarget::Roughness || target == AnimationTarget::Emission;
}

size_t MaterialComponents(AnimationTarget target) {
  switch (target) {
    case AnimationTarget::BaseColour: return 4;
    case AnimationTarget::Metalness:
    case AnimationTarget::Roughness: return 1;
    case AnimationTarget::Emission: return 3;
    case AnimationTarget::Translation:
    case AnimationTarget::Rotation:
    case AnimationTarget::Scale:
    case AnimationTarget::MorphWeights: return 0;
  }
  return 0;
}

}

void AnimationClip::Adopt(std::vector<AnimationRestPose> &&nodes,
                          std::vector<double> &&weights,
                          std::vector<std::unique_ptr<AnimationTrack>> &&tracks,
                          AnimationTimeRange range) {
  Nodes_ = std::move(nodes);
  RestWeights_ = std::move(weights);
  Tracks_ = std::move(tracks);
  StartS_ = range.StartS;
  EndS_ = range.EndS;
}

void AnimationClip::SamplePose(double seconds,
                               std::vector<AffineTransform> &locals,
                               std::vector<double> &weights) const {
  locals.resize(Nodes_.size());
  weights = RestWeights_;
  for (size_t node = 0; node < Nodes_.size(); ++node) {
    AnimationRestPose posed = Nodes_[node];
    for (const std::unique_ptr<AnimationTrack> &track : Tracks_) {
      if (track->Target < 0 || std::cmp_not_equal(track->Target, node)) { continue; }
      switch (track->Property) {
        case AnimationTarget::Translation: track->Curve.At(seconds, posed.Translation.Row()); break;
        case AnimationTarget::Rotation: {
          std::array<double, 4> sampled = {0.0, 0.0, 0.0, 1.0};
          track->Curve.At(seconds, sampled);
          posed.Rotation = {.X = sampled[0], .Y = sampled[1], .Z = sampled[2], .W = sampled[3]};
          break;
        }
        case AnimationTarget::Scale: track->Curve.At(seconds, posed.Scale.Row()); break;
        case AnimationTarget::MorphWeights:
          track->Curve.At(seconds, std::span(weights).subspan(posed.WeightFirst));
          break;
        case AnimationTarget::BaseColour:
        case AnimationTarget::Metalness:
        case AnimationTarget::Roughness:
        case AnimationTarget::Emission: break;
      }
    }
    locals[node] = posed.HasMatrix
                       ? AffineTransform::FromColumnMajor(posed.Matrix)
                       : AffineTransform::FromTrs(posed.Translation, posed.Rotation, posed.Scale);
  }
}

void AnimationClip::SampleMaterials(double seconds,
                                    std::vector<AnimatedMaterialSample> &samples) const {
  samples.clear();
  for (const std::unique_ptr<AnimationTrack> &track : Tracks_) {
    if (track->Target < 0 || !MaterialTarget(track->Property)) { continue; }
    AnimatedMaterialSample sampled;
    sampled.Material = track->Target;
    sampled.Property = track->Property;
    std::array<double, 4> values = {0, 0, 0, 0};
    track->Curve.At(seconds, values);
    const size_t width = MaterialComponents(track->Property);
    for (size_t component = 0; component < width; ++component) {
      sampled.Values[component] =
          values[component] *
          (track->Property == AnimationTarget::Emission ? track->EmissionScale : 1.0);
    }
    samples.push_back(sampled);
  }
}

}
