/* THE DOCUMENT'S NODE HIERARCHY AT ONE TIME, and it is the link this tree did not have
 * (board:1169). The sampler existed at four layers -- `core/Keyframes`, `core/CatmullRom`,
 * `gltf/Track`, `scenario/Animation` -- and nothing anywhere took a time, sampled the channels and
 * handed the result to something that draws. This is that consumer's engine half: a time in, one
 * LOCAL transform per node out, which is exactly what the flatten already walks.
 *
 * IT WRITES A LOCAL AND NOT A WORLD, so the parent chain stays `Document`'s single walk. A second
 * chain walk here would be a second answer to "where is this node", and the two would disagree the
 * first time one of them learned about a new way to spell a placement.
 *
 * EVERY NODE, NOT EVERY DRIVEN NODE. A run one entry short would be a partial pose, and a partial
 * pose is unspellable here: `At` writes `Document::Nodes().size()` transforms whatever the animation
 * drives, and `WorldTransform` refuses a run of any other length. That is what makes "the animated
 * node moved and its unanimated sibling did not" a property of the arithmetic rather than of a
 * caller remembering to fill the gaps.
 *
 * SECONDS, BECAUSE THAT IS WHAT THE FILE STATES. glTF's sampler input accessor is in seconds; the
 * frame is the case's own currency (board:1129) and the division by fps happens where the frame is
 * declared. Nothing here accumulates a clock.
 *
 * THREE NAMED REFUSALS AND NO PARTIAL POSE. A channel targeting a node the file does not carry, a
 * `weights` channel -- a morph target, which has no node transform to write -- and a driven node
 * that spells its placement as a `matrix`, which the format forbids for exactly this reason
 * ("When a node is targeted for animation ... `matrix` MUST NOT be present"). */
#ifndef GLTF_POSE_H
#define GLTF_POSE_H

#include <memory>
#include <string>
#include <vector>

#include "Track.h"
#include "Span.h"
#include "Transform.h"
#include "Types.h"

namespace outshine::Gltf {

class Document;

class Pose {
public:
  /* `animation` indexes `Document::Animations()`. A document with none, or an index outside it, is a
   * refusal naming the count -- "the first animation found" would render one animation and report
   * another. */
  [[nodiscard]] static bool Build(const Document &document, int animation, Pose &out,
                                  std::string &error);

  /* A DECLARED SET, BECAUSE A FILE'S ANIMATIONS ARE INDEPENDENT AND A CLIENT PLAYS ANY SUBSET
   * (board:1198). `InterpolationTest` states nine of them, one per node -- three interpolations over
   * three paths -- and its picture is all nine at once, so a pose built from one index would render
   * eight still cubes beside one moving one and report the asset's claim as met.
   *
   * WHICH SUBSET IS THE CALLER'S DECLARATION AND NEVER A DEFAULT: "all of them" is a declaration like
   * any other, and "the first one found" is the shape the single-index refusal below already refuses.
   * TWO ANIMATIONS DRIVING ONE NODE'S SAME PATH IS A REFUSAL naming both -- the format leaves the
   * result undefined, and a pose that silently kept the last would decide it here. */
  [[nodiscard]] static bool Build(const Document &document, Span<const int> animations, Pose &out,
                                  std::string &error);

  [[nodiscard]] bool Valid() const { return !Nodes_.empty(); }
  /* The extent of the file's own keyframe grid over every channel of this animation, in seconds.
   * Outside it the first or last keyframe stands, which is the format's clamp. */
  double StartS() const { return StartS_; }
  double EndS() const { return EndS_; }
  size_t ChannelCount() const { return Channels_.size(); }
  size_t NodeCount() const { return Nodes_.size(); }

  /* Writes `NodeCount()` local transforms AND the morph weights of every node, both into the
   * caller's buffers so a sweep over a frame grid allocates once -- the hot-loop exception `F.20`
   * states for itself.
   *
   * ONE CALL AND NOT TWO, BECAUSE A POSE IS BOTH (board:1203). glTF drives a node's placement and
   * its mesh's morph weights through channels of one animation over one time grid, so a caller that
   * could take the transforms without the weights could draw a body posed at one instant and shaped
   * at another. `weights` is FLAT and indexed by `WeightsOf`, because a node with no morph target
   * contributes a run of length zero and a vector of vectors would allocate one per node per
   * frame. */
  void At(double seconds, std::vector<Transform> &locals, std::vector<double> &weights) const;

  /* ONE ANIMATED MATERIAL FACTOR AT ONE INSTANT (board:1392). `Values` carries the factor's own width
   * -- four for a base colour, one for metalness or roughness, three for an emissive -- which is
   * `FactorComponents` and not a number this struct restates. */
  struct FactorAt {
    int Material = -1;
    MaterialFactor Factor = MaterialFactor::BaseColour;
    double Values[4] = {0, 0, 0, 0};
  };

  /* EVERY MATERIAL FACTOR THIS ANIMATION DRIVES, SAMPLED AT `seconds` (board:1392). A separate call
   * from `At` because it answers a separate question and its result is keyed by MATERIAL where that
   * one is keyed by node -- and a consumer that shades from the manifest rather than from the file
   * has no use for it at all. Empty where the file drives no material, which is every subject in
   * this corpus but three. */
  void FactorsAt(double seconds, std::vector<FactorAt> &factors) const;

  /* Where one node's weights sit in the flat run `At` writes: `{first, count}`, and `count` is zero
   * for a node whose mesh has no morph target -- which is most of them. */
  [[nodiscard]] size_t WeightsFirst(size_t node) const { return Nodes_[node].WeightFirst; }
  [[nodiscard]] size_t WeightsCount(size_t node) const { return Nodes_[node].WeightCount; }
  [[nodiscard]] size_t WeightCount() const { return RestWeights_.size(); }

private:
  /* THE FILE'S OWN PLACEMENT OF ONE NODE, which is what an undriven component keeps. A driven node
   * carries no matrix -- `Build` refuses one -- so the TRS triple is the whole of what a channel
   * overrides and there is no arm where a matrix and a track both claim the same node. */
  struct Placement {
    double Translation[3] = {0, 0, 0};
    double Rotation[4] = {0, 0, 0, 1};
    double Scale[3] = {1, 1, 1};
    bool HasMatrix = false;
    double Matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    /* This node's slice of `RestWeights_`, and of the run `At` writes. */
    size_t WeightFirst = 0;
    size_t WeightCount = 0;
  };

  /* ONE DRIVEN PROPERTY: the decoded runs the file carries and the curve over them. The curve is a
   * VIEW of the two vectors beside it, so the element's address has to outlive a push -- which a
   * `unique_ptr` element makes true by construction rather than by remembering to reserve. */
  struct Channel {
    int Node = -1;
    AnimationPath Path = AnimationPath::Translation;
    /* WHICH MATERIAL AND WHICH OF ITS NUMBERS, carried through from the document's own channel and
     * set only where `Path` is `MaterialFactor` (board:1392). A pointer channel names no node, so
     * these two are its whole target. */
    int Material = -1;
    MaterialFactor Factor = MaterialFactor::BaseColour;
    std::vector<double> Times, Values;
    Track Curve;
  };

  std::vector<std::unique_ptr<Channel>> Channels_;
  std::vector<Placement> Nodes_;
  /* THE FILE'S OWN WEIGHTS, which is what an undriven target keeps -- `mesh.weights` where the file
   * declares them and the format's zeros where it does not. Flat over every node, sliced by
   * `Placement::WeightFirst`. */
  std::vector<double> RestWeights_;
  double StartS_ = 0, EndS_ = 0;
};

} // namespace outshine::Gltf
#endif
