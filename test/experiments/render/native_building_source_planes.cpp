#include "BuildingSourceCapture.h"
#include "ReadTextFile.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
namespace Fs = std::filesystem;

[[noreturn]] void Fail(std::string_view reason) {
  std::println(stderr, "{}", reason);
  std::_Exit(1);
}

Json Document(const Fs::path &path) {
  const auto bytes = ReadTextFile(path.string(), size_t{1024} * 1024);
  if (!bytes) { Fail(bytes.error()); }
  const auto data = Json::parse(*bytes, nullptr, false);
  if (!data.is_object()) { Fail("object metadata required"); }
  return data;
}

template <class T> void Write(const Fs::path &path, std::span<const T> bytes) {
  std::ofstream stream(path, std::ios::binary);
  stream.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size_bytes()));
  if (!stream) { Fail("output write failed"); }
}

void SaveFields(const ImpostorAtlas &atlas, const Fs::path &path) {
  std::vector<float> base;
  for (const auto &view : atlas.Views()) {
    for (size_t at = 0; at < view.Texels.size(); ++at) {
      for (const auto channel : view.Materials[at].BaseColour) { base.push_back(channel); }
      base.push_back(view.Texels[at].Surface == 0 ? 0.0f : 1.0f);
    }
  }
  Write<float>(path / "base.linear", base);
}

void Run(const Fs::path &inputPath, const Json &request, const Fs::path &output) {
  const auto began = Clock::now();
  Test::BuildingSourceCapture source(inputPath, request);
  const auto selected =
      request.contains("view") ? std::optional(request.at("view").get<size_t>()) : std::nullopt;
  const auto atlas =
      source.Capture(selected, request.value("export_geometry", false) ? output : Fs::path{});
  std::string error;
  const auto encoded = atlas.Encode("native-source-planes-impostor-1", error);
  if (!encoded) { Fail(error); }
  Write<uint8_t>(output / "atlas.bin", *encoded);
  SaveFields(atlas, output);
  Json report = source.Report();
  report.update(
      {{"atlas_bytes", encoded->size()},
       {"pixels", request.at("pixels")},
       {"elapsed_ms", std::chrono::duration<double, std::milli>(Clock::now() - began).count()},
       {"scope",
        "Source index and lazy analytical forms only. No native fine/shell mesher, "
        "world/SSE/occlusion admission or device frame-budget claim."}});
  std::ofstream reportFile(output / "source-result.json");
  reportFile << report.dump(2) << '\n';
  if (!reportFile) { Fail("report write failed"); }
}
}

int main(int argc, char *const *argv) {
  if (argc != 4) { Fail("expected original-input.json request.json new-output-directory"); }
  const Fs::path output(argv[3]);
  if (Fs::exists(output) || !Fs::create_directories(output)) {
    Fail("new output directory required");
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) { Fail(SDL_GetError()); }
  Run(argv[1], Document(argv[2]), output);
  SDL_Quit();
  return 0;
}
