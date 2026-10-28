#include "ImpostorBaker.h"
#include "Check.h"
#include "GpuSubmission.h"
#include "Heap.h"
#include "test/experiments/render/NativeMaterialMemory.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

Render::ImpostorCapture Source(size_t materials, bool green = false) {
  Geometry mesh;
  std::vector<uint8_t> pixels(size_t{64} * 64 * 4, 255);
  for (size_t at = 0; at < pixels.size(); at += 4) { pixels[at] = 128; }
  const auto image = mesh.addImage(64, 64, pixels);
  CHECK(image, "each capture owns a newly uploaded source image");
  const std::array<Vec4f, 3> colours{green ? Vec4f{{.1f, .8f, .2f, 1}} : Vec4f{{.8f, .2f, .1f, 1}},
                                     Vec4f{{.1f, .3f, .9f, 1}},
                                     Vec4f{{.7f, .6f, .2f, 1}}};
  for (size_t at = 0; at < materials; ++at) {
    Material material;
    material.BaseColour = colours.at(at);
    material.Roughness = .25f + static_cast<float>(at) * .2f;
    if (image) { material.BaseColourMap.Image = *image; }
    CHECK(mesh.addSurface("material", material), "capture material is valid");
  }
  for (size_t at = 0; at < 2; ++at) {
    const auto part = mesh.addPart("plane", MaterialInstance(static_cast<int>(at % materials)));
    CHECK(part, "capture part is allocated");
    if (!part) { continue; }
    const float left = at == 0 ? -.9f : 0;
    const float right = at == 0 ? 0 : .9f;
    const float depth = green ? .05f : 0;
    CHECK(mesh.setPositions(
              *part,
              std::array<float, 12>{
                  left, -.8f, depth, right, -.8f, depth, right, .8f, depth, left, .8f, depth}) &&
              mesh.setNormals(*part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}) &&
              mesh.setTexture(*part, std::array<float, 8>{0, 1, 1, 1, 1, 0, 0, 0}) &&
              mesh.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
          "capture source owns complete native planes");
  }
  Render::ImpostorCapture capture;
  capture.LeastM = {{-1, -1, -.1}};
  capture.MostM = {{1, 1, .1}};
  capture.ViewDirections = {{{0, 0, 1}}};
  capture.Sources.push_back({.Mesh = std::move(mesh), .Instances = {Mat4{}}});
  return capture;
}

std::optional<std::vector<uint8_t>> Encode(const std::optional<ImpostorAtlas> &atlas) {
  if (!atlas) { return std::nullopt; }
  std::string error;
  return atlas->Encode("fixed-source-replay", error);
}

void CheckSession() {
  size_t devices = 0;
  const Render::GpuSubmission submission{
      .Context = &devices,
      .Create = [](void *context, Render::Native::DeviceOptions options)
          -> Render::Native::Result<std::unique_ptr<Render::Native::Device>> {
        ++*static_cast<size_t *>(context);
        return Render::Native::CreateDevice(options);
      }};
  Render::ImpostorBaker baker(submission);
  std::string error;
  CHECK(!baker.Capture({}, {.Pixels = 2, .Views = 1}, error) && devices == 0,
        "invalid work never creates a native device");
  std::array<std::optional<std::vector<uint8_t>>, 3> references;
  for (size_t at = 0; at < references.size(); ++at) {
    references[at] =
        Encode(Render::ImpostorBaker::Bake(Source(at + 1), {.Pixels = 16, .Views = 1}, error));
    CHECK(references[at], "fresh-renderer source reference captures");
  }
  for (const size_t materials : {3U, 1U, 2U, 3U, 2U, 1U, 3U, 2U}) {
    const auto atlas = baker.Capture(Source(materials), {.Pixels = 16, .Views = 1}, error);
    CHECK(atlas, error.c_str());
    CHECK(Encode(atlas) == references[materials - 1],
          "recycled native material rows map to the same local atlas identities and values");
    CHECK(devices == 1, "consecutive source products reuse the same actual native device");
  }
  auto failed = Source(2, true);
  failed.Sources.push_back({.Mesh = Geometry{}, .Instances = {Mat4{}}});
  CHECK(!baker.Capture(std::move(failed), {.Pixels = 16, .Views = 1}, error) && !error.empty(),
        "a later invalid source fails after installing an actual foreground source");
  CHECK(Encode(baker.Capture(Source(2), {.Pixels = 16, .Views = 1}, error)) == references[1],
        "a failed capture leaves no stale foreground geometry or material behind");
  const auto resized = baker.Capture(Source(2), {.Pixels = 31, .Views = 1}, error);
  CHECK(resized && resized->Pixels() == 31 && devices == 1,
        "attachment resize preserves the existing device");
  CHECK(Encode(baker.Capture(Source(2), {.Pixels = 16, .Views = 1}, error)) == references[1] &&
            devices == 1,
        "returning to the original raster preserves material output and device ownership");
  const auto before = Heap::LiveBytes();
  const auto gpuBefore = MaterialDeviceAllocatedBytes();
  CHECK(Heap::ProcessInstrumentationEnabled() && before > 0,
        "heap diagnostics use enabled nonempty process instrumentation");
  for (size_t at = 0; at < 16; ++at) {
    const auto atlas = baker.Capture(Source(2), {.Pixels = 16, .Views = 1}, error);
    CHECK(Encode(atlas) == references[1], "steady captures retire previous source products");
  }
  std::cout << "capture_session devices=" << devices << " heap_before=" << before
            << " heap_after=" << Heap::LiveBytes() << '\n';
  const auto gpuAfter = MaterialDeviceAllocatedBytes();
  if (gpuBefore && gpuAfter) {
    std::cout << "capture_session gpu_before=" << *gpuBefore << " gpu_after=" << *gpuAfter << '\n';
    CHECK(*gpuAfter <= *gpuBefore,
          "steady source captures do not retain additional actual Metal device allocations");
  }
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  CheckSession();
  SDL_Quit();
  return Report();
}
