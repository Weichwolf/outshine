#include "Check.h"
#include "SurfacePreparation.h"
#include "PreparedGroundRegions.h"
#include "ReadTextFile.h"
#include "Sink.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <expected>
#include <string>
#include <system_error>

namespace {
class NoRequests final : public outshine::Data::Transport {
public:
  outshine::Data::FetchStart Begin([[maybe_unused]] const std::string &url) override {
    ++Begins;
    return std::unexpected(outshine::Data::FetchFailureReason::OfflineMiss);
  }

  outshine::Data::Wire Collect([[maybe_unused]] outshine::Data::Ticket ticket) override {
    return outshine::Data::Wire::Never(outshine::Data::FetchFailureReason::OfflineMiss);
  }

  void Cancel([[maybe_unused]] outshine::Data::Ticket ticket) override {}

  unsigned Begins = 0;
};

class SilentSink final : public outshine::Sink {
public:
  void Number([[maybe_unused]] const char *what,
              [[maybe_unused]] double value,
              [[maybe_unused]] const char *unit) override {}

  void Claim([[maybe_unused]] bool held, [[maybe_unused]] const char *why) override {}

  void Near(double got, double want, double within, const char *unit, const char *why) override {
    CHECK_NEAR(got, want, within, unit, why);
  }

  void Say([[maybe_unused]] const std::string &line) override {}
};

struct TemporaryCache {
  std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-material-region-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  ~TemporaryCache() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  TemporaryCache cache;
  std::error_code directoryError;
  std::filesystem::create_directories(cache.Path / "assets", directoryError);
  CHECK(!directoryError, "the private asset-cache directory exists");
  if (directoryError) { return Report(); }
  constexpr size_t bytesMost = size_t{1024} * 1024;
  auto materialRules = ReadTextFile("src/assets/world/ground-materials.json", bytesMost);
  auto vegetationRules = ReadTextFile("src/assets/world/vegetation.json", bytesMost);
  CHECK(materialRules && vegetationRules, "shipped material inputs are available");
  if (!materialRules || !vegetationRules) { return Report(); }
  materialRules->push_back('\0');
  materialRules->append(*vegetationRules);
  Data::ContentStore store({.Directory = {}, .Using = Data::ContentStore::Use::Off});
  const Data::SourceSet sources(store);
  auto legacy = Generators::Osm::PreparedGroundRegions::Open(
      (cache.Path / "assets").string(), sources, *materialRules);
  CHECK(legacy, "the previous region recipe opens without source acquisition");
  if (!legacy) { return Report(); }
  const Ground::ShapedGround shape;
  const std::array<uint8_t, 3> parameters{1, 2, 3};
  const std::array<Ground::TileSpot, 1> inputs{{{.Zoom = 2, .X = 1, .Y = 1}}};
  const auto request = [&](const Generators::Osm::PreparedGroundRegions &regions) {
    return regions.RequestKey(2, Generators::Osm::MvtSchema::Shortbread, shape, parameters, inputs);
  };
  const auto oldKey = request(**legacy);
  SilentSink sink;
  NoRequests wire;
  Tasks compute(1);
  Ground::SurfacePreparation stack;
  const auto open = [&] {
    return stack.Open({.Shipped = "src/assets",
                       .Cache = (cache.Path / "sources").string(),
                       .AssetCache = (cache.Path / "assets").string()},
                      {},
                      {},
                      wire,
                      compute,
                      sink,
                      nullptr);
  };
  CHECK(open() && stack.RegionAssets(), "the actual engine path binds its native region recipe");
  if (!stack.Opened() || !stack.RegionAssets()) { return Report(); }
  const auto currentKey = request(*stack.RegionAssets());
  CHECK(!oldKey.empty() && !currentKey.empty() && oldKey != currentKey,
        "unchanged inputs cannot bind a cached palette missing the litter constituents");
  stack.Close();
  CHECK(open() && stack.RegionAssets(), "a fresh world binds the same enriched product version");
  if (stack.Opened() && stack.RegionAssets()) {
    CHECK(request(*stack.RegionAssets()) == currentKey,
          "the next world retains its cache identity instead of rebuilding every time");
  }
  CHECK(wire.Begins == 0, "recipe binding performs no terrain or vector acquisition");
  return Report();
}
