#include "Pose.h"

#include "Document.h"

namespace outshine::Gltf {

namespace {

const char *PathName(AnimationPath path) {
  switch (path) {
    case AnimationPath::Translation: return "translation";
    case AnimationPath::Rotation: return "rotation";
    case AnimationPath::Scale: return "scale";
    case AnimationPath::Weights: return "weights";
  }
  return "unknown";
}

} // namespace

bool Pose::Build(const Document &document, int animation, Pose &out, std::string &error) {
  const int one[1] = {animation};
  return Build(document, Span<const int>(one, 1), out, error);
}

bool Pose::Build(const Document &document, Span<const int> animations, Pose &out,
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
    if (animation < 0 || (size_t)animation >= declared.size()) {
      error = document.Path() + ": animation " + std::to_string(animation) + " of " +
              std::to_string(declared.size()) + " the file carries";
      return false;
    }
  }
  out.Nodes_.resize(document.Nodes().size());
  for (size_t node = 0; node < document.Nodes().size(); ++node) {
    const Node &source = document.Nodes()[node];
    Placement &held = out.Nodes_[node];
    held.HasMatrix = source.HasMatrix;
    for (size_t at = 0; at < 3; ++at) {
      held.Translation[at] = source.Translation[at];
      held.Scale[at] = source.Scale[at];
    }
    for (size_t at = 0; at < 4; ++at) { held.Rotation[at] = source.Rotation[at]; }
    for (size_t at = 0; at < 16; ++at) { held.Matrix[at] = source.Matrix[at]; }
  }

  bool first = true;
  std::vector<double> times, values;
  /* WHICH ANIMATION CLAIMED A NODE'S PATH, so a second claim is refused by name rather than resolved
   * by order. The format leaves two animations on one property undefined; deciding it here would be
   * this engine inventing a rule the file does not carry. */
  std::vector<std::pair<int, AnimationPath>> claimed;
  std::vector<int> claimedBy;
  for (size_t which = 0; which < animations.Size(); ++which) {
  const int animation = animations[which];
  const Animation &what = declared[(size_t)animation];
  for (const AnimationChannel &channel : what.Channels) {
    /* THE FORMAT'S OWN "IGNORE THIS ONE": a channel with no node is defined as skipped rather than
     * as an error, so it is not a refusal here either. */
    if (channel.Node < 0) { continue; }
    if ((size_t)channel.Node >= document.Nodes().size()) {
      error = document.Path() + ": animation channel targets node " +
              std::to_string(channel.Node) + ", which the file does not carry";
      return false;
    }
    if (channel.Path == AnimationPath::Weights) {
      error = document.Path() + ": animation channel targets the morph weights of node " +
              std::to_string(channel.Node) + ", and a pose writes node transforms only";
      return false;
    }
    if (out.Nodes_[(size_t)channel.Node].HasMatrix) {
      error = document.Path() + ": node " + std::to_string(channel.Node) +
              " spells its placement as a matrix and is targeted for animation, which the format "
              "forbids";
      return false;
    }
    if (channel.Sampler < 0 || (size_t)channel.Sampler >= what.Samplers.size()) {
      error = document.Path() + ": animation channel names sampler " +
              std::to_string(channel.Sampler) + " of " + std::to_string(what.Samplers.size());
      return false;
    }
    const AnimationSampler &sampler = what.Samplers[(size_t)channel.Sampler];
    if (!document.ReadElements(sampler.Input, times)) {
      error = document.Path() + ": an animation sampler's input does not decode: " +
              document.Error();
      return false;
    }
    if (!document.ReadElements(sampler.Output, values)) {
      error = document.Path() + ": an animation sampler's output does not decode: " +
              document.Error();
      return false;
    }
    auto held = std::make_unique<Channel>();
    held->Node = channel.Node;
    held->Path = channel.Path;
    held->Times = times;
    held->Values = values;
    if (!Track::Build(channel.Path, sampler.How, held->Times, held->Values, held->Curve)) {
      error = document.Path() + ": the " + PathName(channel.Path) + " channel of node " +
              std::to_string(channel.Node) + " states " + std::to_string(held->Times.size()) +
              " keyframes and " + std::to_string(held->Values.size()) +
              " values, which do not describe a curve";
      return false;
    }
    for (const double when : held->Times) {
      out.StartS_ = first ? when : (when < out.StartS_ ? when : out.StartS_);
      out.EndS_ = first ? when : (when > out.EndS_ ? when : out.EndS_);
      first = false;
    }
    for (size_t already = 0; already < claimed.size(); ++already) {
      if (claimed[already].first == channel.Node && claimed[already].second == channel.Path) {
        error = document.Path() + ": animations " + std::to_string(claimedBy[already]) + " and " +
                std::to_string(animation) + " both drive the " + PathName(channel.Path) +
                " of node " + std::to_string(channel.Node) +
                ", and the format states no result for that";
        return false;
      }
    }
    claimed.push_back({channel.Node, channel.Path});
    claimedBy.push_back(animation);
    out.Channels_.push_back(std::move(held));
  }
  }
  if (out.Channels_.empty()) {
    error = document.Path() + ": the declared animations drive no node, so there is no pose to take";
    return false;
  }
  return true;
}

void Pose::At(double seconds, std::vector<Transform> &locals) const {
  locals.resize(Nodes_.size());
  for (size_t node = 0; node < Nodes_.size(); ++node) {
    Placement posed = Nodes_[node];
    for (const std::unique_ptr<Channel> &channel : Channels_) {
      if ((size_t)channel->Node != node) { continue; }
      switch (channel->Path) {
        case AnimationPath::Translation: channel->Curve.At(seconds, posed.Translation); break;
        case AnimationPath::Rotation: channel->Curve.At(seconds, posed.Rotation); break;
        case AnimationPath::Scale: channel->Curve.At(seconds, posed.Scale); break;
        case AnimationPath::Weights: break;
      }
    }
    locals[node] = posed.HasMatrix
                       ? Transform::FromColumnMajor(posed.Matrix)
                       : Transform::FromTrs(posed.Translation, posed.Rotation, posed.Scale);
  }
}

} // namespace outshine::Gltf
