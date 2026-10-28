#include "Check.h"
#include "MaterialRegistry.h"
#include "GeometryRenderer.h"
#include "RenderScene.h"
#include "Gpu.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

struct ImageFaults final : NativeDeviceProxy {
  using NativeDeviceProxy::NativeDeviceProxy;
  size_t Creates = 0;
  size_t RejectAt = 0;

  Result<std::unique_ptr<Texture>> CreateTexture(TextureDescription description,
                                                 std::string_view label = {}) override {
    if (++Creates == RejectAt) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Busy, .Message = "injected material image failure"});
    }
    return Backing().CreateTexture(description, label);
  }
};

Geometry Source() {
  Geometry source;
  const std::vector<uint8_t> pixels(size_t{256} * 256 * 4, 128);
  const auto image = source.addImage(256, 256, pixels);
  CHECK(image, "the material input owns its image");
  if (!image) { return source; }
  Material material;
  material.Unlit = true;
  material.BaseColourMap.Image = *image;
  CHECK(source.addSurface("mapped", material), "the material input is valid");
  return source;
}
}

int main() {
  auto native = Require(CreateDevice({.Validation = true}));
  ImageFaults opaqueDevice(*native);
  ImageFaults glassDevice(*native);
  RenderScene opaqueScene;
  RenderScene glassScene;
  Require(opaqueScene.Meshes().StandsOn(&opaqueDevice));
  Require(glassScene.Meshes().StandsOn(&glassDevice));
  GeometryRenderer opaque(opaqueScene);
  GeometryRenderer glass(glassScene, GeometryPassKind::Transmission);
  Render::Gpu gpu;
  gpu.Device = &opaqueDevice;
  gpu.HdrFormat = Format::Rgba16Float;
  gpu.SurfaceFormat = Format::Rgba8Unorm;
  gpu.Width = gpu.Height = 2;
  CHECK(gpu.SceneColours.Add(Resource::SceneHdr), "the material target has a real colour output");
  Require(opaque.Configure(gpu));
  gpu.Device = &glassDevice;
  Require(glass.Configure(gpu));
  MaterialRegistry registry;
  std::string error;
  for (size_t at = 0; at < 16; ++at) {
    glassDevice.RejectAt = glassDevice.Creates + 1;
    CHECK(!registry.Register(opaque, &glass, Source()),
          "the paired registration refuses an injected native image failure");
    CHECK(opaque.ColourImages() == 0 && glass.ColourImages() == 0 &&
              opaqueScene.Meshes().Allocations().TransferBytes < 256 &&
              glassScene.Meshes().Allocations().TransferBytes < 256,
          "failed pair writes retain only shared tiny defaults, no source images or large staging");
  }
  glassDevice.RejectAt = 0;
  auto first = registry.Register(opaque, &glass, Source());
  auto second = registry.Register(opaque, &glass, Source());
  CHECK(first && second && first->First == 0 && second->First == 1,
        "rejected writes return their rows instead of exhausting logical indices");
  if (!first) { std::println(stderr, "first registration: {}", first.error()); }
  if (!second) { std::println(stderr, "second registration: {}", second.error()); }
  if (!first || !second) { return Report(); }
  CHECK(opaque.SetMaterials({}, error) && glass.SetMaterials({}, error),
        "a changed native material base invalidates the previous GPU bindings");
  glassDevice.RejectAt = glassDevice.Creates + 2;
  CHECK(!registry.Restore(opaque, &glass, error),
        "restoration can refuse after an earlier source was already installed");
  CHECK(opaque.ColourImages() == 0 && glass.ColourImages() == 0 &&
            opaqueScene.Meshes().Allocations().TransferBytes < 256 &&
            glassScene.Meshes().Allocations().TransferBytes < 256,
        "restore rollback cancels all partially restored source images and transfers");
  glassDevice.RejectAt = 0;
  CHECK(registry.Prepare(opaque, &glass, error) && registry.Contains(*first) &&
            registry.Contains(*second),
        "retry retains both original CPU sources and their stable logical references");
  CHECK(opaque.ColourImages() == 2 && glass.ColourImages() == 2,
        "both native paths recover the exact two real image bindings");
  first->Source.reset();
  second->Source.reset();
  CHECK(registry.Prepare(opaque, &glass, error) && opaque.ColourImages() == 0 &&
            glass.ColourImages() == 0 && opaqueScene.Meshes().Allocations().TransferBytes < 256 &&
            glassScene.Meshes().Allocations().TransferBytes < 256,
        "last-consumer retirement cancels remaining unsubmitted source uploads in both paths");
  return Report();
}
