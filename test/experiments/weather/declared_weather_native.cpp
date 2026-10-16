#include <Outshine.h>
#include <world/weather/WeatherSnapshot.h>
#include "CloudViewProbe.h"
#include "MediumViewProbe.h"
#include "NativeGpuCheck.h"
#include "ScenarioRead.h"
#include "Check.h"

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <print>
#include <string>
#include <string_view>
#include <system_error>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using namespace outshine::Test::CloudViewProbe;

Scenario::Document Read(const char *path) {
  constexpr std::streamoff kMaximumInputBytes = 16384;
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const auto bytes = file ? static_cast<std::streamoff>(file.tellg()) : -1;
  CHECK(bytes > 0 && bytes <= kMaximumInputBytes, "the private declared input is bounded");
  if (bytes <= 0 || bytes > kMaximumInputBytes) { Failed(); }
  std::string input(static_cast<size_t>(bytes), '\0');
  file.seekg(0);
  CHECK(file.read(input.data(), bytes), "the original scenario input is read completely");
  Scenario::Document scenario;
  std::string error;
  CHECK(ReadScenario(input.data(), input.size(), scenario, error),
        "the original native weather scenario passes the production parser");
  CHECK(scenario.Weather.has_value(), "an explicit weather input exists");
  return scenario;
}

uint32_t Extent(const char *text) {
  constexpr uint32_t kMaximum = 8192;
  uint32_t value = 0;
  const std::string_view input(text);
  const auto parsed = std::from_chars(input.data(), input.data() + input.size(), value);
  const bool valid = parsed.ec == std::errc{} && parsed.ptr == input.data() + input.size() &&
                     value > 0 && value <= kMaximum;
  CHECK(valid, "the private capture extent is valid and bounded");
  if (!valid) { Failed(); }
  return value;
}
}

int main(int argc, char **argv) {
  if (argc != 7 && argc != 9) {
    std::println(stderr,
                 "usage: declared-weather-native scene.vert scene.frag fullscreen.vert "
                 "front.frag output-prefix scenario [width height]");
    return 2;
  }
  const uint32_t width = argc == 9 ? Extent(argv[7]) : MediumViewProbe::kWidth;
  const uint32_t height = argc == 9 ? Extent(argv[8]) : MediumViewProbe::kHeight;
  constexpr uint32_t kRowAlignment = 256;
  const MediumViewProbe::ImageShape shape{
      .Width = width,
      .Height = height,
      .RowBytes = (width * static_cast<uint32_t>(sizeof(Pixel)) + kRowAlignment - 1) /
                  kRowAlignment * kRowAlignment};
  Engine engine;
  CHECK(engine.setRoots({.Assets = {},
                         .Shipped = {},
                         .Cache = std::filesystem::path(argv[5]).parent_path().string(),
                         .Offline = true,
                         .AssetCache = {}}) &&
            engine.declare(Read(argv[6])) && engine.assemble() && engine.preload(0.0),
        "the actual public engine publishes the parsed declaration headlessly");
  const auto selected = engine.weather();
  CHECK(selected && *selected && (**selected).Source.Origin == WeatherOrigin::ScenarioDeclaration,
        "the actual raster input comes from the owned engine weather publication");
  if (!selected || !*selected) { return Report(); }
  const auto loading = engine.loading();
  CHECK(loading.ProviderStarts == 0 && loading.SourceDeliveries == 0 &&
            loading.SourceFromStore == 0 && loading.Sources.empty() && loading.GroundWanted == 0 &&
            loading.VectorWanted == 0,
        "the declared raster case starts no provider or terrain work");
  auto scenario = DeclaredSkyCase(shape, **selected);
  scenario.Profile = CloudProfile::CumulusV1;
  auto device = Require(CreateDevice({.Validation = true}));
  const auto scene = MakePipeline(*device, argv[1], argv[2], true);
  const auto front = MakePipeline(*device, argv[3], argv[4], false);
  const Targets targets(*device, shape);
  const auto first = targets.Draw(*device, {.Scene = *scene, .Front = *front}, scenario);
  const auto repeat = targets.Draw(*device, {.Scene = *scene, .Front = *front}, scenario);
  CHECK(first.Images == repeat.Images, "repeated declared-input raster output is byte-identical");
  WriteCapture(scenario, first, shape, std::string(argv[5]) + "." + scenario.Name);
  std::println("device={} raster_frames=2 profile={}x{} selected_utc_s={} provider_starts={}",
               device->Limits().Name,
               width,
               height,
               (**selected).Values.ValidUtcS,
               loading.ProviderStarts);
  return Report();
}
