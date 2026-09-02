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

} // namespace

bool Pose::Build(const Document &document, int animation, Pose &out, std::string &error) {
  const std::array<int, 1> one = {{animation}};
  return Build(document, Span<const int>(one.data(), 1), out, error);
}

bool Pose::Build(const Document &document,
                 Span<const int> animations,
                 Pose &out,
                 std::string &error) {
  out = Pose();
  const std::vector<Animation> &declared = document.Animations();
  if (animations.Size() == 0) {
    error = document.Path() + ": a pose is built from a declared set of animations and the set is "
                              "empty, which is a different statement from a file with none";
    return false;
  }
  for (size_t which = 0; which < animations.Size(); ++which) {
    const int animation = animations[which];
    if (animation < 0 || static_cast<size_t>(animation) >= declared.size()) {
      error = document.Path() + ": animation " + std::to_string(animation) + " of " +
              std::to_string(declared.size()) + " the file carries";
      return false;
    }
  }
  out.Nodes_.resize(document.Nodes().size());
  for (size_t node = 0; node < document.Nodes().size(); ++node) {
    const Node &source = document.Nodes()[node];
    Viewpoint &held = out.Nodes_[node];
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
      out.RestWeights_.push_back(at < meshWeights.size() ? meshWeights[at] : 0.0);
    }
  }

  bool first = true;
  std::vector<double> times;
  std::vector<double> values;

  struct Claim {
    int Node = -1;
    AnimationPath Path = AnimationPath::Translation;
    int Material = -1;
    MaterialFactor Factor = MaterialFactor::BaseColour;
  };

  std::vector<Claim> claimed;
  std::vector<int> claimedBy;
  for (size_t which = 0; which < animations.Size(); ++which) {
    const int animation = animations[which];
    const Animation &what = declared[static_cast<size_t>(animation)];
    for (const AnimationChannel &channel : what.Channels) {
      const bool drivesMaterial = channel.Path == AnimationPath::MaterialFactor;

      if (!drivesMaterial && channel.Node < 0) { continue; }
      if (!drivesMaterial && static_cast<size_t>(channel.Node) >= document.Nodes().size()) {
        error = document.Path() + ": animation channel targets node " +
                std::to_string(channel.Node) + ", which the file does not carry";
        return false;
      }

      if (!drivesMaterial && channel.Path == AnimationPath::Weights &&
          out.Nodes_[static_cast<size_t>(channel.Node)].WeightCount == 0) {
        error = document.Path() + ": animation channel targets the morph weights of node " +
                std::to_string(channel.Node) + ", whose mesh declares no morph target";
        return false;
      }
      if (!drivesMaterial && out.Nodes_[static_cast<size_t>(channel.Node)].HasMatrix) {
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
      const AnimationSampler &sampler = what.Samplers[static_cast<size_t>(channel.Sampler)];
      if (!document.ReadElements(sampler.Input, times)) {
        error =
            document.Path() + ": an animation sampler's input does not decode: " + document.Error();
        return false;
      }
      if (!document.ReadElements(sampler.Output, values)) {
        error = document.Path() +
                ": an animation sampler's output does not decode: " + document.Error();
        return false;
      }
      auto held = std::make_unique<Channel>();
      held->Node = channel.Node;
      held->Path = channel.Path;
      held->Material = channel.Material;
      held->Factor = channel.Factor;
      held->Times = times;
      held->Values = values;
      if (!Track::Build(channel.Path, sampler.How, held->Times, held->Values, held->Curve)) {
        error = document.Path() + ": the " + PathName(channel.Path) + " channel of node " +
                std::to_string(channel.Node) + " states " + std::to_string(held->Times.size()) +
                " keyframes and " + std::to_string(held->Values.size()) +
                " values, which do not describe a curve";
        return false;
      }

      if (channel.Path == AnimationPath::Weights &&
          held->Curve.Components() != out.Nodes_[static_cast<size_t>(channel.Node)].WeightCount) {
        error = document.Path() + ": the weights channel of node " + std::to_string(channel.Node) +
                " carries " + std::to_string(held->Curve.Components()) +
                " values per keyframe and its mesh declares " +
                std::to_string(out.Nodes_[static_cast<size_t>(channel.Node)].WeightCount) +
                " morph targets";
        return false;
      }
      for (const double when : held->Times) {
        out.StartS_ = first ? when : std::min(when, out.StartS_);
        out.EndS_ = first ? when : std::max(when, out.EndS_);
        first = false;
      }
      for (size_t already = 0; already < claimed.size(); ++already) {
        if (claimed[already].Path != channel.Path) { continue; }
        if (drivesMaterial ? (claimed[already].Material == channel.Material &&
                              claimed[already].Factor == channel.Factor)
                           : claimed[already].Node == channel.Node) {
          error = document.Path() + ": animations " + std::to_string(claimedBy[already]) + " and " +
                  std::to_string(animation) + " both drive the " + PathName(channel.Path) +
                  " of node " + std::to_string(channel.Node) +
                  ", and the format states no result for that";
          return false;
        }
      }
      claimed.push_back(Claim{.Node = channel.Node,
                              .Path = channel.Path,
                              .Material = channel.Material,
                              .Factor = channel.Factor});
      claimedBy.push_back(animation);
      out.Channels_.push_back(std::move(held));
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

} // namespace outshine::Gltf
