#include "RenderPlan.h"

#include <algorithm>
#include <cstdio>

#include "Sha256.h"

namespace outshine::Render {
namespace {

/* WHY A WORKLIST AND NOT A RECURSION: the pull is a backward closure over "who produces what I
 * read", and the catalogue's enumeration is already a linear extension of that graph, so nothing
 * here has to sort, visit twice or detect a cycle. The static assertions in RenderCatalogue.h are
 * what pay for that. */
struct Pull {
  const PlanSpec &Spec;
  bool Declared[kStageCount] = {};
  bool HeldStage[kStageCount] = {};
  bool HeldResource[kResourceCount] = {};
  bool Seen[kResourceCount] = {};
  Resource Bound[kResourceCount] = {};
  std::vector<Resource> Wanted;
  std::vector<std::string> Aliases;
  std::string Error;

  explicit Pull(const PlanSpec &spec) : Spec(spec) {
    for (size_t r = 0; r < kResourceCount; ++r) { Bound[r] = static_cast<Resource>(r); }
    for (Stage s : spec.Content) { Declared[static_cast<size_t>(s)] = true; }
  }

  void Want(Resource r) {
    if (Seen[static_cast<size_t>(r)]) { return; }
    Wanted.push_back(r);
  }

  void Hold(Stage s) {
    if (HeldStage[static_cast<size_t>(s)]) { return; }
    HeldStage[static_cast<size_t>(s)] = true;
    const StageRow &row = Row(s);
    for (size_t e = 0; e < kMaxEdges && row.Reads[e] != kNoEdge; ++e) { Want(row.Reads[e]); }
    for (size_t e = 0; e < kMaxEdges && row.Contributes[e] != kNoEdge; ++e) { Want(row.Contributes[e]); }
  }

  /* THE REFUSAL NAMES THE REPAIR AND NOT ONLY THE DEFECT: the resource that was missing, the stage
   * that wanted it, and the stage the catalogue says would have supplied it. */
  void Missing(Resource r, const char *why) {
    Error += Error.empty() ? "render.outputs: " : "; also ";
    Error += std::string(Row(r).Name) + " " + why;
    for (size_t s = 0; s < kStageCount; ++s) {
      const Stage id = static_cast<Stage>(s);
      if (Produces(id, r) && Row(id).From == Provenance::Content) {
        Error += std::string(" -- declare render.content.") + Row(id).Name + " to supply it";
        return;
      }
    }
  }

  [[nodiscard]] bool Resolve(Resource r) {
    const size_t index = static_cast<size_t>(r);
    if (Seen[index]) { return true; }
    Seen[index] = true;
    const ResourceRow &row = Row(r);
    if (row.Kind == ResourceKind::Given) {
      HeldResource[index] = true;
      return true;
    }

    size_t contributors = 0;
    for (size_t s = 0; s < kStageCount; ++s) {
      const Stage id = static_cast<Stage>(s);
      if (!Produces(id, r)) { continue; }
      if (Row(id).From == Provenance::Machinery || Declared[s]) {
        Hold(id);
        ++contributors;
      }
    }
    if (contributors > 0) {
      HeldResource[index] = true;
      return true;
    }

    switch (row.Fallback) {
      case FallbackKind::Alias:
        Bound[index] = row.AliasOf;
        Aliases.push_back(std::string(row.Name) + " -> " + Row(row.AliasOf).Name);
        Want(row.AliasOf);
        return true;
      case FallbackKind::Neutral:
        /* Absent, and its readers are generated without it -- which is what makes "the plan carries
         * no dead path" true rather than asserted. */
        return true;
      case FallbackKind::None:
        Missing(r, row.Kind == ResourceKind::Attachment
                       ? "is an attachment of this plan and nothing draws into it, so the plan would "
                         "compile, run and render black"
                       : "has no producer in this plan");
        return false;
    }
    return false;
  }

  /* THE WHOLE WALK, NOT THE FIRST STUMBLE. Stopping at the first unsupplied resource made the
   * sentence depend on the order the queue happened to reach two equally missing attachments in, so
   * adding one read edge to one stage rewrote a refusal that had not changed meaning. */
  [[nodiscard]] bool Run() {
    for (Resource r : Spec.Outputs) { Want(r); }
    for (size_t at = 0; at < Wanted.size(); ++at) { (void)Resolve(Wanted[at]); }
    return Error.empty();
  }
};

const char *TransferName(Transfer t) { return t == Transfer::Linear ? "linear" : "filmic"; }

const char *FormatName(TexelFormat f) {
  switch (f) {
    case TexelFormat::Handle: return "handle";
    case TexelFormat::Rgba16Float: return "rgba16float";
    case TexelFormat::Rg16Float: return "rg16float";
    case TexelFormat::R8Unorm: return "r8unorm";
    case TexelFormat::Rgba8UnormSrgb: return "rgba8unorm-srgb";
    case TexelFormat::Depth32Float: return "depth32float";
  }
  return "";
}

std::string Decimal(float value) {
  char text[32] = {};
  std::snprintf(text, sizeof text, "%.9g", (double)value);
  return text;
}

} // namespace

bool RenderPlan::StageByName(const std::string &name, Stage *out) {
  for (size_t s = 0; s < kStageCount; ++s) {
    if (name == kStages[s].Name) {
      *out = static_cast<Stage>(s);
      return true;
    }
  }
  return false;
}

bool RenderPlan::Compile(const PlanSpec &spec, std::shared_ptr<const RenderPlan> *out,
                         std::string &error) {
  if (spec.Outputs.empty()) {
    error = "render.outputs: a plan that requests no output renders nothing";
    return false;
  }

  Pull pull(spec);
  if (!pull.Run()) {
    error = pull.Error;
    return false;
  }

  /* A DECLARED CONTENT STAGE THAT NOTHING PULLED contributes to nothing this plan requests. It is
   * not harmless: it is a statement the consumer believes it made and the picture does not carry. */
  for (Stage s : spec.Content) {
    if (pull.HeldStage[static_cast<size_t>(s)]) { continue; }
    error = std::string("render.content.") + Row(s).Name +
            ": nothing this plan requests reads what it draws into";
    return false;
  }

  std::unique_ptr<RenderPlan> plan(new RenderPlan());
  for (size_t s = 0; s < kStageCount; ++s) {
    plan->HeldStage_[s] = pull.HeldStage[s];
    if (pull.HeldStage[s]) { plan->Order_.push_back(static_cast<Stage>(s)); }
  }
  for (size_t r = 0; r < kResourceCount; ++r) {
    plan->HeldResource_[r] = pull.HeldResource[r];
    plan->Bound_[r] = pull.Bound[r];
  }
  plan->Aliases_ = pull.Aliases;

  /* THE ONE PAIR THAT HAS A FUSED IMPLEMENTATION, and half of it is not a plan. */
  if (plan->HeldStage_[static_cast<size_t>(Stage::TemporalResolve)] &&
      !plan->HeldStage_[static_cast<size_t>(Stage::Tonemap)]) {
    error = "render.content.temporalResolve: the resolve and the display transfer are one fragment, "
            "so a plan that resolves must also request a picture -- request render.outputs.frameTex";
    return false;
  }

  /* THE OPTIONS, and an option nothing in the compiled plan reads is refused with its path. */
  if (spec.Display.IsSet() && !plan->HeldStage_[static_cast<size_t>(Stage::Tonemap)]) {
    error = "render.display: no stage of the compiled plan reads a display transfer";
    return false;
  }
  if (spec.Exposure.IsSet() && plan->HeldStage_[static_cast<size_t>(Stage::AutoExposure)]) {
    error = "render.exposure: the plan also declares render.content.autoExposure, and the metered "
            "scale is the only writer of the meter";
    return false;
  }
  if (spec.Exposure.IsSet() && !plan->HeldStage_[static_cast<size_t>(Stage::Tonemap)]) {
    error = "render.exposure: no stage of the compiled plan reads an exposure";
    return false;
  }
  plan->Display_ = spec.Display.Or(Transfer::Filmic);
  plan->Exposure_ = spec.Exposure.Or(1.0f);

  /* PASSES ARE COMPILED, NOT COUNTED. A pass is a maximal run of consecutive stages of one kind over
   * one target set -- which is what a render pass IS, and for compute (an empty target set) is R1,
   * dispatch order inside one pass. R2 is the exception: a declared pair with a fused implementation
   * becomes one pass carrying both target sets. */
  for (size_t at = 0; at < plan->Order_.size(); ++at) {
    const Stage stage = plan->Order_[at];
    const StageRow &row = Row(stage);
    bool merged = false;
    if (!plan->Passes_.empty()) {
      const Pass &open = plan->Passes_.back();
      const StageRow &last = Row(plan->Order_[open.First + open.Count - 1]);
      if (last.Kind == row.Kind) {
        bool sameTargets = true;
        for (size_t e = 0; e < kMaxEdges; ++e) {
          if (last.Contributes[e] != row.Contributes[e]) { sameTargets = false; }
        }
        if (sameTargets) {
          merged = true;
          if (row.Kind == PassKind::Compute) {
            plan->Merges_.push_back(std::string("R1 ") + last.Name + " + " + row.Name);
          }
        } else if (last.FusesInto == stage) {
          merged = true;
          plan->Fused_[static_cast<size_t>(stage)] = true;
          plan->Merges_.push_back(std::string("R2 ") + last.Name + " + " + row.Name);
        }
      }
      if (merged) { plan->Passes_.back().Count++; }
    }
    if (!merged) { plan->Passes_.push_back({row.Kind, row.Name, at, 1, AttachmentSet{}, kNoEdge}); }
  }

  for (Pass &pass : plan->Passes_) {
    if (pass.Kind == PassKind::Compute) { continue; }
    for (size_t at = 0; at < pass.Count; ++at) {
      const StageRow &row = Row(plan->Order_[pass.First + at]);
      const Resource *const edges[2] = {row.Writes, row.Contributes};
      for (const Resource *edge : edges) {
        for (size_t e = 0; e < kMaxEdges && edge[e] != kNoEdge; ++e) {
          const Resource target = edge[e];
          if (Row(target).Format == TexelFormat::Depth32Float) {
            if (pass.Depth != kNoEdge && pass.Depth != target) {
              error = std::string("render pass ") + pass.Name + ": stage " + row.Name +
                      " attaches depth " + Row(target).Name + " to a pass already attaching " +
                      Row(pass.Depth).Name;
              return false;
            }
            pass.Depth = target;
            continue;
          }
          if (!pass.Colours.Add(target)) {
            error = std::string("render pass ") + pass.Name + ": more than " +
                    std::to_string(kMaxColourAttachments) +
                    " distinct colour targets, which is the device floor";
            return false;
          }
        }
      }
    }
  }

  /* A plan with no temporal history needs no settle frames; one frame is what the device needs to
   * have submitted before there is anything to copy. */
  plan->SettleFrames_ =
      1 + (plan->HeldStage_[static_cast<size_t>(Stage::TemporalResolve)] ? kTemporalSettleFrames : 0);

  std::string material = "outshine/render-plan/1\n";
  for (Stage s : plan->Order_) { material += std::string("stage ") + Row(s).Name + "\n"; }
  for (const Pass &pass : plan->Passes_) {
    material += std::string("pass ") + (pass.Kind == PassKind::Compute ? "compute " : "raster ") +
                pass.Name + " " + std::to_string(pass.Count) + "\n";
  }
  for (const std::string &merge : plan->Merges_) { material += "merge " + merge + "\n"; }
  for (const std::string &alias : plan->Aliases_) { material += "alias " + alias + "\n"; }
  for (size_t r = 0; r < kResourceCount; ++r) {
    if (!plan->HeldResource_[r]) { continue; }
    material += std::string("resource ") + kResources[r].Name + " " +
                FormatName(kResources[r].Format) + "\n";
  }
  material += std::string("display ") + TransferName(plan->Display_) + "\n";
  material += "exposure " + Decimal(plan->Exposure_) + "\n";
  plan->Digest_ = Sha256Hex(material).substr(0, 16);

  *out = std::shared_ptr<const RenderPlan>(plan.release());
  return true;
}

} // namespace outshine::Render
