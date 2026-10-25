#include "FrameTargets.h"
#include "Check.h"
#include <cstdint>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  const auto device = Native::CreateDevice();
  CHECK(device, "native attachment tests need no renderer or world");
  if (!device) { return Report(); }
  PlanSpec declaration;
  declaration.Content = {Stage::Geometry};
  declaration.Outputs = {Resource::SceneHdr, Resource::SceneDepth};
  FrameTargets targets;
  constexpr Extent kExtent{.WidthPx = 16, .HeightPx = 8};
  for (const auto precision : {ScenePrecision::Half, ScenePrecision::Float}) {
    declaration.Precision = Declared(precision);
    const auto plan = ExecutionPlan::Compile(declaration);
    CHECK(plan, "explicit color and depth demand compiles independently");
    if (!plan) { return Report(); }
    CHECK(targets.Replace(**device, kExtent, **plan), "the requested attachment set allocates");
    const auto *hdr = targets.Get(Resource::SceneHdr);
    const auto *depth = targets.Get(Resource::SceneDepth);
    CHECK(hdr && depth && targets.Count() == 2,
          "only the two requested geometry attachments exist");
    if (hdr == nullptr || depth == nullptr) { return Report(); }
    CHECK(hdr->Description().Width == kExtent.WidthPx &&
              hdr->Description().Height == kExtent.HeightPx &&
              hdr->Description().Texels == (precision == ScenePrecision::Half
                                                ? Native::Format::Rgba16Float
                                                : Native::Format::Rgba32Float),
          "color dimensions and precision retain the declared profile");
    const auto colorUse = Native::TextureUse::ColourAttachment | Native::TextureUse::Sampled |
                          Native::TextureUse::CopySource;
    const auto depthUse = Native::TextureUse::DepthAttachment | Native::TextureUse::Sampled |
                          Native::TextureUse::CopySource;
    CHECK(depth->Description().Texels == Native::Format::Depth32Float &&
              static_cast<uint32_t>(hdr->Description().Use) == static_cast<uint32_t>(colorUse) &&
              static_cast<uint32_t>(depth->Description().Use) == static_cast<uint32_t>(depthUse),
          "render, sampling and readback usages remain distinct for color and depth");
    CHECK(!targets.Get(Resource::SceneVelocity) && !targets.Get(Resource::SceneLinear) &&
              !targets.Get(Resource::SceneSurfaceBase),
          "unrequested velocity, history and material attachments allocate nothing");
    CHECK(!FrameTargets::Owns(Resource::ShadowAtlas) && !FrameTargets::Owns(Resource::SkyViewLut) &&
              !targets.Get(Resource::Surface),
          "pass-specific fixed resources and presentation have their own owners");
  }
  return Report();
}
