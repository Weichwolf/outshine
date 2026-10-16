#include "CloudViewProbe.h"
#include "CloudLayer.h"
#include "MediumViewProbe.h"
#include "NativeGpuCheck.h"
#include "Check.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using namespace outshine::Test::CloudViewProbe;

std::unique_ptr<GraphicsPipeline>
Pipeline(Device &device, const char *vertex, const char *fragment, bool scene) {
  const auto vertexCode = Spirv(vertex);
  const auto fragmentCode = Spirv(fragment);
  std::array<ColourTarget, 4> targets;
  for (auto &target : targets) { target.Texels = Format::Rgba32Float; }
  auto pipeline = Require(device.CreateGraphicsPipeline(
      {.Vertex = {.Spirv = vertexCode},
       .Fragment = ShaderCode{.Spirv = fragmentCode},
       .Streams = {},
       .Attributes = {},
       .Colours = std::span(targets).first(scene ? 1 : targets.size()),
       .Depth = scene ? std::optional{DepthTarget{}} : std::nullopt,
       .Culling = Cull::None,
       .Label = "CloudFrontRasterControl"}));
  if (!scene) {
    size_t medium = 0;
    size_t weather = 0;
    for (const auto &binding : pipeline->Layout().Bindings) {
      const auto &resource = binding.Resource;
      if (resource.Set == 3 && resource.Slot == 2 && resource.Kind == BindingKind::Uniform) {
        CHECK(resource.MinimumBytes == sizeof(outshine::Render::MediumViewParameters),
              "the actual fragment3/2 frame stays224 bytes");
        ++medium;
      }
      if (resource.Set == 3 && resource.Slot == 3 && resource.Kind == BindingKind::Uniform) {
        CHECK(resource.MinimumBytes == sizeof(WeatherFrame),
              "the private fragment3/3 weather payload is96 bytes");
        ++weather;
      }
    }
    CHECK(medium == 1 && weather == 1, "both independently owned snapshots are reflected once");
  }
  return pipeline;
}

uint32_t Extent(const char *text) {
  uint32_t value = 0;
  const std::string_view input(text);
  const auto parsed = std::from_chars(input.data(), input.data() + input.size(), value);
  constexpr uint32_t maximum = 8192;
  return parsed.ec == std::errc{} && parsed.ptr == input.data() + input.size() && value > 0 &&
                 value <= maximum
             ? value
             : 0;
}
}

int main(int argc, char **argv) {
  if (argc != 6 && argc != 8 && argc != 9 && argc != 10) {
    std::println(
        stderr,
        "usage: cloud-view-native scene.vert scene.frag fullscreen.vert front.frag output-prefix "
        "[width height [scenario [boundary-sky|cumulus-sky|stratus-sky]]]");
    return 2;
  }
  const uint32_t width = argc >= 8 ? Extent(argv[6]) : MediumViewProbe::kWidth;
  const uint32_t height = argc >= 8 ? Extent(argv[7]) : MediumViewProbe::kHeight;
  if (width == 0 || height == 0) { return 2; }
  constexpr uint32_t rowAlignment = 256;
  const MediumViewProbe::ImageShape shape{
      .Width = width,
      .Height = height,
      .RowBytes = (width * static_cast<uint32_t>(sizeof(Pixel)) + rowAlignment - 1) / rowAlignment *
                  rowAlignment};
  const bool cumulus = argc == 10 && std::string_view(argv[9]) == "cumulus-sky";
  const bool stratus = argc == 10 && std::string_view(argv[9]) == "stratus-sky";
  const bool boundarySky =
      argc == 10 && (std::string_view(argv[9]) == "boundary-sky" || cumulus || stratus);
  if (argc == 10 && !boundarySky) { return 2; }
  std::vector<Case> cases;
  if (cumulus || stratus) {
    cases = ProfileCases(
        shape, cumulus ? outshine::CloudProfile::CumulusV1 : outshine::CloudProfile::Stratus);
  } else if (boundarySky) {
    cases = BoundaryCases(shape);
  } else {
    cases = Cases(shape);
  }
  auto device = Require(CreateDevice({.Validation = true}));
  CHECK(device->Limits().MaxColourAttachments >= 4, "four actual cloud-front MRTs are supported");
  const auto scene = Pipeline(*device, argv[1], argv[2], true);
  const auto front = Pipeline(*device, argv[3], argv[4], false);
  const Targets targets(*device, shape);
  size_t frames = 0;
  for (const auto &scenario : cases) {
    if (argc >= 9 && std::string_view(argv[8]) != "all" && scenario.Name != argv[8]) { continue; }
    const auto capture = targets.Draw(*device, {.Scene = *scene, .Front = *front}, scenario);
    const auto repeat = targets.Draw(*device, {.Scene = *scene, .Front = *front}, scenario);
    CHECK(capture.Images == repeat.Images, "repeated actual raster output is bitwise identical");
    WriteCapture(scenario, capture, shape, std::string(argv[5]) + "." + scenario.Name);
    frames += 2;
  }
  CHECK(frames > 0, "the requested declared scenario exists");
  std::println(
      "device={} raster_frames={} profile={}x{}", device->Limits().Name, frames, width, height);
  return Report();
}
