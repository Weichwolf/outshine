#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Check.h"

#include "RenderPlan.h"
#include "Renderer.h"

using outshine::Render::PlanSpec;
using outshine::Render::RenderPlan;
using outshine::Render::Renderer;
using outshine::Render::Resource;
using outshine::Render::Row;
using outshine::Render::Stage;
using outshine::Render::Transfer;

namespace {

PlanSpec Declaring(Stage content) {
  PlanSpec spec;
  spec.Outputs = {Resource::FrameTex};
  spec.Content = {content};
  spec.Display = outshine::Render::Declared<Transfer>(Transfer::Linear);
  spec.Exposure = outshine::Render::Declared<float>(1.0f);
  return spec;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  {
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    CHECK([&] { auto made = RenderPlan::Compile(Declaring(Stage::Sun)); if (made) { plan = *std::move(made); return true; } why = std::move(made).error(); return false; }(),
          "the catalogue offers the sun, so the declaration compiles -- the refusal belongs "
          "to the device layer, by name, not to a catalogue hole");
    Renderer refused;
    refused.Init(64, 64, plan);
    std::printf("NOTE the device says: %s\n", refused.WhyNot().c_str());
    CHECK(!refused.WhyNot().empty() &&
              refused.WhyNot().find("does not execute the stage") != std::string::npos,
          "**A DECLARED STAGE THE DEVICE CANNOT RUN REFUSES AT PLAN TIME, BY NAME** -- before "
          "any frame is drawn, never as a silently unchanged picture (the 49.03 % that filed "
          "board:1549)");
  }

  {
    std::shared_ptr<const RenderPlan> plan;
    std::string why;
    CHECK([&] { auto made = RenderPlan::Compile(Declaring(Stage::Sky)); if (made) { plan = *std::move(made); return true; } why = std::move(made).error(); return false; }(),
          "and the sky declaration compiles");
    Renderer standing;
    standing.Init(64, 64, plan);
    CHECK(standing.WhyNot().empty(),
          "**AND THE CATALOGUE AND THE DEVICE AGREE ON WHAT RUNS**: the sky chain the device "
          "grew since the item filed -- transmittance, multi-scatter, radiance, sky -- stands "
          "up without a refusal, so a refusal is always a stage genuinely not yet built");
  }

  Covers("IV.9 declaring a stage the device cannot execute refuses at plan time naming the "
         "stage; a stage the device grew executes; the not-yet-built stay catalogued because "
         "the TARGET builds them and the loud refusal keeps the path honest (board:1549)");
  return Report();
}
