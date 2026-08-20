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
    case AnimationPath::MaterialFactor: return "a material factor";
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
    /* THE NODE'S MORPH WEIGHTS AND THEIR REST VALUES (board:1203). The count is the mesh's, because
     * glTF puts the targets on the primitive and the weights on the mesh, and the reader has already
     * refused a mesh whose primitives disagree about the count. An absent `mesh.weights` is the
     * format's zeros, which is what an empty `Weights` means and why the resize below states it once
     * rather than at every reader of the run. */
    held.WeightFirst = document.MorphWeightsFirst(node);
    held.WeightCount = document.MorphWeightsCount(node);
    for (size_t at = 0; at < held.WeightCount; ++at) {
      const std::vector<double> &declared = document.Meshes()[(size_t)source.Mesh].Weights;
      out.RestWeights_.push_back(at < declared.size() ? declared[at] : 0.0);
    }
  }

  bool first = true;
  std::vector<double> times, values;
  /* WHICH ANIMATION CLAIMED A NODE'S PATH, so a second claim is refused by name rather than resolved
   * by order. The format leaves two animations on one property undefined; deciding it here would be
   * this engine inventing a rule the file does not carry. */
  /* WHAT A CHANNEL DRIVES, AND IT IS NOT ALWAYS A NODE (board:1392). A `KHR_animation_pointer`
   * channel names no node at all -- the extension states the `node` property MUST NOT be set -- so a
   * key of `(node, path)` would read every material channel in a file as one and the same claim and
   * refuse the second. What identifies a claim is the thing driven: a node and its path, or a
   * material and its factor. */
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
  const Animation &what = declared[(size_t)animation];
  for (const AnimationChannel &channel : what.Channels) {
    /* **A CHANNEL DRIVES A NODE OR IT DRIVES A MATERIAL, AND ONLY ONE OF THOSE HAS A NODE**
     * (board:1392). Every check between here and the sampler is about a node, and a pointer channel
     * has none by the extension's own rule -- so guarding them as one is what let `Node < 0` swallow
     * every material channel before it could be held. [MEASURED] `FactorsAt` was unreachable code:
     * it answered nothing for a file whose animation resolved perfectly, because the pose had
     * dropped the channel one function earlier. */
    const bool drivesMaterial = channel.Path == AnimationPath::MaterialFactor;
    /* THE FORMAT'S OWN "IGNORE THIS ONE": a channel with no node is defined as skipped rather than
     * as an error, so it is not a refusal here either. */
    if (!drivesMaterial && channel.Node < 0) { continue; }
    if (!drivesMaterial && (size_t)channel.Node >= document.Nodes().size()) {
      error = document.Path() + ": animation channel targets node " +
              std::to_string(channel.Node) + ", which the file does not carry";
      return false;
    }
    /* THE REFUSAL THAT STOOD HERE IS GONE WITH ITS CAUSE (board:1203). It read *a pose writes node
     * transforms only*, which was true of a pose that carried none. What survives is narrower and
     * is about the FILE: a weights channel driving a node whose mesh has no morph target drives
     * nothing, and the format gives its keyframes no length. */
    if (!drivesMaterial && channel.Path == AnimationPath::Weights &&
        out.Nodes_[(size_t)channel.Node].WeightCount == 0) {
      error = document.Path() + ": animation channel targets the morph weights of node " +
              std::to_string(channel.Node) + ", whose mesh declares no morph target";
      return false;
    }
    if (!drivesMaterial && out.Nodes_[(size_t)channel.Node].HasMatrix) {
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
    /* THE CURVE'S WIDTH IS THE MESH'S TARGET COUNT, and `Track` derived it from the run rather than
     * from the path, which cannot answer it. A file whose weights run divides to a different number
     * is refused HERE, where the mesh is in scope: 127 keyframes and 254 values is two targets, and
     * a mesh with three would be a keyframe run that silently drives the wrong ones. */
    if (channel.Path == AnimationPath::Weights &&
        held->Curve.Components() != out.Nodes_[(size_t)channel.Node].WeightCount) {
      error = document.Path() + ": the weights channel of node " + std::to_string(channel.Node) +
              " carries " + std::to_string(held->Curve.Components()) +
              " values per keyframe and its mesh declares " +
              std::to_string(out.Nodes_[(size_t)channel.Node].WeightCount) + " morph targets";
      return false;
    }
    for (const double when : held->Times) {
      out.StartS_ = first ? when : (when < out.StartS_ ? when : out.StartS_);
      out.EndS_ = first ? when : (when > out.EndS_ ? when : out.EndS_);
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
    claimed.push_back(Claim{channel.Node, channel.Path, channel.Material, channel.Factor});
    claimedBy.push_back(animation);
    out.Channels_.push_back(std::move(held));
  }
  }
  /* AN ANIMATION THIS ENGINE CANNOT DRIVE IS A STILL AND NOT A REFUSAL (board:1392). It was a refusal
   * while every channel a file could declare was a node channel, so an empty result meant the file
   * was self-contradictory. `KHR_animation_pointer` broke that: a file may drive nothing BUT
   * materials, and every one of those channels is legitimately undriven here -- the reader counts
   * them and names their pointers, so the shortfall is published rather than lost.
   *
   * WHAT IS RETURNED IS THE REST POSE, which is the file's own placement of every node, and that is
   * exactly what glTF says a client that ignores animations shows. **A subject that stands still is a
   * picture; a subject that refuses is a hole.** */
  return true;
}

void Pose::At(double seconds, std::vector<Transform> &locals, std::vector<double> &weights) const {
  locals.resize(Nodes_.size());
  weights = RestWeights_;
  for (size_t node = 0; node < Nodes_.size(); ++node) {
    Placement posed = Nodes_[node];
    for (const std::unique_ptr<Channel> &channel : Channels_) {
      if ((size_t)channel->Node != node) { continue; }
      switch (channel->Path) {
        case AnimationPath::Translation: channel->Curve.At(seconds, posed.Translation); break;
        case AnimationPath::Rotation: channel->Curve.At(seconds, posed.Rotation); break;
        case AnimationPath::Scale: channel->Curve.At(seconds, posed.Scale); break;
        /* `Build` has already refused a width that disagrees with the mesh, so this writes exactly
         * the node's own slice and no bound needs re-checking on the frame path. */
        /* A MATERIAL FACTOR IS NOT A POSE AND REACHES NO NODE (board:1392). A pointer channel names
         * no node at all -- the format forbids it -- so this arm is unreachable through the `Node`
         * test above and exists to say the enumeration was ANSWERED rather than defaulted. What a
         * consumer does with an animated material row is its own question and not this pose's. */
        /* ANSWERED BY `FactorsAt` AND NOT HERE (board:1392). A pointer channel names no node, so it
         * is unreachable through the `Node` test above; the arm stays so the enumeration is answered
         * rather than defaulted. */
        case AnimationPath::MaterialFactor: break;
        case AnimationPath::Weights:
          channel->Curve.At(seconds, &weights[posed.WeightFirst]);
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
    /* THE SAME `Track` THE FOUR NODE PATHS USE, which is the whole of what this item asked for: a
     * pointer that resolves is animated by the machinery already here rather than by a second
     * sampler beside it. The curve's width was derived from the run at `Build`, and the factor's own
     * width bounds the write. */
    double all[4] = {0, 0, 0, 0};
    channel->Curve.At(seconds, all);
    const size_t width = FactorComponents(channel->Factor);
    for (size_t component = 0; component < width && component < 4; ++component) {
      sampled.Values[component] = all[component];
    }
    factors.push_back(sampled);
  }
}

} // namespace outshine::Gltf
