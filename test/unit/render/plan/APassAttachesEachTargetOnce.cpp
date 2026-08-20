#include <memory>
#include <set>
#include <string>

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
      CHECK(pass.Colours.Empty() && pass.Depth == kNoEdge,
            "a compute pass attaches nothing at all");
      continue;
    }
    CHECK(pass.Colours.Size() == distinct,
          "a raster pass attaches each distinct colour target exactly once");
    CHECK(pass.Colours.Size() <= kMaxColourAttachments,
          "no pass exceeds the device's colour-attachment floor");
    std::set<Resource> written;
    for (const Resource target : pass.Colours) {
      CHECK(written.insert(target).second, "no target appears twice in one pass's set");
      CHECK(Row(target).Format != TexelFormat::Depth32Float,
            "a depth target is never a colour attachment");
    }
    if (edgeCount > widestEdges) { widestEdges = edgeCount; }
    if (pass.Colours.Size() > widest) { widest = pass.Colours.Size(); }
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
    const bool compiled = RenderPlan::Compile(spec, &plan, why);
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
    const bool compiled = RenderPlan::Compile(spec, &plan, why);
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
    const bool bothCompiled = RenderPlan::Compile(unread, &without, why) &&
                              RenderPlan::Compile(read, &with, why);
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
        for (const Resource colour : pass.Colours) {
          attachedWithout += colour == Resource::SceneVelocity ? 1u : 0u;
        }
      }
      for (const RenderPlan::Pass &pass : with->Passes()) {
        for (const Resource colour : pass.Colours) {
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

  Covers("I.27 the pass descriptor is the union of its stages' targets, one entry per distinct "
         "resource, and a pass wider than the device floor is refused where the plan is compiled");
  return Report();
}
