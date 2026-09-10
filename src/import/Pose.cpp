#include <span>
#include <map>
#include <tuple>
#include <algorithm>
#include <array>
#include "Pose.h"

#include "Document.h"
#include <string>
#include <utility>
#include <vector>
#include <cstddef>
#include <memory>

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

}

bool Pose::Build(const Document &document, int animation, Pose &out, std::string &error) {
  const std::array<int, 1> one = {{animation}};
  return Build(document, std::span<const int>(one.data(), 1), out, error);
}

struct Pose::BuildState {
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

bool Pose::Build(const Document &document,
                 std::span<const int> animations,
                 Pose &out,
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
  Pose candidate;
  candidate.InitialiseNodes(document);
  BuildState state;
  for (const int animation : animations) {
    if (!candidate.AppendAnimation(document, animation, state, error)) { return false; }
  }
  out = std::move(candidate);
  return true;
}

void Pose::InitialiseNodes(const Document &document) {
  Nodes_.resize(document.Nodes().size());
  for (size_t node = 0; node < document.Nodes().size(); ++node) {
    const Node &source = document.Nodes()[node];
    Viewpoint &held = Nodes_[node];
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

bool Pose::ValidateChannel(const Document &document,
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

bool Pose::AppendChannel(const Document &document,
                         const Animation &what,
                         const AnimationChannel &channel,
                         std::string &error) {
  const bool drivesMaterial = channel.Path == AnimationPath::MaterialFactor;
  const AnimationSampler &sampler = what.Samplers[static_cast<size_t>(channel.Sampler)];
  auto held = std::make_unique<Channel>();
  if (!document.ReadElements(sampler.Input, held->Times)) {
    error = document.Path() + ": an animation sampler's input does not decode: " + document.Error();
    return false;
  }
  if (!document.ReadElements(sampler.Output, held->Values)) {
    error =
        document.Path() + ": an animation sampler's output does not decode: " + document.Error();
    return false;
  }
  held->Node = channel.Node;
  held->Path = channel.Path;
  held->Material = channel.Material;
  held->Factor = channel.Factor;
  if (!Track::Build(channel.Path, sampler.How, held->Times, held->Values, held->Curve)) {
    error = document.Path() + ": the " + PathName(channel.Path) + " channel of node " +
            std::to_string(channel.Node) + " states " + std::to_string(held->Times.size()) +
            " keyframes and " + std::to_string(held->Values.size()) +
            " values, which do not describe a curve";
    return false;
  }

  if (drivesMaterial && held->Curve.Components() != FactorComponents(channel.Factor)) {
    error = Says::InvalidMaterialComponents;
    return false;
  }

  if (channel.Path == AnimationPath::Weights &&
      held->Curve.Components() != Nodes_[static_cast<size_t>(channel.Node)].WeightCount) {
    error = document.Path() + ": the weights channel of node " + std::to_string(channel.Node) +
            " carries " + std::to_string(held->Curve.Components()) +
            " values per keyframe and its mesh declares " +
            std::to_string(Nodes_[static_cast<size_t>(channel.Node)].WeightCount) +
            " morph targets";
    return false;
  }
  bool first = Channels_.empty();
  for (const double when : held->Times) {
    StartS_ = first ? when : std::min(when, StartS_);
    EndS_ = first ? when : std::max(when, EndS_);
    first = false;
  }
  Channels_.push_back(std::move(held));
  return true;
}

bool Pose::AppendAnimation(const Document &document,
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

void Pose::At(double seconds, std::vector<Transform> &locals, std::vector<double> &weights) const {
  locals.resize(Nodes_.size());
  weights = RestWeights_;
  for (size_t node = 0; node < Nodes_.size(); ++node) {
    Viewpoint posed = Nodes_[node];
    for (const std::unique_ptr<Channel> &channel : Channels_) {
      if (std::cmp_not_equal(channel->Node, node)) { continue; }
      switch (channel->Path) {
        case AnimationPath::Translation: channel->Curve.At(seconds, posed.Translation.Row()); break;
        case AnimationPath::Rotation: {
          std::array<double, 4> sampled = {0.0, 0.0, 0.0, 1.0};
          channel->Curve.At(seconds, sampled);
          posed.Rotation = {.X = sampled[0], .Y = sampled[1], .Z = sampled[2], .W = sampled[3]};
          break;
        }
        case AnimationPath::Scale: channel->Curve.At(seconds, posed.Scale.Row()); break;

        case AnimationPath::MaterialFactor: break;
        case AnimationPath::Weights:
          channel->Curve.At(seconds, std::span(weights).subspan(posed.WeightFirst));
          break;
      }
    }
    locals[node] = posed.HasMatrix
                       ? Transform::FromColumnMajor(posed.Matrix)
                       : Transform::FromTrs(posed.Translation, posed.Rotation, posed.Scale);
  }
}

void Pose::FactorsAt(double seconds, std::vector<FactorAt> &factors) const {
  factors.clear();
  for (const std::unique_ptr<Channel> &channel : Channels_) {
    if (channel->Path != AnimationPath::MaterialFactor || channel->Material < 0) { continue; }
    FactorAt sampled;
    sampled.Material = channel->Material;
    sampled.Factor = channel->Factor;

    std::array<double, 4> all = {0, 0, 0, 0};
    channel->Curve.At(seconds, all);
    const size_t width = FactorComponents(channel->Factor);
    for (size_t component = 0; component < width && component < 4; ++component) {
      sampled.Values[component] = all[component];
    }
    factors.push_back(sampled);
  }
}

}
