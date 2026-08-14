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

  [[nodiscard]] bool Valid() const { return !Nodes_.empty(); }
  /* The extent of the file's own keyframe grid over every channel of this animation, in seconds.
   * Outside it the first or last keyframe stands, which is the format's clamp. */
  double StartS() const { return StartS_; }
  double EndS() const { return EndS_; }
  size_t ChannelCount() const { return Channels_.size(); }
  size_t NodeCount() const { return Nodes_.size(); }

  /* Writes `NodeCount()` local transforms, the caller's buffer so a sweep over a frame grid
   * allocates once -- the hot-loop exception `F.20` states for itself. */
  void At(double seconds, std::vector<Transform> &locals) const;

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
  };

  /* ONE DRIVEN PROPERTY: the decoded runs the file carries and the curve over them. The curve is a
   * VIEW of the two vectors beside it, so the element's address has to outlive a push -- which a
   * `unique_ptr` element makes true by construction rather than by remembering to reserve. */
  struct Channel {
    int Node = -1;
    AnimationPath Path = AnimationPath::Translation;
    std::vector<double> Times, Values;
    Track Curve;
  };

  std::vector<std::unique_ptr<Channel>> Channels_;
  std::vector<Placement> Nodes_;
  double StartS_ = 0, EndS_ = 0;
};

} // namespace outshine::Gltf
#endif
