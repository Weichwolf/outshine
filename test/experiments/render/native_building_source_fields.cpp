#include "BuildingSourceCapture.h"
#include "BuildingFieldRequest.h"
#include "PreparedBuildingAssets.h"
#include "Tasks.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "ReadTextFile.h"
#include "HeapProbe.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <print>
#include <ratio>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using Json = nlohmann::json;
namespace Fs = std::filesystem;
using Clock = std::chrono::steady_clock;

[[noreturn]] void Fail(std::string_view reason) {
  std::println(stderr, "{}", reason);
  std::_Exit(1);
}

Json Document(const Fs::path &path) {
  const auto bytes = ReadTextFile(path.string(), size_t{1024} * 1024);
  if (!bytes) { Fail(bytes.error()); }
  const auto value = Json::parse(*bytes, nullptr, false);
  if (!value.is_object()) { Fail("object request required"); }
  return value;
}

Vec3 Vector(const Json &value) {
  return Vec3{value.get<std::array<double, 3>>()};
}

Json Vector(const Vec3 &value) {
  return {value[0], value[1], value[2]};
}

Json Description(const BuildingSourceDemand::Source &source) {
  return {{"base_key", source.BaseKey},
          {"least", Vector(source.Bounds.Min)},
          {"most", Vector(source.Bounds.Max)},
          {"anchor", Vector(source.AnchorEcef)},
          {"cells", source.Cells},
          {"height_qualified", source.HeightQualified}};
}

BuildingSourceDemand::Source Description(const Json &source) {
  return {.RequestKey = {},
          .BaseKey = source.at("base_key").get<std::string>(),
          .Bounds = {.Min = Vector(source.at("least")), .Max = Vector(source.at("most"))},
          .AnchorEcef = Vector(source.at("anchor")),
          .Cells = source.at("cells").get<uint64_t>(),
          .HeightQualified = source.at("height_qualified").get<bool>()};
}

BuildingFieldRequest::Budget Budget(const Json &limits) {
  return {.ProducerBytes = limits.at("producer_bytes").get<size_t>(),
          .ResidentBytes = limits.at("resident_bytes").get<size_t>(),
          .FieldBytesMost = limits.at("field_bytes").get<size_t>(),
          .RasterBytesMost = limits.at("raster_bytes").get<size_t>(),
          .Capture = {.Samples = limits.at("samples").get<uint64_t>(),
                      .Sources = limits.at("sources").get<uint64_t>(),
                      .Faces = limits.at("faces").get<uint64_t>(),
                      .TriangleAttempts = limits.at("triangle_attempts").get<uint64_t>()}};
}

Json Costs(const Generators::PreparedBuildingAssets::Counters &costs) {
  return {{"header_hits", costs.Hits},
          {"header_bytes", costs.ReadBytes},
          {"form_hits", costs.SurfaceHits},
          {"form_bytes", costs.SurfaceReadBytes},
          {"form_writes", costs.SurfaceWrites}};
}

void Write(const Fs::path &path, const Json &value) {
  std::ofstream output(path);
  output << value.dump(2) << '\n';
  if (!output) { Fail("JSON result write failed"); }
}

BuildingSourceDemand::Source Import(const Fs::path &input,
                                    const Json &request,
                                    const Fs::path &root,
                                    std::shared_ptr<Generators::PreparedBuildingAssets> &sources) {
  BuildingSourceDemand::Source descriptor;
  {
    Test::BuildingSourceCapture archive(input, request);
    auto stored = archive.StorePrepared(root / "native");
    if (!stored) { Fail(stored.error()); }
    descriptor = std::move(*stored);
    Write(root / "import-result.json", archive.Report());
  }
  Data::RawSourceCache raw({.Directory = (root / "sources").string()});
  const Data::SourceSet inputs(raw);
  auto assets = Generators::PreparedBuildingAssets::Open((root / "native").string(), inputs);
  if (!assets) { Fail(assets.error()); }
  sources = std::move(*assets);
  const auto migrated = sources->Load(descriptor.BaseKey);
  if (!migrated || !*migrated) { Fail("native index/cell migration failed"); }
  Write(root / "import-costs.json", Costs(sources->Costs()));
  return descriptor;
}

std::shared_ptr<const BuildingFieldRequest::Product>
Produce(const Json &request,
        const BuildingSourceDemand::Source &source,
        const std::shared_ptr<Generators::PreparedBuildingAssets> &sources,
        const std::shared_ptr<AssetCache> &cache,
        Tasks &workers,
        AssetPipeline &pipeline,
        Json &report) {
  const auto began = Clock::now();
  BuildingFieldRequest::Demand demand;
  demand.Pixels = request.at("pixels").get<uint32_t>();
  demand.View.Value = request.value("view", uint32_t{0});
  for (const auto &direction : request.at("directions")) {
    demand.Directions.push_back(Vector(direction));
  }
  auto field = BuildingFieldRequest::Start(
      pipeline, workers, cache, sources, source, demand, Budget(request.at("limits")));
  if (!field) { Fail("native field admission failed"); }
  HeapProbe::ForgetPeak();
  size_t reserved = 0;
  do {
    reserved = std::max(reserved, pipeline.Counts().InputBytes);
    (void)HeapProbe::Sample();
    auto ready = field->Poll();
    if (!ready) { Fail(ready.error()); }
    if (*ready) {
      report["request_ms"] =
          std::chrono::duration<double, std::milli>(Clock::now() - began).count();
      report["reserved_input_bytes_peak"] = reserved;
      report["sampled_host_heap_bytes_peak"] = HeapProbe::PeakLiveBytes();
      return std::move(*ready);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (Clock::now() - began < std::chrono::seconds(30));
  Fail("native field request deadline expired");
}

void Export(const BuildingFieldRequest::Product &product, const Fs::path &root, Json &report) {
  const auto &atlas = *product.Field;
  std::string error;
  const auto encoded = atlas.Encode("native-original-impostor-1", error);
  if (!encoded) { Fail(error); }
  std::ofstream file(root / "atlas.bin", std::ios::binary);
  file.write(reinterpret_cast<const char *>(encoded->data()),
             static_cast<std::streamsize>(encoded->size()));
  if (!file) { Fail("native field export failed"); }
  report["source"] = Description(product.Source);
  report["atlas_bytes"] = encoded->size();
  report["centre"] = Vector(atlas.CentreM());
  report["half_extent_m"] = atlas.HalfExtentM();
  report["pixels"] = atlas.Pixels();
  report["views"] = atlas.Views().size();
  report["covered"] = std::count_if(atlas.Views()[0].Texels.begin(),
                                    atlas.Views()[0].Texels.end(),
                                    [](const auto &texel) { return texel.Surface != 0; });
  if (!product.Metadata->Metadata().WorldFromField) { Fail("native field has no World frame"); }
  const auto &frame = *product.Metadata->Metadata().WorldFromField;
  report["world_from_field"] = Json::array();
  for (size_t row = 0; row < 4; ++row) {
    for (size_t column = 0; column < 4; ++column) {
      report["world_from_field"].push_back(frame.At(row, column));
    }
  }
}
}

int main(int argc, char *const *argv) {
  if (argc != 4) { Fail("expected source-input.json request.json new-output-directory"); }
  const Fs::path root(argv[3]);
  std::error_code error;
  const bool exists = Fs::exists(root, error);
  if (error || exists || !Fs::create_directories(root, error) || error) {
    Fail("fresh output directory required");
  }
  const auto request = Document(argv[2]);
  std::shared_ptr<Generators::PreparedBuildingAssets> sources;
  const bool importing = !request.contains("source");
  const auto source =
      importing ? Import(argv[1], request, root, sources) : Description(request.at("source"));
  if (importing && !SDL_Init(SDL_INIT_VIDEO)) { Fail("native source video initialization failed"); }
  {
    const Fs::path cachePath = request.value("cache", (root / "fields.sqlite").string());
    auto opened = AssetCache::Open(cachePath.string());
    if (!opened) { Fail("native field cache unavailable"); }
    const std::shared_ptr<AssetCache> cache(std::move(*opened));
    const auto limits = Budget(request.at("limits"));
    if (limits.ProducerBytes > (size_t{1} << 28) || limits.ResidentBytes > (size_t{1} << 28)) {
      Fail("diagnostic input reservations exceed the declared one-GiB scope");
    }
    const auto before = sources ? Costs(sources->Costs()) : Costs({});
    Tasks workers(1);
    AssetPipeline pipeline(workers,
                           {.JobsMost = 2,
                            .ConsumersMost = 2,
                            .TicketsMost = 4,
                            .InputBytesMost = 2 * (limits.ProducerBytes + limits.ResidentBytes),
                            .ReceiptBytesMost = 16384});
    Json report{{"native_sources_present", importing}, {"source_costs_before", before}};
    const auto product = Produce(request, source, sources, cache, workers, pipeline, report);
    Export(*product, root, report);
    report["source_costs_after"] = sources ? Costs(sources->Costs()) : Costs({});
    report["scope"] =
        "Native BuildingFieldRequest; import excluded from request timing, "
        "sampled host heap excludes unsampled/GPU/driver peaks; no World/pose/p99/8GB pass";
    Write(root / "native-field-result.json", report);
  }
  if (importing) { SDL_Quit(); }
  return 0;
}
