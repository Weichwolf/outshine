#include <memory>
#include <set>
#include <string>
#include <utility>

#include "Check.h"

#include "RenderPlan.h"

using outshine::Render::AttachmentSet;
using outshine::Render::kMaxColourAttachments;
using outshine::Render::kMaxEdges;
using outshine::Render::kNoEdge;
using outshine::Render::PassKind;
using outshine::Render::PlanSpec;
using outshine::Render::RenderPlan;
using outshine::Render::Resource;
using outshine::Render::Row;
using outshine::Render::Stage;
using outshine::Render::TexelFormat;
using outshine::Render::Transfer;

namespace {

size_t DistinctColourTargets(const RenderPlan &plan, const RenderPlan::Pass &pass) {
  std::set<Resource> seen;
  for (size_t at = 0; at < pass.Count; ++at) {
    const auto &row = Row(plan.Order()[pass.First + at]);
    const Resource *const edges[2] = {row.Writes, row.Contributes};
    for (const Resource *edge : edges) {
      for (size_t e = 0; e < kMaxEdges && edge[e] != kNoEdge; ++e) {

        if (!plan.Holds(edge[e])) { continue; }
        if (Row(edge[e]).Format != TexelFormat::Depth32Float) { seen.insert(edge[e]); }
      }
    }
  }
  return seen.size();
}

size_t ColourEdges(const RenderPlan &plan, const RenderPlan::Pass &pass) {
  size_t edgeCount = 0;
  for (size_t at = 0; at < pass.Count; ++at) {
    const auto &row = Row(plan.Order()[pass.First + at]);
    const Resource *const edges[2] = {row.Writes, row.Contributes};
    for (const Resource *edge : edges) {
      for (size_t e = 0; e < kMaxEdges && edge[e] != kNoEdge; ++e) {
        if (!plan.Holds(edge[e])) { continue; }
        if (Row(edge[e]).Format != TexelFormat::Depth32Float) { ++edgeCount; }
      }
    }
  }
  return edgeCount;
}

void JudgePlan(const RenderPlan &plan, const char *what) {
  using outshine::Test::Note;
  size_t widest = 0, widestEdges = 0;
  for (const RenderPlan::Pass &pass : plan.Passes()) {
    const size_t distinct = DistinctColourTargets(plan, pass);
    const size_t edgeCount = ColourEdges(plan, pass);
    if (pass.Kind == PassKind::Compute) {
      size_t writes = 0;
      std::set<Resource> stored;
      for (size_t at = 0; at < pass.Count; ++at) {
        const outshine::Render::StageRow &row = Row(plan.Order()[pass.First + at]);
        for (size_t e = 0; e < kMaxEdges && row.Writes[e] != kNoEdge; ++e) {
          if (!plan.Holds(row.Writes[e])) { continue; }
          if (Row(row.Writes[e]).Format == TexelFormat::Handle) { continue; }
          if (stored.insert(row.Writes[e]).second) { ++writes; }
        }
      }
      CHECK(pass.Targets.Size() == writes,
            "**A COMPUTE PASS ATTACHES EVERY TEXTURE IT WRITES AND NOTHING ELSE.** This claim used "
            "to read 'a compute pass attaches nothing at all', which was true only while the "
            "device ran no compute stage -- a test that records an absent capability goes red the "
            "day the capability arrives, which is what it is for. A device binds its read-write "
            "storage textures when the pass OPENS, so the plan owes the renderer that set before "
            "a single dispatch is encoded");
      for (const Resource target : pass.Targets) {
        CHECK(stored.count(target) == 1,
              "and every one of them is a resource some stage of this pass declared it writes");
      }
      CHECK(pass.Depth == kNoEdge,
            "and a compute pass never attaches depth, because there is no rasteriser in it to "
            "test against one");
      continue;
    }
    CHECK(pass.Targets.Size() == distinct,
          "a raster pass attaches each distinct colour target exactly once");
    CHECK(pass.Targets.Size() <= kMaxColourAttachments,
          "no pass exceeds the device's colour-attachment floor");
    std::set<Resource> written;
    for (const Resource target : pass.Targets) {
      CHECK(written.insert(target).second, "no target appears twice in one pass's set");
      CHECK(Row(target).Format != TexelFormat::Depth32Float,
            "a depth target is never a colour attachment");
    }
    if (edgeCount > widestEdges) { widestEdges = edgeCount; }
    if (pass.Targets.Size() > widest) { widest = pass.Targets.Size(); }
  }
  Note((std::string(what) + " widest pass, distinct colour targets").c_str(), (double)widest,
       "attachments");
  Note((std::string(what) + " widest pass, stage-target edges").c_str(), (double)widestEdges,
       "edges");
}

}

int main() {
  using namespace outshine::Test;

  {
    AttachmentSet set;
    CHECK(set.Empty(), "a fresh set attaches nothing");
    CHECK(set.Add(Resource::SceneHdr) && set.Add(Resource::SceneHdr) && set.Add(Resource::SceneHdr),
          "the same target added three times is accepted three times");
    CHECK(set.Size() == 1u, "and appears once");
    const Resource distinct[kMaxColourAttachments] = {
        Resource::SceneVelocity,  Resource::AoBuffer,  Resource::SceneLinear, Resource::FrameTex,
        Resource::Surface,        Resource::Meter,     Resource::SkyViewLut,
        Resource::TransmittanceLut};
    bool allAdded = true;
    for (const Resource target : distinct) { allAdded = set.Add(target) && allAdded; }
    CHECK(!allAdded, "the ninth distinct target is refused rather than written past the end");
    CHECK(set.Size() == kMaxColourAttachments, "and the set stops at the floor");
  }

  {
    PlanSpec spec;
    spec.Outputs = {Resource::SceneDepth, Resource::FrameTex};
    spec.Content = {Stage::Subjects};
    spec.Display = outshine::Render::Declared<Transfer>(Transfer::Linear);
    spec.Exposure = outshine::Render::Declared<float>(1.0f);
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = [&] { auto made = RenderPlan::Compile(spec); if (made) { plan = *std::move(made); return true; } why = std::move(made).error(); return false; }();
    CHECK(compiled, "the coverage declaration compiles");
    if (compiled) { JudgePlan(*plan, "coverage plan"); }
  }

  {
    PlanSpec spec;
    spec.Outputs = {Resource::Surface};
    spec.Content = {Stage::Sky,     Stage::Sun,          Stage::Moon,      Stage::Stars,
                    Stage::Terrain, Stage::Buildings,    Stage::Water,     Stage::Models,
                    Stage::Subjects, Stage::LightVisibility,   Stage::AmbientOcclusion, Stage::TemporalResolve,
                    Stage::AutoExposure};
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = [&] { auto made = RenderPlan::Compile(spec); if (made) { plan = *std::move(made); return true; } why = std::move(made).error(); return false; }();
    CHECK(compiled, "a declaration naming every content stage compiles");
    if (compiled) {
      JudgePlan(*plan, "every-content plan");
      Note("every-content plan stages", (double)plan->Order().size(), "stages");
      Note("every-content plan passes", (double)plan->Passes().size(), "passes");
    } else {
      Note(("every-content refusal: " + why).c_str());
    }
  }

  {
    PlanSpec unread;
    unread.Outputs = {Resource::FrameTex};
    unread.Content = {Stage::Subjects, Stage::Tonemap};
    PlanSpec read = unread;
    read.Content = {Stage::Subjects, Stage::TemporalResolve, Stage::Tonemap};

    std::shared_ptr<const RenderPlan> without, with;
    std::string why;
    const bool bothCompiled = [&] { auto made = RenderPlan::Compile(unread); if (made) { without = *std::move(made); return true; } why = std::move(made).error(); return false; }() &&
                              [&] { auto made = RenderPlan::Compile(read); if (made) { with = *std::move(made); return true; } why = std::move(made).error(); return false; }();
    CHECK(bothCompiled, "both plans compile: one whose stages read the velocity target and one "
                        "whose stages only draw into it");
    if (bothCompiled) {
      CHECK(!without->Holds(Resource::SceneVelocity),
            "a colour target no held stage reads is not held, so the plan neither allocates nor "
            "attaches it");
      CHECK(with->Holds(Resource::SceneVelocity),
            "the same target IS held where a declared stage reads it, so the prune removes what is "
            "unread rather than what is merely contributed to");
      size_t attachedWithout = 0, attachedWith = 0;
      for (const RenderPlan::Pass &pass : without->Passes()) {
        for (const Resource colour : pass.Targets) {
          attachedWithout += colour == Resource::SceneVelocity ? 1u : 0u;
        }
      }
      for (const RenderPlan::Pass &pass : with->Passes()) {
        for (const Resource colour : pass.Targets) {
          attachedWith += colour == Resource::SceneVelocity ? 1u : 0u;
        }
      }
      CHECK(attachedWithout == 0 && attachedWith == 1,
            "the two compiled plans DIFFER in what they attach, which is the property a backward "
            "closure has and a forward one cannot");

      CHECK(without->Holds(Resource::SceneDepth) && with->Holds(Resource::SceneDepth),
            "the depth target is held in both, because the depth test consumes it inside the pass "
            "and no stage reads it as a resource");
      Note("velocity attachments, plan whose stages only draw into it", (double)attachedWithout,
           "attachments");
      Note("velocity attachments, plan with a reader", (double)attachedWith, "attachments");
    } else {
      Note(("prune pair refusal: " + why).c_str());
    }
  }

  {
    outshine::Render::PlanSpec bare;
    bare.Outputs = {Resource::FrameTex};
    bare.Content = {outshine::Render::Stage::Subjects};
    std::shared_ptr<const RenderPlan> plan;
    std::string error;
    CHECK([&] { auto made = RenderPlan::Compile(bare); if (made) { plan = *std::move(made); return true; } error = std::move(made).error(); return false; }(), "a plain subjects plan compiles");
    if (plan) {
      CHECK(!plan->Stored(Resource::SceneShadingNormal) &&
                !plan->Stored(Resource::SceneSurfaceIdentity),
            "**AN ATTACHMENT NOBODY READS AND NOBODY ASKED FOR IS NOT STORED.** The shading "
            "normal and the surface identity are written by the subjects pass and read by no "
            "stage of this plan and no declared output -- deriving DONT_CARE for them saves 24 "
            "bytes per pixel of tile write-back on a TBDR device, per frame, for nothing");
      CHECK(plan->Stored(Resource::FrameTex) && plan->Stored(Resource::SceneDepth),
            "while the frame the consumer asked for and the depth the tonemap reads are stored");
    }
    outshine::Render::PlanSpec parity = bare;
    parity.Outputs.push_back(Resource::SceneShadingNormal);
    std::shared_ptr<const RenderPlan> kept;
    CHECK([&] { auto made = RenderPlan::Compile(parity); if (made) { kept = *std::move(made); return true; } error = std::move(made).error(); return false; }() && kept &&
              kept->Stored(Resource::SceneShadingNormal),
          "**AND ASKING FOR IT IS ENOUGH**: the parity harness declares the normal as an output "
          "and the derivation keeps it -- the consumer decides what survives the pass, in the "
          "declaration and nowhere else");
  }

  Covers("I.84 the pass descriptor is the union of its stages' targets, one entry per distinct "
         "resource, and a pass wider than the device floor is refused where the plan is compiled");
  return Report();
}
