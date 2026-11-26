#include "BuiltinSourceProviders.h"
#include "Check.h"
#include "SurfacePreparation.h"
#include "TerrainPathPreparation.h"
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Data;

class InterleavedTransport final : public SourceTransport {
public:
  FetchStart Begin(const std::string &url) override {
    if (url.starts_with("https://elevation.fixture.invalid/")) {
      ElevationStarted.store(true);
      return Elevation;
    }
    if (url.starts_with("https://vector.fixture.invalid/")) { return Vector; }
    return std::unexpected(SourceFailureReason::ProviderRefused);
  }

  FetchStart Begin(const std::string &url, size_t) override { return Begin(url); }

  Wire Collect(Ticket ticket) override {
    if (ticket == Vector && !ElevationStarted.load()) { return Wire::Working(); }
    return Wire::Answered(404, {});
  }

  void Cancel(Ticket) override {}

  std::atomic<bool> ElevationStarted{false};

private:
  static constexpr auto Vector = static_cast<Ticket>(1);
  static constexpr auto Elevation = static_cast<Ticket>(2);
};

struct TemporarySources {
  std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-path-sources-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  ~TemporarySources() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};
}

int main() {
  using namespace outshine::Test;
  const TemporarySources sources;
  InterleavedTransport transport;
  SourceProvider elevation;
  elevation.Kind = "terrain";
  elevation.Dataset = "fixture.elevation";
  elevation.Endpoint = "https://elevation.fixture.invalid/{z}/{x}/{y}.webp";
  SourceProvider vector;
  vector.Kind = "vector";
  vector.Dataset = "fixture.vector";
  vector.Endpoint = "https://vector.fixture.invalid/{z}/{x}/{y}.pbf";
  vector.Schema = "shortbread";
  const std::array providers{elevation, vector};
  Tasks compute(1);
  ProviderRegistry registry;
  Generators::RegisterBuiltinSourceProviders(registry);
  Ground::SurfacePreparation stack;
  CHECK(stack.Open({.BuiltinData = "data", .Sources = sources.Path.string(), .Assets = {}},
                   providers,
                   registry,
                   {.LongitudeDeg = 0, .LatitudeDeg = 0},
                   transport,
                   compute,
                   nullptr,
                   1),
        "camera source preparation uses registered terrain and vector endpoints");
  if (!stack.Opened()) { return Report(); }
  CHECK(!transport.ElevationStarted.load(), "opening does not fetch the camera path");
  const auto prepared =
      PrepareTerrainPath({.Fields = {{.Zoom = 0, .X = 0, .Y = 0}},
                          .Vectors = {{.Zoom = stack.VectorZoom(), .X = 0, .Y = 0}},
                          .Meshes = {}},
                         stack,
                         std::chrono::steady_clock::now() + std::chrono::seconds(2));
  CHECK(prepared.has_value() && transport.ElevationStarted.load(),
        "known heights progress while vector data is still pending");
  return Report();
}
