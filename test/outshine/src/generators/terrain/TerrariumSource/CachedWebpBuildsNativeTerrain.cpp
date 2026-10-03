#include "ShippedProviders.h"
#include "SourceConfiguration.h"
#include "SourceSet.h"
#include "OfflineTransport.h"
#include "TilePool.h"
#include "Check.h"
#include <webp/encode.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Data;

class Tiles final : public Transport {
public:
  explicit Tiles(std::vector<uint8_t> bytes) : Bytes_(std::move(bytes)) {}

  std::atomic<unsigned> Starts = 0;
  std::atomic<unsigned> InvalidEndpoints = 0;

  FetchStart Begin(const std::string &url) override {
    if (!url.starts_with("https://fixture.invalid/height/") || !url.ends_with(".webp")) {
      ++InvalidEndpoints;
    }
    return static_cast<Ticket>(++Starts);
  }

  Wire Collect(Ticket) override { return Wire::Answered(200, Bytes_); }

  void Cancel(Ticket) override {}

private:
  std::vector<uint8_t> Bytes_;
};

void Build(ContentStore &cache, Transport &transport, ProviderRegistry &registry, bool warm) {
  const SourceProvider config{.Kind = "terrain",
                              .Revision = "fixture-2026",
                              .Missing = MissingDataPolicy::Fail,
                              .Dataset = "fixture.terrarium",
                              .Endpoint = "https://fixture.invalid/height/{z}/{x}/{y}.webp"};
  SourceSet sources(cache);
  std::string error;
  CHECK(RegisterSources(sources, std::span(&config, 1), {}, error, registry), error.c_str());
  if (sources.Count() != 1) { return; }
  CHECK(sources.At(0).Declaration().Wire == WireFormat::TerrariumWebp,
        "factory declares actual network encoding independently of native heights");
  Ground::TilePool pool({.Threads = 2, .ByteBudget = 1024u * 1024u}, sources, transport);
  const TileId at{.Zoom = 12, .X = 2048, .Y = 2048};
  std::shared_ptr<const Ground::TerrainField> field;
  Ground::TilePool::Reply reply = Ground::TilePool::Reply::Pending;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    reply = pool.Field(at, &field);
    if (reply == Ground::TilePool::Reply::Pending) { (void)pool.AwaitLanding(0.01); }
  } while (reply == Ground::TilePool::Reply::Pending &&
           std::chrono::steady_clock::now() < deadline);
  CHECK(reply == Ground::TilePool::Reply::Ready && field,
        "registered WebP provider uses existing source, compute and native terrain queues");
  if (field) {
    CHECK(field->Rows() == 8 && field->Cols() == 8 && !field->HasMissingBoundary(),
          "all neighbouring source rasters contribute to a complete native field");
    CHECK_NEAR(
        field->AtM(3, 5), 172.875, 0.0, "m", "fractional heights survive native publication");
    CHECK(field->Sources().size() == 9 && field->Certificate().IsComplete(),
          "native terrain retains every neighbouring source and delivery stamp");
  }
  const auto bytes = cache.Read(ContentKey(sources.At(0).Declaration(), Address::At(at)));
  CHECK(bytes && bytes->size() >= 12 && (*bytes)[0] == 'R' && (*bytes)[8] == 'W',
        "persistent cache retains original WebP, never generated float geometry");
  CHECK((sources.Counters().RemoteStarts == 0) == warm,
        "fresh offline world rebuild consumes cached bytes without network acquisition");
}
}

int main() {
  using namespace outshine::Test;
  std::array<uint8_t, 8 * 8 * 3> rgb{};
  for (size_t at = 0; at < rgb.size(); at += 3) {
    rgb[at] = 128;
    rgb[at + 1] = 172;
    rgb[at + 2] = 224;
  }
  uint8_t *encoded = nullptr;
  const size_t count = WebPEncodeLosslessRGB(rgb.data(), 8, 8, 24, &encoded);
  const std::unique_ptr<uint8_t, decltype(&WebPFree)> owned(encoded, WebPFree);
  CHECK(count > 0 && owned, "independent encoder creates exact source fixture");
  if (count == 0 || !owned) { return Report(); }
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-native-webp-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated network cache created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  {
    ContentStore cache({.Directory = directory});
    ProviderRegistry registry;
    Generators::RegisterShippedProviders(registry);
    Tiles online({owned.get(), owned.get() + count});
    Build(cache, online, registry, false);
    CHECK(online.InvalidEndpoints == 0, "configured adapter expands its own endpoint");
    CHECK(online.Starts == 9, "one request per actual required source tile");
    OfflineTransport offline;
    Build(cache, offline, registry, true);
  }
  std::filesystem::remove_all(directory);
  return Report();
}
