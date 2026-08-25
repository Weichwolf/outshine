#include <memory>
#include <string>
#include <utility>

#include "Check.h"

#include "RenderPlan.h"

using outshine::Render::PlanSpec;
using outshine::Render::RenderPlan;
using outshine::Render::Resource;
using outshine::Render::Stage;
using outshine::Render::Transfer;

namespace {

PlanSpec CoverageSpec() {
  PlanSpec spec;
  spec.Outputs = {Resource::SceneDepth, Resource::FrameTex};
  spec.Content = {Stage::Subjects};
  spec.Display = outshine::Render::Declared<Transfer>(Transfer::Linear);
  spec.Exposure = outshine::Render::Declared<float>(1.0f);
  return spec;
}

int PassesOf(const RenderPlan &plan) { return plan.PassCount(); }

}

int main() {
  using namespace outshine::Test;

  {
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = [&] { auto made = RenderPlan::Compile(CoverageSpec()); if (made) { plan = *std::move(made); return true; } why = std::move(made).error(); return false; }();
    CHECK(compiled, "the coverage declaration compiles");
    if (compiled) {
      CHECK(PassesOf(*plan) <= 2,
            "a plan that requests a depth buffer and a picture over one drawn subject compiles to "
            "at most two passes");
      Note("coverage plan passes", PassesOf(*plan), "passes");
      Note("coverage plan stages", (double)plan->Order().size(), "stages");
      CHECK(!plan->Holds(Stage::Sky) && !plan->Holds(Stage::MediumTransmittance) &&
                !plan->Holds(Stage::AutoExposure) && !plan->Holds(Stage::AmbientOcclusion) &&
                !plan->Holds(Stage::TemporalResolve) && !plan->Holds(Stage::Present),
            "nothing the declaration did not ask for is in the plan -- no atmosphere chain, no "
            "meter, no occlusion, no resolve, no present");
      CHECK(plan->Holds(Stage::Tonemap),
            "the tonemap is machinery and was pulled by the requested picture rather than declared");
      CHECK(plan->Bound(Resource::SceneLinear) == Resource::SceneHdr,
            "with no temporal resolve declared, a reader of the linear resolve binds the scene "
            "target it falls back to");

      size_t bound = 0, neutral = 0;
      for (const std::string &alias : plan->Aliases()) {
        if (alias.find("-> neutral") != std::string::npos) { ++neutral; } else { ++bound; }
      }
      CHECK(bound == 2u && neutral == 3u,
            "the plan publishes every alias it applied: two rebindings (composited to hdr, "
            "linear to composited) and three neutral stand-ins (the meter, the occlusion buffer "
            "and the shadow atlas) -- a silent neutral was how an unshadowed picture used to be "
            "indistinguishable from a shadowed one");
      CHECK(plan->SettleFrames() == 1,
            "a plan with no temporal history needs no settle frames beyond the one the device needs "
            "to have submitted something to copy");
      CHECK(!plan->Digest().empty(), "the plan carries its own digest");
      Note(("coverage plan digest " + plan->Digest()).c_str());
    }
  }

  {
    std::shared_ptr<const RenderPlan> first, second;
    std::string why;
    const bool both = [&] { auto made = RenderPlan::Compile(CoverageSpec()); if (made) { first = *std::move(made); return true; } why = std::move(made).error(); return false; }() &&
                      [&] { auto made = RenderPlan::Compile(CoverageSpec()); if (made) { second = *std::move(made); return true; } why = std::move(made).error(); return false; }();
    CHECK(both, "one declaration compiles twice");
    if (both) {
      CHECK(first->Digest() == second->Digest(), "two compilations of one declaration agree");
    }
  }

  {
    PlanSpec spec;
    spec.Outputs = {Resource::Surface};
    spec.Content = {Stage::Sky, Stage::Sun, Stage::LightVisibility, Stage::Terrain, Stage::AmbientOcclusion,
                    Stage::TemporalResolve, Stage::AutoExposure};
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = [&] { auto made = RenderPlan::Compile(spec); if (made) { plan = *std::move(made); return true; } why = std::move(made).error(); return false; }();
    CHECK(compiled, "a declaration that asks for a lit picture compiles");
    if (compiled) {
      CHECK(plan->Holds(Stage::MediumTransmittance) && plan->Holds(Stage::MediumMultiScatter) &&
                plan->Holds(Stage::MediumRadiance) && plan->Holds(Stage::Irradiance),
            "the atmosphere chain was pulled by the sky and the meter and is in the plan without "
            "the declaration naming one of its four stages");
      CHECK(plan->Holds(Stage::Present) && plan->Holds(Stage::Tonemap),
            "the tonemap and the present were pulled by the requested surface");
      CHECK(plan->Fused(Stage::Tonemap),
            "R2 fused the temporal resolve with the display transfer into one pass");
      CHECK(plan->Bound(Resource::SceneLinear) == Resource::SceneLinear,
            "with the resolve declared, its readers bind the resolve's own attachment");
      CHECK(plan->SettleFrames() > 1, "a plan with a temporal history states how long it takes");
      Note("picture plan passes", PassesOf(*plan), "passes");
      Note("picture plan stages", (double)plan->Order().size(), "stages");
      Note("picture plan merges", (double)plan->Merges().size(), "merges");
    }
  }

  {
    PlanSpec spec;
    spec.Outputs = {Resource::FrameTex};
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = [&] { auto made = RenderPlan::Compile(spec); if (made) { plan = *std::move(made); return true; } why = std::move(made).error(); return false; }();
    CHECK(!compiled, "a picture with no content stage at all is refused rather than rendered black");
    CHECK(why.find("sceneHdr") != std::string::npos && why.find("render.content.") != std::string::npos,
          "the refusal names the attachment nothing draws into and a stage that would supply it");
    CHECK(!plan, "the refused plan left the handle untouched");
    Note(("zero-contributor refusal: " + why).c_str());
  }

  {
    PlanSpec spec;
    spec.Outputs = {Resource::SceneDepth};
    spec.Content = {Stage::Subjects, Stage::AmbientOcclusion};
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = [&] { auto made = RenderPlan::Compile(spec); if (made) { plan = *std::move(made); return true; } why = std::move(made).error(); return false; }();
    CHECK(!compiled, "a declared stage nothing reads is refused rather than encoded for nobody");

    CHECK(why.find(std::string("render.content.") + Row(Stage::AmbientOcclusion).Name) !=
              std::string::npos,
          "the refusal names the declaration path of the stage it refused");
    Note(("unread-content refusal: " + why).c_str());
  }

  {
    PlanSpec spec;
    spec.Outputs = {Resource::SceneDepth};
    spec.Content = {Stage::Subjects};
    spec.Display = outshine::Render::Declared<Transfer>(Transfer::Linear);
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = [&] { auto made = RenderPlan::Compile(spec); if (made) { plan = *std::move(made); return true; } why = std::move(made).error(); return false; }();
    CHECK(!compiled, "an option nothing reads is refused");
    CHECK(why.find("render.display") != std::string::npos,
          "the option refusal names the option's own declaration path");
  }

  {
    std::shared_ptr<const RenderPlan> narrow, wide;
    std::string why;
    PlanSpec spec = CoverageSpec();
    const bool half = [&] { auto made = RenderPlan::Compile(spec); if (made) { narrow = *std::move(made); return true; } why = std::move(made).error(); return false; }();
    spec.Precision = outshine::Render::Declared<outshine::Render::ScenePrecision>(
        outshine::Render::ScenePrecision::Float);
    const bool full = [&] { auto made = RenderPlan::Compile(spec); if (made) { wide = *std::move(made); return true; } why = std::move(made).error(); return false; }();
    CHECK(half && full, "the same declaration compiles at both declared precisions");
    if (half && full) {
      CHECK(narrow->Format(Resource::SceneHdr) == outshine::Render::TexelFormat::Rgba16Float &&
                narrow->Format(Resource::SceneLinear) == outshine::Render::TexelFormat::Rgba16Float,
            "a plan that declares no precision stores scene radiance in the catalogue's half");
      CHECK(wide->Format(Resource::SceneHdr) == outshine::Render::TexelFormat::Rgba32Float &&
                wide->Format(Resource::SceneLinear) == outshine::Render::TexelFormat::Rgba32Float,
            "a plan that declares Float widens the scene target and the resolve it aliases to, "
            "together -- a half target feeding a float readback would report the storage as the "
            "arithmetic");
      CHECK(wide->Format(Resource::SceneVelocity) ==
                    narrow->Format(Resource::SceneVelocity) &&
                wide->Format(Resource::FrameTex) == narrow->Format(Resource::FrameTex),
            "the declared precision reaches the scene-referred rows and no other: a velocity and a "
            "display-encoded frame are their formats");
      CHECK(wide->Digest() != narrow->Digest(),
            "the storage is part of the plan's identity, so the two precisions are two plans");
      Note(("half plan digest: " + narrow->Digest()).c_str());
      Note(("float plan digest: " + wide->Digest()).c_str());
    }
  }

  {
    PlanSpec spec;
    spec.Outputs = {Resource::ShadowAtlas};
    spec.Content = {Stage::LightVisibility};
    spec.Precision = outshine::Render::Declared<outshine::Render::ScenePrecision>(
        outshine::Render::ScenePrecision::Float);
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = [&] { auto made = RenderPlan::Compile(spec); if (made) { plan = *std::move(made); return true; } why = std::move(made).error(); return false; }();
    CHECK(!compiled, "a declared precision over a plan holding no scene-referred target is refused");
    CHECK(why.find("render.precision") != std::string::npos,
          "the precision refusal names its own declaration path");
    Note(("unread-precision refusal: " + why).c_str());
  }

  {
    PlanSpec spec;
    spec.Outputs = {Resource::FrameTex};
    spec.Content = {Stage::Terrain};
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = [&] { auto made = RenderPlan::Compile(spec); if (made) { plan = *std::move(made); return true; } why = std::move(made).error(); return false; }();
    if (!compiled) { Note(("terrain-without-shadows refusal: " + why).c_str()); }
    CHECK(compiled,
          "**A LIT SURFACE WITHOUT A SHADOW CASTER IS A PICTURE, NOT A REFUSAL.** This claim "
          "once demanded the refusal, and it was wrong as specified: LightVisibility CONTRIBUTES "
          "the atlas, and CLAUDE.md's own row reads 'a missing contributor is a picture choice'. "
          "The atlas falls back to NEUTRAL -- nothing is shadowed -- which is exactly what a "
          "consumer who declared no caster asked to see");
    bool published = false;
    if (plan) {
      for (const std::string &alias : plan->Aliases()) {
        if (alias.find("shadowAtlas") != std::string::npos) { published = true; }
      }
    }
    CHECK(published,
          "and the neutral stand-in is PUBLISHED with the compiled plan, so a reader can tell an "
          "unshadowed picture from a shadowed one without rendering either");
  }

  {
    CHECK(RenderPlan::StageByName("terrain") == Stage::Terrain,
          "a declared stage name resolves to its stage");
    CHECK(!RenderPlan::StageByName("terrian").has_value(),
          "an unknown stage name is refused rather than dropped");
  }

  Covers("I.81 the declared render plan: machinery is pulled backwards from the requested outputs, "
         "content is declared, the pass count is an output of the compiler, and the four refusals a "
         "consumer can cause each name their declaration path");
  return Report();
}
