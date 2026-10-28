#include "MaterialRegistry.h"
#include "MaterialSource.h"
#include "GeometryRenderer.h"
#include "RenderScene.h"
#include "RadianceFilter.h"
#include "Gpu.h"
#include "Check.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeRadiance.h"
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

Geometry Support(Material material) {
  Geometry result;
  CHECK(result.addSurface("support", material), "support material is a valid native CPU value");
  return result;
}

void Configure(GeometryRenderer &renderer, RenderScene &scene, Native::Device &device) {
  Require(scene.Meshes().StandsOn(&device));
  Render::Gpu gpu;
  gpu.Device = &device;
  gpu.HdrFormat = Native::Format::Rgba16Float;
  gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
  gpu.Width = gpu.Height = 2;
  CHECK(gpu.SceneColours.Add(Resource::SceneHdr), "the material renderer has a colour target");
  Require(renderer.Configure(gpu));
}
}

int main() {
  const auto device = Require(Native::CreateDevice({.Validation = true}));
  const auto source = RadianceSource(*device);
  RadianceFilter filter;
  auto image = Require(filter.Apply(*device, *source, {.WidthPx = 1, .HeightPx = 1}));
  RenderScene scene;
  GeometryRenderer renderer(scene);
  Configure(renderer, scene, *device);
  MaterialRegistry registry;
  Material material;
  material.Unlit = true;
  material.Alpha = AlphaMode::Blended;
  material.BaseColour = {{1, 1, 1, 1}};
  const auto support = Support(material);
  std::array<MaterialRadiance, 1> input{{{.Material = 0, .Image = image}}};
  input.front().Material = 1;
  CHECK(!registry.Register(renderer, nullptr, support.clone(), input),
        "an absent material cannot receive a resident field");
  input.front().Material = 0;
  {
    const std::array duplicate{input.front(), input.front()};
    CHECK(!registry.Register(renderer, nullptr, support.clone(), duplicate),
          "two overrides cannot silently replace one another");
  }
  auto lit = material;
  lit.Unlit = false;
  CHECK(!registry.Register(renderer, nullptr, Support(lit), input),
        "a prelit field cannot silently enter a lit PBR material");
  auto opaque = material;
  opaque.Alpha = AlphaMode::Opaque;
  CHECK(!registry.Register(renderer, nullptr, Support(opaque), input),
        "fractional radiance cannot become opaque coverage authority");
  auto registered = registry.Register(renderer, nullptr, support.clone(), input);
  CHECK(registered, "valid prelit radiance registers without CPU image placeholders");
  if (!registered) { return Report(); }
  CHECK(registered->Source->Source.images() == 0 &&
            registered->Source->Source.surfaceAt(MaterialInstance(0)).Scattering ==
                ScatteringModel::MetallicRoughness,
        "render representation does not rewrite or populate native source material data");
  const std::weak_ptr<const RadianceImage> lifetime = image;
  input.front().Image.reset();
  image.reset();
  CHECK(!lifetime.expired(), "the material consumer owns the image after producer release");
  RenderScene restoredScene;
  GeometryRenderer restored(restoredScene);
  Configure(restored, restoredScene, *device);
  MaterialRegistry copied;
  copied.CopySourcesFrom(registry);
  std::string error;
  CHECK(copied.Restore(restored, nullptr, error) && copied.Contains(*registered),
        "a new render state restores the same image on the same producing device");
  CHECK(renderer.ColourImages() == 1 && restored.ColourImages() == 1,
        "both render states refer to their one immutable radiance field");
  registered->Source.reset();
  CHECK(registry.Prepare(renderer, nullptr, error) && copied.Prepare(restored, nullptr, error) &&
            renderer.ColourImages() == 0 && restored.ColourImages() == 0,
        "last-consumer retirement clears the field bindings in both states");
  CHECK(lifetime.expired(), "no abandoned radiance wrapper survives material retirement");
  return Report();
}
