#include "Check.h"
#include "SceneRenderer.h"

#include <SDL3/SDL.h>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  if (SDL_WasInit(SDL_INIT_VIDEO) == 0) { return Report(); }
  {
    PlanSpec spec;
    spec.Outputs = {Resource::SkyViewLut, Resource::VegetationTable, Resource::CascadeUniform};
    const auto plan = Compiled::Compile(spec);
    CHECK(plan.has_value(), "native sky and logical input plan compiles");
    if (plan) {
      CHECK(!(*plan)->Holds(Resource::ShadowAtlas) && !(*plan)->Holds(Resource::IrradianceBuffer),
            "neither shadows nor irradiance are required by this plan");
      SceneRenderer renderer;
      const auto opened = renderer.Init({.WidthPx = 32, .HeightPx = 32}, *plan);
      CHECK(opened.has_value(), opened ? "sky resource plan opens" : opened.error().c_str());
      if (opened) {
        const auto held = renderer.FrameGraphAllocationCounts();
        CHECK(held.Textures == 3, "only the three atmosphere lookup textures are held");
        CHECK(held.Buffers == 0, "pushed uniforms do not allocate storage buffers");
        CHECK(held.Samplers == 1, "atmosphere lookups share one sampler");
        spec.Outputs.push_back(Resource::IrradianceBuffer);
        const auto litPlan = Compiled::Compile(spec);
        CHECK(litPlan.has_value(), "explicit irradiance demand compiles");
        if (litPlan) {
          const auto lit = renderer.Init({.WidthPx = 32, .HeightPx = 32}, *litPlan);
          CHECK(lit.has_value(), lit ? "irradiance plan opens" : lit.error().c_str());
          if (lit) {
            const auto required = renderer.FrameGraphAllocationCounts();
            CHECK(required.Buffers == 1 && required.Textures == 3 && required.Samplers == 1,
                  "real irradiance storage is allocated without unrelated targets");
          }
        }
      }
    }
  }
  SDL_Quit();
  return Report();
}
