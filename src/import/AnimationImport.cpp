#include <span>
#include <map>
#include <tuple>
#include <algorithm>
#include <array>
#include "AnimationImport.h"

#include "Document.h"
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace outshine::Gltf {

namespace {

namespace Says {
constexpr auto InvalidMaterialTarget = "animation targets a material outside the asset";
constexpr auto InvalidMaterialComponents =
    "animation output components do not match the material property";
}

const char *PathName(AnimationPath path) {
  switch (path) {
    case AnimationPath::Translation: return "translation";
    case AnimationPath::Rotation: return "rotation";
    case AnimationPath::Scale: return "scale";
    case AnimationPath::Weights: return "weights";
    case AnimationPath::MaterialFactor: return "a material factor";
  }
  return "unknown";
}

AnimationTarget NativeTarget(const AnimationChannel &channel) {
  switch (channel.Path) {
    case AnimationPath::Translation: return AnimationTarget::Translation;
    case AnimationPath::Rotation: return AnimationTarget::Rotation;
    case AnimationPath::Scale: return AnimationTarget::Scale;
    case AnimationPath::Weights: return AnimationTarget::MorphWeights;
    case AnimationPath::MaterialFactor:
      switch (channel.Factor) {
        case MaterialFactor::BaseColour: return AnimationTarget::BaseColour;
        case MaterialFactor::Metalness: return AnimationTarget::Metalness;
        case MaterialFactor::Roughness: return AnimationTarget::Roughness;
        case MaterialFactor::Emissive: return AnimationTarget::Emission;
      }
  }
  return AnimationTarget::Translation;
}

}

bool AnimationImport::Build(const Document &document,
                            int animation,
                            AnimationClip &out,
                            std::string &error) {
  const std::array<int, 1> one = {{animation}};
  return Build(document, std::span<const int>(one.data(), 1), out, error);
}

struct AnimationImport::BuildState {
  std::map<std::tuple<AnimationPath, int, MaterialFactor>, int> Claimed;

  bool Claim(const Document &document,
             const AnimationChannel &channel,
             int animation,
             std::string &error) {
    const bool material = channel.Path == AnimationPath::MaterialFactor;
    const auto key = std::tuple{channel.Path,
                                material ? channel.Material : channel.Node,
                                material ? channel.Factor : MaterialFactor::BaseColour};
    const auto [found, inserted] = Claimed.emplace(key, animation);
    if (inserted) { return true; }
    error = document.Path() + ": animations " + std::to_string(found->second) + " and " +
            std::to_string(animation) + " both drive the " + PathName(channel.Path) + " of node " +
            std::to_string(channel.Node) + ", and the format states no result for that";
    return false;
  }
};

bool AnimationImport::Build(const Document &document,
                            std::span<const int> animations,
                            AnimationClip &out,
                            std::string &error) {
  error.clear();
  const auto &declared = document.Animations();
  if (animations.empty()) {
    error = document.Path() + ": a pose requires a nonempty selection of animations";
    return false;
  }
  for (const int animation : animations) {
    if (animation < 0 || static_cast<size_t>(animation) >= declared.size()) {
      error = document.Path() + ": animation " + std::to_string(animation) + " of " +
              std::to_string(declared.size()) + " the file carries";
      return false;
    }
  }
  AnimationImport candidate;
  candidate.InitialiseNodes(document);
  BuildState state;
  for (const int animation : animations) {
    if (!candidate.AppendAnimation(document, animation, state, error)) { return false; }
  }
  candidate.Publish(out);
  return true;
}

void AnimationImport::InitialiseNodes(const Document &document) {
  Nodes_.resize(document.Nodes().size());
  for (size_t node = 0; node < document.Nodes().size(); ++node) {
    const Node &source = document.Nodes()[node];
    AnimationRestPose &held = Nodes_[node];
    held.HasMatrix = source.HasMatrix;
    held.Translation = source.Translation;
    held.Scale = source.Scale;
    held.Rotation = source.Rotation;
    for (size_t at = 0; at < 16; ++at) { held.Matrix[at] = source.Matrix[at]; }

    held.WeightFirst = document.MorphWeightsFirst(node);
    held.WeightCount = document.MorphWeightsCount(node);
    for (size_t at = 0; at < held.WeightCount; ++at) {
      const std::vector<double> &meshWeights =
          document.Meshes()[static_cast<size_t>(source.Mesh)].Weights;
      RestWeights_.push_back(at < meshWeights.size() ? meshWeights[at] : 0.0);
    }
  }
}

bool AnimationImport::ValidateChannel(const Document &document,
                                      const Animation &what,
                                      const AnimationChannel &channel,
                                      std::string &error) const {
  const bool drivesMaterial = channel.Path == AnimationPath::MaterialFactor;

  if (drivesMaterial && (channel.Material < 0 ||
                         static_cast<size_t>(channel.Material) >= document.Materials().size())) {
    error = Says::InvalidMaterialTarget;
    return false;
  }
  if (!drivesMaterial && static_cast<size_t>(channel.Node) >= document.Nodes().size()) {
    error = document.Path() + ": animation channel targets node " + std::to_string(channel.Node) +
            ", which the file does not carry";
    return false;
  }

  if (!drivesMaterial && channel.Path == AnimationPath::Weights &&
      Nodes_[static_cast<size_t>(channel.Node)].WeightCount == 0) {
    error = document.Path() + ": animation channel targets the morph weights of node " +
            std::to_string(channel.Node) + ", whose mesh declares no morph target";
    return false;
  }
  if (!drivesMaterial && Nodes_[static_cast<size_t>(channel.Node)].HasMatrix) {
    error = document.Path() + ": node " + std::to_string(channel.Node) +
            " spells its placement as a matrix and is targeted for animation, which the format "
            "forbids";
    return false;
  }
  if (channel.Sampler < 0 || static_cast<size_t>(channel.Sampler) >= what.Samplers.size()) {
    error = document.Path() + ": animation channel names sampler " +
            std::to_string(channel.Sampler) + " of " + std::to_string(what.Samplers.size());
    return false;
  }
  return true;
}

bool AnimationImport::AppendChannel(const Document &document,
                                    const Animation &what,
                                    const AnimationChannel &channel,
                                    std::string &error) {
  const bool drivesMaterial = channel.Path == AnimationPath::MaterialFactor;
  const AnimationSampler &sampler = what.Samplers[static_cast<size_t>(channel.Sampler)];
  AnimationTrack held;
  if (!document.ReadElements(sampler.Input, held.Times)) {
    error = document.Path() + ": an animation sampler's input does not decode: " + document.Error();
    return false;
  }
  if (!document.ReadElements(sampler.Output, held.Values)) {
    error =
        document.Path() + ": an animation sampler's output does not decode: " + document.Error();
    return false;
  }
  held.Target = drivesMaterial ? channel.Material : channel.Node;
  held.Property = NativeTarget(channel);
  if (held.Property == AnimationTarget::Emission) {
    held.EmissionScale =
        document.Materials()[static_cast<size_t>(channel.Material)].EmissiveStrength;
  }
  const size_t fixedComponents = PathComponents(channel.Path);
  const size_t perKeyframe = sampler.How == Interpolation::CubicSpline ? 3u : 1u;
  size_t components = fixedComponents;
  if (components == 0 && !held.Times.empty() && held.Values.size() % held.Times.size() == 0) {
    const size_t perTime = held.Values.size() / held.Times.size();
    if (perTime % perKeyframe == 0) { components = perTime / perKeyframe; }
  }
  if (!AnimationCurve::Build(sampler.How,
                             held.Times,
                             held.Values,
                             components,
                             channel.Path == AnimationPath::Rotation
                                 ? AnimationCurve::Values::Rotation
                                 : AnimationCurve::Values::Linear,
                             held.Curve)) {
    error = document.Path() + ": the " + PathName(channel.Path) + " channel of node " +
            std::to_string(channel.Node) + " states " + std::to_string(held.Times.size()) +
            " keyframes and " + std::to_string(held.Values.size()) +
            " values, which do not describe a curve";
    return false;
  }

  if (drivesMaterial && held.Curve.Components() != FactorComponents(channel.Factor)) {
    error = Says::InvalidMaterialComponents;
    return false;
  }

  if (channel.Path == AnimationPath::Weights &&
      held.Curve.Components() != Nodes_[static_cast<size_t>(channel.Node)].WeightCount) {
    error = document.Path() + ": the weights channel of node " + std::to_string(channel.Node) +
            " carries " + std::to_string(held.Curve.Components()) +
            " values per keyframe and its mesh declares " +
            std::to_string(Nodes_[static_cast<size_t>(channel.Node)].WeightCount) +
            " morph targets";
    return false;
  }
  bool first = Tracks_.empty();
  for (const double when : held.Times) {
    StartS_ = first ? when : std::min(when, StartS_);
    EndS_ = first ? when : std::max(when, EndS_);
    first = false;
  }
  Tracks_.push_back(std::move(held));
  return true;
}

bool AnimationImport::AppendAnimation(const Document &document,
                                      int animation,
                                      BuildState &state,
                                      std::string &error) {
  const auto &what = document.Animations()[static_cast<size_t>(animation)];
  for (const auto &channel : what.Channels) {
    if (channel.Path != AnimationPath::MaterialFactor && channel.Node < 0) { continue; }
    if (!ValidateChannel(document, what, channel, error) ||
        !state.Claim(document, channel, animation, error) ||
        !AppendChannel(document, what, channel, error)) {
      return false;
    }
  }
  return true;
}

void AnimationImport::Publish(AnimationClip &out) {
  out.Adopt(std::move(Nodes_),
            std::move(RestWeights_),
            std::move(Tracks_),
            {.StartS = StartS_, .EndS = EndS_});
}

}
