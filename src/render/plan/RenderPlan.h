/* THE DECLARED RENDER PLAN. A consumer says what it wants out and what content it wants in; the
 * compiler pulls the machinery backwards from the requested outputs and answers with a plan, or
 * refuses and names the repair. NOTHING IS FIXED HERE: there is no preset, no default composition
 * and no implicit pipeline -- a plan that declares nothing renders nothing and is refused for it.
 *
 * NO DEVICE TYPE APPEARS IN THIS FILE OR IN RenderCatalogue.h. That is what makes a plan checkable
 * before a bind group can exist, and it is also what makes the device layer replaceable underneath:
 * a port changes what executes a plan and not what a plan is.
 *
 * A `RenderPlan` HAS NO PUBLIC CONSTRUCTOR. It exists only as `Compile`'s output, so "a renderer
 * holding an unvalidated plan" has no spelling. The out-parameter is the handle, which is
 * default-constructible while the plan is not, and on a refusal it is untouched. */
#ifndef RENDERPLAN_H
#define RENDERPLAN_H

#include <memory>
#include <string>
#include <vector>

#include "RenderCatalogue.h"

namespace outshine::Render {

/* THE DISPLAY TRANSFER, and `Linear` is not a look. It writes scene-referred radiance into an
 * sRGB-encoding attachment and does nothing else, which is what a numeric comparison against a path
 * tracer needs: a curve there would be measuring the curve. */
enum class Transfer { Linear, Filmic };

/* HOW EXACTLY THE SCENE-REFERRED CHAIN IS STORED. `Half` is what a shipping frame pays for; `Float`
 * removes the store's own rounding from the picture and is what a plan declares when the number
 * taken out of `SceneLinear` is the verdict (board:0087). It governs `SceneHdr` and
 * `SceneLinear` together, because the second is the first under the plan's alias and a half target
 * feeding a float readback would report the storage as if it were the arithmetic. */
enum class ScenePrecision { Half, Float };

/* A VALUE PLUS THE FACT THAT SOMEBODY SET IT. A plan that leaves an option alone is not declaring
 * it, and only a declared option can be refused for being unread -- which is the whole of the
 * option check. */
template <typename T>
class Declared {
public:
  Declared() = default;
  explicit Declared(T value) : Value_(value), Set_(true) {}
  [[nodiscard]] bool IsSet() const { return Set_; }
  [[nodiscard]] T Or(T fallback) const { return Set_ ? Value_ : fallback; }

private:
  T Value_{};
  bool Set_ = false;
};

/* WHAT A CONSUMER WRITES DOWN, and it carries no order at all: an order a consumer writes is a
 * second place for it to be wrong, and the catalogue's enumeration already is one. */
struct PlanSpec {
  std::vector<Resource> Outputs;
  std::vector<Stage> Content;
  /* The scale the tonemap multiplies scene radiance by when the plan declares no meter. */
  Declared<float> Exposure;
  Declared<Transfer> Display;
  Declared<ScenePrecision> Precision;
};

class RenderPlan {
public:
  struct Pass {
    PassKind Kind;
    std::string Name;
    size_t First;   /* into Order() */
    size_t Count;
    /* WHAT THE PASS ATTACHES, derived once here rather than rebuilt per frame by whoever encodes it.
     * `Colours` is the union of the pass's stages' targets and `Depth` the one depth target among
     * them; a second, different depth target in one pass is a refusal, not a silent drop. */
    AttachmentSet Colours;
    Resource Depth = kNoEdge;
  };

  [[nodiscard]] static bool Compile(const PlanSpec &spec, std::shared_ptr<const RenderPlan> *out,
                                    std::string &error);

  /* A name a declaration file can carry, and the only place a string ever becomes a stage. */
  [[nodiscard]] static bool StageByName(const std::string &name, Stage *out);

  [[nodiscard]] bool Holds(Stage stage) const { return HeldStage_[static_cast<size_t>(stage)]; }
  [[nodiscard]] bool Holds(Resource resource) const {
    return HeldResource_[static_cast<size_t>(resource)];
  }
  /* What a reader of `resource` actually binds, after the aliases the plan applied. */
  [[nodiscard]] Resource Bound(Resource resource) const {
    return Bound_[static_cast<size_t>(resource)];
  }

  [[nodiscard]] const std::vector<Stage> &Order() const { return Order_; }
  [[nodiscard]] const std::vector<Pass> &Passes() const { return Passes_; }
  [[nodiscard]] int PassCount() const { return static_cast<int>(Passes_.size()); }
  /* Whether the pair of stages at `Order()[first]` was fused into one pass under R2. */
  [[nodiscard]] bool Fused(Stage stage) const { return Fused_[static_cast<size_t>(stage)]; }

  /* HOW MANY FRAMES BEFORE THE PICTURE IS THE PICTURE. One, plus the history a temporal stage has to
   * fill: a plan with no temporal history needs no settle frames, and a test therefore carries no
   * `[SET]` constant of its own. */
  [[nodiscard]] int SettleFrames() const { return SettleFrames_; }

  [[nodiscard]] Transfer Display() const { return Display_; }
  [[nodiscard]] float Exposure() const { return Exposure_; }

  /* WHAT A RESOURCE IS STORED IN, after the plan's declared precision has been applied. Read this
   * and never the catalogue row: the row is the default and the plan is the answer. */
  [[nodiscard]] TexelFormat Format(Resource resource) const {
    return Format_[static_cast<size_t>(resource)];
  }
  [[nodiscard]] ScenePrecision Precision() const { return Precision_; }

  /* THE PLAN'S OWN IDENTITY: the stage set, the derived order, the passes, the merges, the aliases,
   * the resource formats and the options -- everything whose change can move a pixel. A baseline is
   * keyed by it, so a plan change reports "this plan has no baseline" rather than "the sha changed". */
  [[nodiscard]] const std::string &Digest() const { return Digest_; }

  /* Published, because a merge that is invisible is a property of a shader instead of a plan. */
  [[nodiscard]] const std::vector<std::string> &Merges() const { return Merges_; }
  [[nodiscard]] const std::vector<std::string> &Aliases() const { return Aliases_; }

private:
  RenderPlan() = default;

  bool HeldStage_[kStageCount] = {};
  bool HeldResource_[kResourceCount] = {};
  bool Fused_[kStageCount] = {};
  Resource Bound_[kResourceCount] = {};
  TexelFormat Format_[kResourceCount] = {};
  std::vector<Stage> Order_;
  std::vector<Pass> Passes_;
  std::vector<std::string> Merges_;
  std::vector<std::string> Aliases_;
  std::string Digest_;
  Transfer Display_ = Transfer::Filmic;
  ScenePrecision Precision_ = ScenePrecision::Half;
  float Exposure_ = 1.0f;
  int SettleFrames_ = 1;
};

} // namespace outshine::Render
#endif
