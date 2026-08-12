/* THE PASS DESCRIPTOR'S TARGET SET, judged with no device in the process. It used to be rebuilt at
 * encode time from the pass's stages with a duplicate guard that compared texture views -- and the
 * renderer's `View()` answers `CreateView()`, a new object every call, so the guard never fired and
 * every stage of a pass wrote its own copy of a shared target. Eight content stages produced sixteen
 * entries against a device floor of eight.
 *
 * The union is now the plan's, computed once, and the type that holds it absorbs a repeat and
 * refuses beyond the floor. What this test measures is the property the defect violated: the number
 * of colour attachments a pass declares equals the number of DISTINCT targets its stages name, never
 * the number of stage-target edges. */
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

/* The distinct colour targets of a pass, counted here from the catalogue rows independently of the
 * plan's own set -- otherwise the claim would be that the set agrees with itself. */
size_t DistinctColourTargets(const RenderPlan &plan, const RenderPlan::Pass &pass) {
  std::set<Resource> seen;
  for (size_t at = 0; at < pass.Count; ++at) {
    const auto &row = Row(plan.Order()[pass.First + at]);
    const Resource *const edges[2] = {row.Writes, row.Contributes};
    for (const Resource *edge : edges) {
      for (size_t e = 0; e < kMaxEdges && edge[e] != kNoEdge; ++e) {
        if (Row(edge[e]).Format != TexelFormat::Depth32Float) { seen.insert(edge[e]); }
      }
    }
  }
  return seen.size();
}

/* The number of stage-target EDGES, which is what the defective guard let through. */
size_t ColourEdges(const RenderPlan &plan, const RenderPlan::Pass &pass) {
  size_t edgeCount = 0;
  for (size_t at = 0; at < pass.Count; ++at) {
    const auto &row = Row(plan.Order()[pass.First + at]);
    const Resource *const edges[2] = {row.Writes, row.Contributes};
    for (const Resource *edge : edges) {
      for (size_t e = 0; e < kMaxEdges && edge[e] != kNoEdge; ++e) {
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

} // namespace

int main() {
  using namespace outshine::Test;

  /* THE SET ITSELF, before any plan: a repeat is absorbed and the ninth distinct entry is refused
   * rather than written past the end. */
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

  /* THE COVERAGE PLAN, which is what every render case in the tree compiles today. */
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

  /* EVERY CONTENT STAGE THE CATALOGUE HAS, into a surface. This is the declaration the defect was
   * measured on: with the target set rebuilt per stage it produced twenty entries in an array of
   * sixteen. Here it is a plan that compiles, or a refusal that names the pass. */
  {
    PlanSpec spec;
    spec.Outputs = {Resource::Surface};
    spec.Content = {Stage::Sky,     Stage::Sun,          Stage::Moon,      Stage::Stars,
                    Stage::Terrain, Stage::Buildings,    Stage::Water,     Stage::Models,
                    Stage::Subjects, Stage::ShadowMap,   Stage::Occlusion, Stage::TemporalResolve,
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

  Covers("I.27 the pass descriptor is the union of its stages' targets, one entry per distinct "
         "resource, and a pass wider than the device floor is refused where the plan is compiled");
  return Report();
}
