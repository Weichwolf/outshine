/* THE COMPILER OF THE DECLARED PLAN, judged with no device in the process at all. This test's layer
 * include set is `-Isrc/core -Isrc/render/plan` and nothing else, so "a plan is checkable before a
 * bind group can exist" is proved by the build rather than claimed in a comment: if anything in the
 * plan reached for the renderer or for a `wgpu::` type, this test would not compile. */
#include <memory>
#include <string>

#include "Check.h"

#include "RenderPlan.h"

using outshine::Render::PlanSpec;
using outshine::Render::RenderPlan;
using outshine::Render::Resource;
using outshine::Render::Stage;
using outshine::Render::Transfer;

namespace {

/* The declaration a coverage rung makes: one content stage, two requested outputs, a declared
 * exposure and no display curve. Written here rather than in a helper the runner shares, because a
 * test that used the runner's own declaration would agree with it by construction. */
PlanSpec CoverageSpec() {
  PlanSpec spec;
  spec.Outputs = {Resource::SceneDepth, Resource::FrameTex};
  spec.Content = {Stage::Subjects};
  spec.Display = outshine::Render::Declared<Transfer>(Transfer::Linear);
  spec.Exposure = outshine::Render::Declared<float>(1.0f);
  return spec;
}

int PassesOf(const RenderPlan &plan) { return plan.PassCount(); }

} // namespace

int main() {
  using namespace outshine::Test;

  {
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = RenderPlan::Compile(CoverageSpec(), &plan, why);
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
      CHECK(plan->Aliases().size() == 1u, "the plan publishes the one alias it applied");
      CHECK(plan->SettleFrames() == 1,
            "a plan with no temporal history needs no settle frames beyond the one the device needs "
            "to have submitted something to copy");
      CHECK(!plan->Digest().empty(), "the plan carries its own digest");
      Note(("coverage plan digest " + plan->Digest()).c_str());
    }
  }

  /* THE SAME DECLARATION TWICE IS THE SAME PLAN. A digest that moved between two compilations of one
   * declaration would key a baseline to the weather. */
  {
    std::shared_ptr<const RenderPlan> first, second;
    std::string why;
    const bool both = RenderPlan::Compile(CoverageSpec(), &first, why) &&
                      RenderPlan::Compile(CoverageSpec(), &second, why);
    CHECK(both, "one declaration compiles twice");
    if (both) {
      CHECK(first->Digest() == second->Digest(), "two compilations of one declaration agree");
    }
  }

  /* THE PICTURE PLAN: content that declares the resolve gets it and its history, and the compiler
   * re-fuses the resolve with the tonemap rather than the shader hiding the merge. */
  {
    PlanSpec spec;
    spec.Outputs = {Resource::Surface};
    spec.Content = {Stage::Sky, Stage::Sun, Stage::LightVisibility, Stage::Terrain, Stage::AmbientOcclusion,
                    Stage::TemporalResolve, Stage::AutoExposure};
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = RenderPlan::Compile(spec, &plan, why);
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

  /* THE ANTI-VACUOUS RULE. A plan that compiles, runs and renders black is the vacuous gate wearing
   * a pipeline, and it is refused where the declaration is written. */
  {
    PlanSpec spec;
    spec.Outputs = {Resource::FrameTex};
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = RenderPlan::Compile(spec, &plan, why);
    CHECK(!compiled, "a picture with no content stage at all is refused rather than rendered black");
    CHECK(why.find("sceneHdr") != std::string::npos && why.find("render.content.") != std::string::npos,
          "the refusal names the attachment nothing draws into and a stage that would supply it");
    CHECK(!plan, "the refused plan left the handle untouched");
    Note(("zero-contributor refusal: " + why).c_str());
  }

  /* A CONTENT STAGE THAT CONTRIBUTES TO NOTHING THE PLAN REQUESTS. */
  {
    PlanSpec spec;
    spec.Outputs = {Resource::SceneDepth};
    spec.Content = {Stage::Subjects, Stage::AmbientOcclusion};
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = RenderPlan::Compile(spec, &plan, why);
    CHECK(!compiled, "a declared stage nothing reads is refused rather than encoded for nobody");
    /* THE PATH IS DERIVED FROM THE CATALOGUE AND NOT SPELLED HERE. The refusal builds it from
     * `Row(id).Name`, so a test that spelled the name would go stale on a rename without failing to
     * compile -- which it did twice, and cost two gate runs, when five stages became mechanisms. */
    CHECK(why.find(std::string("render.content.") + Row(Stage::AmbientOcclusion).Name) !=
              std::string::npos,
          "the refusal names the declaration path of the stage it refused");
    Note(("unread-content refusal: " + why).c_str());
  }

  /* AN OPTION NO STAGE OF THE COMPILED PLAN READS. */
  {
    PlanSpec spec;
    spec.Outputs = {Resource::SceneDepth};
    spec.Content = {Stage::Subjects};
    spec.Display = outshine::Render::Declared<Transfer>(Transfer::Linear);
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = RenderPlan::Compile(spec, &plan, why);
    CHECK(!compiled, "an option nothing reads is refused");
    CHECK(why.find("render.display") != std::string::npos,
          "the option refusal names the option's own declaration path");
  }

  /* THE DECLARED STORAGE OF THE SCENE-REFERRED CHAIN, which is what a case whose verdict is the
   * VALUE declares (board:0087). It moves the plan's identity, so a baseline taken
   * at one precision cannot be read as the other's. */
  {
    std::shared_ptr<const RenderPlan> narrow, wide;
    std::string why;
    PlanSpec spec = CoverageSpec();
    const bool half = RenderPlan::Compile(spec, &narrow, why);
    spec.Precision = outshine::Render::Declared<outshine::Render::ScenePrecision>(
        outshine::Render::ScenePrecision::Float);
    const bool full = RenderPlan::Compile(spec, &wide, why);
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

  /* A PRECISION DECLARED OVER A PLAN THAT CARRIES NO RADIANCE AT ALL -- here a shadow atlas and the
   * one stage that draws into it, which is a whole plan whose every target is depth. */
  {
    PlanSpec spec;
    spec.Outputs = {Resource::ShadowAtlas};
    spec.Content = {Stage::LightVisibility};
    spec.Precision = outshine::Render::Declared<outshine::Render::ScenePrecision>(
        outshine::Render::ScenePrecision::Float);
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = RenderPlan::Compile(spec, &plan, why);
    CHECK(!compiled, "a declared precision over a plan holding no scene-referred target is refused");
    CHECK(why.find("render.precision") != std::string::npos,
          "the precision refusal names its own declaration path");
    Note(("unread-precision refusal: " + why).c_str());
  }

  /* A LIT SURFACE WITH NO SHADOW MAP is refused and told which stage supplies the atlas, rather than
   * given a cleared texture that would be a second lighting path. */
  {
    PlanSpec spec;
    spec.Outputs = {Resource::FrameTex};
    spec.Content = {Stage::Terrain};
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    const bool compiled = RenderPlan::Compile(spec, &plan, why);
    CHECK(!compiled, "a lit surface with no shadow map is refused");
    CHECK(why.find("shadowAtlas") != std::string::npos &&
              why.find(std::string("render.content.") + Row(Stage::LightVisibility).Name) !=
                  std::string::npos,
          "the refusal names the missing resource and the stage that would have supplied it");
    Note(("missing-producer refusal: " + why).c_str());
  }

  /* THE ONE PLACE A STRING BECOMES A STAGE, so a declaration file's typo is a refusal and not a
   * silently absent stage. */
  {
    Stage found = Stage::kCount;
    CHECK(RenderPlan::StageByName("terrain", &found) && found == Stage::Terrain,
          "a declared stage name resolves to its stage");
    CHECK(!RenderPlan::StageByName("terrian", &found),
          "an unknown stage name is refused rather than dropped");
  }

  Covers("I.27 the declared render plan: machinery is pulled backwards from the requested outputs, "
         "content is declared, the pass count is an output of the compiler, and the four refusals a "
         "consumer can cause each name their declaration path");
  return Report();
}
