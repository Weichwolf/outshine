#include "Compiled.h"

#include <array>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <vector>
#include <string>
#include <optional>
#include <string_view>
#include <expected>
#include <memory>
#include <utility>

#include "Sha256.h"

namespace outshine::Render {
namespace {

struct Pull {
  const PlanSpec &Spec;
  std::array<bool, kStageCount> Declared = {{}};
  std::array<bool, kStageCount> HeldStage = {{}};
  std::array<bool, kResourceCount> HeldResource = {{}};
  std::array<bool, kResourceCount> Seen = {{}};
  std::array<Resource, kResourceCount> Bound = {{}};
  std::array<Resource, kResourceCount> Wanted = {{}};
  std::array<bool, kResourceCount> Queued = {{}};
  size_t WantedCount = 0;
  std::vector<std::string> Aliases;
  std::string Error;

  explicit Pull(const PlanSpec &spec) : Spec(spec) {
    for (size_t r = 0; r < kResourceCount; ++r) { Bound[r] = static_cast<Resource>(r); }
    for (const Stage s : spec.Content) { Declared[static_cast<size_t>(s)] = true; }
  }

  void Want(Resource r) {
    const auto index = static_cast<size_t>(r);
    if (Queued[index]) { return; }
    Queued[index] = true;
    Wanted[WantedCount++] = r;
  }

  void Hold(Stage s) {
    if (HeldStage[static_cast<size_t>(s)]) { return; }
    HeldStage[static_cast<size_t>(s)] = true;
    const StageRow &row = Row(s);
    for (size_t e = 0; e < kMaxEdges && row.Reads[e] != kNoEdge; ++e) { Want(row.Reads[e]); }
    for (size_t e = 0; e < kMaxEdges && row.ReadsLastFrame[e] != kNoEdge; ++e) {
      Want(row.ReadsLastFrame[e]);
    }
    for (size_t e = 0; e < kMaxEdges && row.Contributes[e] != kNoEdge; ++e) {
      if (Row(row.Contributes[e]).Format == TexelFormat::Depth32Float) { Want(row.Contributes[e]); }
    }
  }

  void Missing(Resource r, const char *why) {
    Error += Error.empty() ? "render.outputs: " : "; also ";
    Error += std::string(Row(r).Name) + " " + why;
    for (size_t s = 0; s < kStageCount; ++s) {
      const auto id = static_cast<Stage>(s);
      if (Produces(id, r) && Row(id).From == Provenance::Content) {
        Error += std::string(" -- declare render.content.") + Row(id).Name + " to supply it";
        return;
      }
    }
  }

  [[nodiscard]] bool Resolve(Resource r) {
    const auto index = static_cast<size_t>(r);
    if (Seen[index]) { return true; }
    Seen[index] = true;
    const ResourceRow &row = Row(r);
    if (row.Kind == ResourceKind::Given) {
      HeldResource[index] = true;
      return true;
    }

    size_t contributors = 0;
    for (size_t s = 0; s < kStageCount; ++s) {
      const auto id = static_cast<Stage>(s);
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
        Aliases.push_back(std::string(row.Name) + " -> neutral");
        return true;
      case FallbackKind::None:
        Missing(r,
                row.Kind == ResourceKind::Attachment
                    ? "is an attachment of this plan and nothing draws into it, so the plan would "
                      "compile, run and render black"
                    : "has no producer in this plan");
        return false;
    }
    return false;
  }

  [[nodiscard]] bool Run() {
    for (const Resource r : Spec.Outputs) { Want(r); }
    size_t drained = 0;
    while (drained < WantedCount) { (void)Resolve(Wanted[drained++]); }
    return Error.empty();
  }
};

namespace Says {
constexpr auto MissingOutput = "render.outputs: a plan that requests no output renders nothing";
constexpr auto InvalidResource = "render.outputs: unknown resource identifier";
constexpr auto InvalidStage = "render.content: unknown stage identifier";
constexpr auto InvalidTransfer = "render.display: unknown transfer";
constexpr auto InvalidPrecision = "render.precision: unknown scene precision";
constexpr auto InvalidExposure = "render.exposure: requires a finite nonnegative scale";
}

bool ValidateSpec(const PlanSpec &spec, std::string &error) {
  if (spec.Outputs.empty()) {
    error = Says::MissingOutput;
    return false;
  }
  for (const Resource resource : spec.Outputs) {
    if (static_cast<size_t>(resource) >= kResourceCount) {
      error = Says::InvalidResource;
      return false;
    }
  }
  for (const Stage stage : spec.Content) {
    if (static_cast<size_t>(stage) >= kStageCount) {
      error = Says::InvalidStage;
      return false;
    }
  }
  const auto transfer = spec.Display.Or(Transfer::Filmic);
  if (transfer != Transfer::Linear && transfer != Transfer::Filmic) {
    error = Says::InvalidTransfer;
    return false;
  }
  const auto precision = spec.Precision.Or(ScenePrecision::Half);
  if (precision != ScenePrecision::Half && precision != ScenePrecision::Float) {
    error = Says::InvalidPrecision;
    return false;
  }
  const float exposure = spec.Exposure.Or(1.0f);
  if (!std::isfinite(exposure) || exposure < 0) {
    error = Says::InvalidExposure;
    return false;
  }
  return true;
}

struct StagePair {
  const StageRow &Earlier;
  const StageRow &Incoming;
};

bool ComputeWriteConflict(StagePair stages) {
  for (const Resource write : stages.Incoming.Writes) {
    if (write == kNoEdge) { break; }
    for (const Resource read : stages.Earlier.Reads) {
      if (read == kNoEdge) { break; }
      if (read == write) { return true; }
    }
    for (const Resource previous : stages.Earlier.Writes) {
      if (previous == kNoEdge) { break; }
      if (Row(write).Format == TexelFormat::Handle || Row(previous).Format == TexelFormat::Handle) {
        continue;
      }
      if (IsBuffer(Row(write)) == IsBuffer(Row(previous))) { return true; }
    }
  }
  return false;
}

bool ReadsEarlierWrite(StagePair stages) {
  for (const Resource write : stages.Earlier.Writes) {
    if (write == kNoEdge) { break; }
    for (const Resource read : stages.Incoming.Reads) {
      if (read == kNoEdge) { break; }
      if (read == write) { return true; }
    }
  }
  return false;
}

const char *TransferName(Transfer t) {
  return t == Transfer::Linear ? "linear" : "filmic";
}

const char *FormatName(TexelFormat f) {
  switch (f) {
    case TexelFormat::Handle: return "handle";
    case TexelFormat::Table: return "table";
    case TexelFormat::Rgba16Float: return "rgba16float";
    case TexelFormat::Rgba32Float: return "rgba32float";
    case TexelFormat::Rg16Float: return "rg16float";
    case TexelFormat::R8Unorm: return "r8unorm";
    case TexelFormat::Rgba8UnormSrgb: return "rgba8unorm-srgb";
    case TexelFormat::Depth32Float: return "depth32float";
  }
  return "";
}

std::string Decimal(float value) {
  std::array<char, 32> text = {{}};
  std::snprintf(text.data(), text.size(), "%.9g", static_cast<double>(value));
  return text.data();
}

}

std::optional<Stage> Compiled::StageByName(std::string_view name) {
  for (size_t s = 0; s < kStageCount; ++s) {
    if (name == kStages[s].Name) { return static_cast<Stage>(s); }
  }
  return std::nullopt;
}

std::optional<Resource> Compiled::ResourceByName(std::string_view name) {
  for (size_t at = 0; at < static_cast<size_t>(Resource::kCount); ++at) {
    if (name == kResources[at].Name) { return static_cast<Resource>(at); }
  }
  return std::nullopt;
}

std::expected<std::shared_ptr<const Compiled>, std::string>
Compiled::Compile(const PlanSpec &spec) {
  std::string error;
  if (!ValidateSpec(spec, error)) { return std::unexpected(std::move(error)); }
  std::unique_ptr<Compiled> plan(new Compiled());
  if (!plan->ResolveDependencies(spec, error) || !plan->ConfigureOutput(spec, error)) {
    return std::unexpected(std::move(error));
  }
  plan->BuildPasses();
  if (!plan->AttachTargets(error)) { return std::unexpected(std::move(error)); }
  plan->PlanStorage(spec);
  plan->BuildDigest();
  return std::shared_ptr<const Compiled>(std::move(plan));
}

bool Compiled::ResolveDependencies(const PlanSpec &spec, std::string &error) {
  Pull pull(spec);
  if (!pull.Run()) {
    error = pull.Error;
    return false;
  }

  for (const Stage s : spec.Content) {
    if (pull.HeldStage[static_cast<size_t>(s)]) { continue; }
    error = std::string("render.content.") + Row(s).Name +
            ": nothing this plan requests reads what it draws into";
    return false;
  }

  for (size_t s = 0; s < kStageCount; ++s) {
    HeldStage_[s] = pull.HeldStage[s];
    if (pull.HeldStage[s]) { Order_.push_back(static_cast<Stage>(s)); }
  }
  for (size_t r = 0; r < kResourceCount; ++r) {
    HeldResource_[r] = pull.HeldResource[r];
    Bound_[r] = pull.Bound[r];
    Format_[r] = kResources[r].Format;
  }

  for (size_t r = 0; r < kResourceCount; ++r) {
    Resource at = Bound_[r];
    for (size_t step = 0; step < kResourceCount; ++step) {
      const Resource next = pull.Bound[static_cast<size_t>(at)];
      if (next == at) { break; }
      at = next;
    }
    Bound_[r] = at;
  }
  Aliases_ = pull.Aliases;

  return true;
}

bool Compiled::ConfigureOutput(const PlanSpec &spec, std::string &error) {
  if (spec.Precision.IsSet() && !HeldResource_[static_cast<size_t>(Resource::SceneHdr)]) {
    error = "render.precision: no resource of the compiled plan carries scene-referred radiance";
    return false;
  }
  if (spec.Precision.Or(ScenePrecision::Half) == ScenePrecision::Float) {
    for (size_t at = 0; at < kResourceCount; ++at) {
      const auto resource = static_cast<Resource>(at);
      if (CarriesSceneRadiance(resource)) { Format_[at] = TexelFormat::Rgba32Float; }
    }
  }
  Precision_ = spec.Precision.Or(ScenePrecision::Half);

  if (HeldStage_[static_cast<size_t>(Stage::TemporalResolve)] &&
      !HeldStage_[static_cast<size_t>(Stage::Tonemap)]) {
    error =
        "render.content.temporalResolve: the resolve and the display transfer are one fragment, "
        "so a plan that resolves must also request a picture -- request render.outputs.frameTex";
    return false;
  }

  if (spec.Display.IsSet() && !HeldStage_[static_cast<size_t>(Stage::Tonemap)]) {
    error = "render.display: no stage of the compiled plan reads a display transfer";
    return false;
  }
  if (spec.Exposure.IsSet() && HeldStage_[static_cast<size_t>(Stage::AutoExposure)]) {
    error = "render.exposure: the plan also declares render.content.autoExposure, and the metered "
            "scale is the only writer of the meter";
    return false;
  }
  if (spec.Exposure.IsSet() && !HeldStage_[static_cast<size_t>(Stage::Tonemap)]) {
    error = "render.exposure: no stage of the compiled plan reads an exposure";
    return false;
  }
  Display_ = spec.Display.Or(Transfer::Filmic);
  Exposure_ = spec.Exposure.Or(1.0f);

  return true;
}

bool Compiled::CanSharePass(const Pass &pass, const StageRow &row) const {
  const StageRow &last = Row(Order_[pass.First + pass.Count - 1]);
  if (last.Contributes != row.Contributes) { return false; }
  for (size_t held = 0; held < pass.Count; ++held) {
    const StagePair stages{.Earlier = Row(Order_[pass.First + held]), .Incoming = row};
    if (row.Kind == PassKind::Compute && ComputeWriteConflict(stages)) { return false; }
    if (ReadsEarlierWrite(stages)) { return false; }
  }
  return true;
}

bool Compiled::MergeStage(Pass &pass, Stage stage) {
  const StageRow &row = Row(stage);
  const StageRow &last = Row(Order_[pass.First + pass.Count - 1]);
  if (last.Kind != row.Kind) { return false; }
  if (CanSharePass(pass, row)) {
    if (row.Kind == PassKind::Compute) {
      Merges_.push_back(std::string("R1 ") + last.Name + " + " + row.Name);
    }
    return true;
  }
  if (last.FusesInto != stage) { return false; }
  Fused_[static_cast<size_t>(stage)] = true;
  Merges_.push_back(std::string("R2 ") + last.Name + " + " + row.Name);
  return true;
}

void Compiled::BuildPasses() {
  for (size_t at = 0; at < Order_.size(); ++at) {
    const Stage stage = Order_[at];
    const StageRow &row = Row(stage);
    if (!Passes_.empty() && MergeStage(Passes_.back(), stage)) {
      ++Passes_.back().Count;
      continue;
    }
    Passes_.push_back({.Kind = row.Kind,
                       .Name = row.Name,
                       .First = at,
                       .Count = 1,
                       .Targets = AttachmentSet{},
                       .Buffers = AttachmentSet{},
                       .Depth = kNoEdge});
  }
}

bool Compiled::AttachComputeTargets(Pass &pass, std::string &error) const {
  for (size_t at = 0; at < pass.Count; ++at) {
    const StageRow &row = Row(Order_[pass.First + at]);
    for (size_t e = 0; e < kMaxEdges && row.Writes[e] != kNoEdge; ++e) {
      const Resource target = row.Writes[e];
      if (!HeldResource_[static_cast<size_t>(target)]) { continue; }

      if (IsBuffer(Row(target))) {
        if (pass.Buffers.Add(target)) { continue; }
        error = std::string("compute pass ") + pass.Name + ": more than " +
                std::to_string(kMaxColourAttachments) +
                " distinct table targets, which is the device floor";
        return false;
      }
      if (Row(target).Format == TexelFormat::Handle) { continue; }
      if (!pass.Targets.Add(target)) {
        error = std::string("compute pass ") + pass.Name + ": more than " +
                std::to_string(kMaxColourAttachments) +
                " distinct storage targets, which is the device floor";
        return false;
      }
    }
  }
  return true;
}

bool Compiled::AttachRasterTargets(Pass &pass, std::string &error) const {
  for (size_t at = 0; at < pass.Count; ++at) {
    const StageRow &row = Row(Order_[pass.First + at]);
    const std::array<const Resource *const, 2> edges = {row.Writes.data(), row.Contributes.data()};
    for (const Resource *edge : edges) {
      for (size_t e = 0; e < kMaxEdges && edge[e] != kNoEdge; ++e) {
        const Resource target = edge[e];

        if (!HeldResource_[static_cast<size_t>(target)]) { continue; }
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
        if (!pass.Targets.Add(target)) {
          error = std::string("render pass ") + pass.Name + ": more than " +
                  std::to_string(kMaxColourAttachments) +
                  " distinct colour targets, which is the device floor";
          return false;
        }
      }
    }
  }
  return true;
}

bool Compiled::AttachTargets(std::string &error) {
  for (Pass &pass : Passes_) {
    const bool attached = pass.Kind == PassKind::Compute ? AttachComputeTargets(pass, error)
                                                         : AttachRasterTargets(pass, error);
    if (!attached) { return false; }
  }
  return true;
}

bool Compiled::IsRead(Resource resource) const {
  for (const Stage held : Order_) {
    const StageRow &row = Row(held);
    for (const Resource read : row.Reads) {
      if (read == kNoEdge) { break; }
      if (Bound(read) == Bound(resource)) { return true; }
    }
    for (const Resource read : row.ReadsLastFrame) {
      if (read == kNoEdge) { break; }
      if (Bound(read) == Bound(resource)) { return true; }
    }
  }
  return false;
}

void Compiled::PlanStorage(const PlanSpec &spec) {
  std::array<size_t, kResourceCount> attachedInPasses = {{}};
  for (const Pass &pass : Passes_) {
    for (const Resource target : pass.Targets) { ++attachedInPasses[static_cast<size_t>(target)]; }
    if (pass.Depth != kNoEdge) { ++attachedInPasses[static_cast<size_t>(pass.Depth)]; }
  }
  for (size_t r = 0; r < kResourceCount; ++r) {
    const auto id = static_cast<Resource>(r);
    if (!HeldResource_[r]) { continue; }
    bool wanted = id == Resource::Surface;
    for (const Resource asked : spec.Outputs) {
      if (Bound(asked) == Bound(id)) { wanted = true; }
    }
    Stored_[r] = IsRead(id) || wanted || attachedInPasses[r] > 1;
  }
  SettleFrames_ =
      1 + (HeldStage_[static_cast<size_t>(Stage::TemporalResolve)] ? kTemporalSettleFrames : 0);
}

void Compiled::BuildDigest() {
  std::string material = "outshine/render-plan/1\n";
  for (const Stage s : Order_) { material += std::string("stage ") + Row(s).Name + "\n"; }
  for (const Pass &pass : Passes_) {
    material += std::string("pass ") + (pass.Kind == PassKind::Compute ? "compute " : "raster ") +
                pass.Name + " " + std::to_string(pass.Count) + "\n";
  }
  for (const std::string &merge : Merges_) { material += "merge " + merge + "\n"; }
  for (const std::string &alias : Aliases_) { material += "alias " + alias + "\n"; }
  for (size_t r = 0; r < kResourceCount; ++r) {
    if (!HeldResource_[r]) { continue; }
    material += std::string("resource ") + kResources[r].Name + " " + FormatName(Format_[r]) + "\n";
  }
  material += std::string("display ") + TransferName(Display_) + "\n";
  material += "exposure " + Decimal(Exposure_) + "\n";
  Digest_ = Sha256Hex(material).substr(0, 16);
}

}
