#include "PreparedOsmTiles.h"
#include "Check.h"
#include "MvtWireFixture.h"
#include "OsmCacheFixture.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ios>
#include <print>
#include <memory>
#include <ratio>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;

struct Result {
  std::shared_ptr<const OsmField> Product;
  bool Failed = false;
  bool WithinBudget = true;
};

Result Await(PreparedOsmTiles &cache,
             const OsmField &layout,
             Tasks &compute,
             size_t budget,
             TileAt at = {.X = 1, .Y = 1}) {
  Result result;
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!result.Product && !result.Failed && std::chrono::steady_clock::now() < until) {
    auto step = cache.Acquire(at, layout);
    result.WithinBudget = result.WithinBudget && cache.AdmissionCounts().InputBytes <= budget;
    if (!step) {
      result.Failed = true;
    } else if (step->Status == PreparedOsmTiles::State::Ready) {
      result.Product = std::move(step->Product);
    }
    (void)compute.AwaitCompletion(0.001);
  }
  return result;
}

void ColdAndWarm(const std::filesystem::path &directory) {
  constexpr size_t budget = size_t{1} * 1024 * 1024;
  const std::array<std::string, 1> layers{"buildings"};
  const OsmField layout(2, layers, MvtSchema::OpenMapTiles);
  OsmCache::NoTransport transport;
  Tasks compute(2);
  std::atomic_uint calls{0};
  auto source = OsmCache::OpenSession(
      std::make_unique<OsmCache::VectorSource>("r1", Mvt::BuildingTile(), calls));
  CHECK(source, "a finite source opens for the low-budget cold producer");
  if (!source) { return; }
  auto cache = PreparedOsmTiles::Open(
      directory.string(), *source, transport, compute, {.InputBytesMost = budget});
  CHECK(cache, "the actual cold service opens far below the old 384-MiB constant");
  if (!cache) { return; }
  const auto cold = Await(**cache, layout, compute, budget);
  const auto costs = (*cache)->Costs();
  CHECK(cold.Product && !cold.Failed && cold.WithinBudget && calls == 1 && costs.Generations == 1 &&
            costs.Writes == 1 && !cold.Product->Points().empty(),
        "one complete cold source/normalize/encode/compress/publish/decode advances under shared "
        "admission");
  cache->reset();
  source->reset();
  std::atomic_uint forbidden{0};
  auto offline = OsmCache::OpenSession(
      std::make_unique<OsmCache::VectorSource>("r1", std::vector<uint8_t>{}, forbidden));
  CHECK(offline, "an unavailable source opens for a fresh offline service");
  if (!offline) { return; }
  constexpr size_t warmBudget = size_t{128} * 1024;
  auto fresh = PreparedOsmTiles::Open(
      directory.string(), *offline, transport, compute, {.InputBytesMost = warmBudget});
  CHECK(fresh, "the warm budget includes stored/native buffers and the explicit Zstd context");
  if (!fresh) { return; }
  const auto warm = Await(**fresh, layout, compute, warmBudget);
  CHECK(warm.Product && !warm.Failed && warm.WithinBudget && forbidden == 0 &&
            (*fresh)->Costs().Generations == 0 && (*fresh)->Costs().Hits == 1,
        "the new codec keeps the persisted native hit free of source work and regeneration");
}

void RealCold(const std::filesystem::path &directory) {
  std::ifstream stream("test/outshine/data/transport/wien-junction-14-8937-5678.pbf",
                       std::ios::binary);
  const std::vector<uint8_t> input{std::istreambuf_iterator<char>(stream), {}};
  CHECK(!input.empty(), "a versioned real MVT supplies the actual cold-job input");
  if (input.empty()) { return; }
  constexpr size_t budget = size_t{4} * 1024 * 1024;
  const std::array<std::string, 6> layers{
      "buildings", "streets", "water_polygons", "water_lines", "land", "sites"};
  const OsmField layout(14, layers, MvtSchema::OpenMapTiles);
  OsmCache::NoTransport transport;
  Tasks compute(2);
  std::atomic_uint calls{0};
  auto source = OsmCache::OpenSession(std::make_unique<OsmCache::VectorSource>("r1", input, calls));
  CHECK(source, "the real-input source opens without external network acquisition");
  if (!source) { return; }
  auto cache = PreparedOsmTiles::Open(
      directory.string(), *source, transport, compute, {.InputBytesMost = budget});
  CHECK(cache, "the real-input service opens with a bounded generation budget");
  if (!cache) { return; }
  const auto began = std::chrono::steady_clock::now();
  const auto result = Await(**cache, layout, compute, budget, {.X = 8937, .Y = 5678});
  const double ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  const auto costs = (*cache)->Costs();
  CHECK(result.Product && !result.Failed && result.WithinBudget && calls == 1 &&
            costs.Generations == 1 && costs.Writes == 1 &&
            result.Product->Features().size() == 1132,
        "the real 1132-feature source reaches native ownership through the eight-MiB admitted cold "
        "job");
  const auto plan = OsmField::NativeTileBuildBytes(input, layers, MvtSchema::OpenMapTiles);
  CHECK(plan, "the same real source has a finite structural generation bound");
  if (plan) {
    std::println("REAL_COLD input_bytes={} working_bytes_most={} encoded_bytes_most={} "
                 "admission_most={} ready_ms={:.6f} generation_ms={:.6f}",
                 input.size(),
                 plan->WorkingBytesMost,
                 plan->EncodedBytesMost,
                 budget,
                 ms,
                 costs.GenerationMs);
  }
}

void RefuseBeforeGeneration(const std::filesystem::path &directory) {
  constexpr size_t budget = size_t{128} * 1024;
  std::atomic_uint calls{0};
  OsmCache::NoTransport transport;
  Tasks compute(1);
  auto source = OsmCache::OpenSession(
      std::make_unique<OsmCache::VectorSource>("r1", Mvt::BuildingTile(), calls));
  CHECK(source, "the source fits the deliberately insufficient generation budget");
  if (!source) { return; }
  auto cache = PreparedOsmTiles::Open(
      directory.string(), *source, transport, compute, {.InputBytesMost = budget});
  CHECK(cache, "a constrained producer opens before resolving its actual input");
  if (!cache) { return; }
  const std::array<std::string, 1> layers{"buildings"};
  const OsmField layout(2, layers, MvtSchema::OpenMapTiles);
  const auto result = Await(**cache, layout, compute, budget);
  CHECK(result.Failed && !result.Product && result.WithinBudget && calls == 1 &&
            (*cache)->Costs().Generations == 0 && (*cache)->Costs().Writes == 0,
        "the job refuses its full planned peak before constructing or encoding any field");
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-cold-plan-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  ColdAndWarm(directory / "ready");
  RealCold(directory / "real");
  RefuseBeforeGeneration(directory / "refused");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated cache files are removed after every owner has retired");
  return Report();
}
