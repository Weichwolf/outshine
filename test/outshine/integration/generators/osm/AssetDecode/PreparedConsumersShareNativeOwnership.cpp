#include "AssetCache.h"
#include "AssetProduct.h"
#include "Check.h"
#include "MvtWireFixture.h"
#include "OsmCacheFixture.h"
#include "PreparedOsmTiles.h"
#include <algorithm>
#include <array>
#include <optional>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using Product = std::shared_ptr<const OsmField>;
constexpr TileAt kAt{.X = 1, .Y = 1};
constexpr size_t kPackageMost = size_t{64} * 1024 * 1024;

bool Poll(PreparedOsmTiles &cache, const OsmField &layout, Product &product) {
  if (product) { return true; }
  auto result = cache.Acquire(kAt, layout);
  CHECK(result, "a native owner advances its admitted logical request");
  if (!result) { return false; }
  if (result->Status == PreparedOsmTiles::State::Ready) { product = std::move(result->Product); }
  return true;
}

struct Products {
  Product First, Second;
};

void ReleaseOwners(PreparedOsmTiles &cache, Products products, size_t decoded) {
  auto &a = products.First;
  auto &b = products.Second;
  const auto held = cache.AdmissionCounts();
  CHECK(held.Jobs == 0 && held.Consumers == 2 && held.Tickets == 0 && held.Running == 0 &&
            held.InputBytes == 2 * decoded,
        "both decoder reservations remain charged while the shared raw package has retired");
  a.reset();
  const auto single = cache.AdmissionCounts();
  CHECK(single.Jobs == 0 && single.Consumers == 1 && single.Tickets == 0 &&
            single.InputBytes == decoded,
        "the first owner release frees only its decoded storage");
  Product alias = b;
  b.reset();
  CHECK(cache.AdmissionCounts().InputBytes == single.InputBytes && !alias->Features().empty(),
        "a retained product alias keeps its decoded admission alive");
  alias.reset();
  const auto empty = cache.AdmissionCounts();
  CHECK(empty.Jobs == 0 && empty.Consumers == 0 && empty.Tickets == 0 && empty.InputBytes == 0 &&
            empty.ReceiptBytes == 0,
        "the final actual product release retires all admission without an owner registry leak");
}

std::optional<size_t> DecoderBytes(const std::filesystem::path &directory) {
  auto store = AssetCache::Open((directory / "assets.sqlite").string());
  CHECK(store, "the real persisted native product can be inspected independently");
  if (!store) { return std::nullopt; }
  auto rows = (*store)->Select({.Bounds = {.Min = {{-1e9, -1e9, -1e9}}, .Max = {{1e9, 1e9, 1e9}}},
                                .Kind = "osm-tile",
                                .Level = 2,
                                .Radius = std::nullopt,
                                .Planes = {}});
  CHECK(rows && rows->size() == 1, "one native content record owns the shared package");
  if (!rows || rows->size() != 1) { return std::nullopt; }
  auto raw = (*store)->Load(rows->front().Key, kPackageMost);
  CHECK(raw && *raw, "the measured decoder plan comes from the actual committed bytes");
  if (!raw || !*raw) { return std::nullopt; }
  const AssetProduct native(std::move(**raw));
  const auto decoded = OsmField::NativeTileDecodeBytes(native.Native()->Bytes());
  CHECK(decoded, "the native structure declares its owner allocation bound");
  if (!decoded) { return std::nullopt; }
  return decoded;
}

void Shared(const std::filesystem::path &directory) {
  std::atomic_uint calls{0};
  Test::OsmCache::NoTransport transport;
  Tasks compute(2);
  auto session = Test::OsmCache::OpenSession(
      std::make_unique<Test::OsmCache::VectorSource>("r1", Test::Mvt::BuildingTile(), calls));
  CHECK(session, "a real MVT provider opens in an owned session");
  if (!session) { return; }
  auto opened = PreparedOsmTiles::Open(directory.string(), *session, transport, compute);
  CHECK(opened, "the actual native producer opens with shared asset admission");
  if (!opened) { return; }
  auto cache = std::move(*opened);
  const std::array<std::string, 1> layers{"buildings"};
  const OsmField first(2, layers, MvtSchema::OpenMapTiles);
  const OsmField second(2, layers, MvtSchema::OpenMapTiles);
  Product a;
  Product b;
  CHECK(Poll(*cache, first, a) && Poll(*cache, second, b),
        "both consumers submit before completion");
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while ((!a || !b) && std::chrono::steady_clock::now() < until) {
    if (!Poll(*cache, first, a) || !Poll(*cache, second, b)) { return; }
    (void)compute.AwaitCompletion(0.001);
  }
  CHECK(a && b, "both owner decoders receive complete native geometry");
  if (!a || !b) { return; }
  CHECK(a.get() != b.get() && std::ranges::equal(a->Points(), b->Points()),
        "independent owner products preserve identical source geometry");
  const auto costs = cache->Costs();
  CHECK(calls == 1 && costs.Misses == 1 && costs.Generations == 1 && costs.Writes == 1 &&
            costs.PublicationBytes == 0,
        "two actual consumers share one source resolution, generation and commit");
  const auto decoded = DecoderBytes(directory);
  if (!decoded) { return; }
  ReleaseOwners(*cache, {.First = std::move(a), .Second = std::move(b)}, *decoded);
  const OsmField warm(2, layers, MvtSchema::OpenMapTiles);
  Product replay;
  const auto replayUntil = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!replay && std::chrono::steady_clock::now() < replayUntil) {
    if (!Poll(*cache, warm, replay)) { return; }
    (void)compute.AwaitCompletion(0.001);
  }
  CHECK(replay && calls == 1 && cache->Costs().Hits == 1 && cache->Costs().Generations == 1,
        "a later owner consumes the native hit without revisiting source normalization");
  const std::weak_ptr<Data::SourceSession> source = *session;
  const std::weak_ptr<PreparedOsmTiles> service = cache;
  session->reset();
  cache.reset();
  CHECK(service.expired() && source.expired() && replay && !replay->Points().empty(),
        "a finished product outlives its service without retaining the source session or a cycle");
}

void BoundedWarm(const std::filesystem::path &directory) {
  std::atomic_uint calls{0};
  Test::OsmCache::NoTransport transport;
  Tasks compute(1);
  auto session = Test::OsmCache::OpenSession(
      std::make_unique<Test::OsmCache::VectorSource>("r1", std::vector<uint8_t>{}, calls));
  CHECK(session, "an unavailable provider opens for the bounded native replay");
  if (!session) { return; }
  constexpr size_t budget = size_t{64} * 1024;
  auto opened = PreparedOsmTiles::Open(
      directory.string(), *session, transport, compute, {.InputBytesMost = budget});
  CHECK(opened, "a small native cache opens with its actual available RAM bound");
  if (!opened) { return; }
  const std::array<std::string, 1> layers{"buildings"};
  const OsmField layout(2, layers, MvtSchema::OpenMapTiles);
  Product product;
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!product && std::chrono::steady_clock::now() < until) {
    auto step = (*opened)->Acquire(kAt, layout);
    CHECK(step, "a valid small native hit advances within the declared RAM budget");
    if (!step) { return; }
    if (step->Status == PreparedOsmTiles::State::Ready) { product = std::move(step->Product); }
    CHECK((*opened)->AdmissionCounts().InputBytes <= budget,
          "lookup, package and decode steps never over-admit the bounded replay");
    (void)compute.AwaitCompletion(0.001);
  }
  CHECK(product && !product->Points().empty() && calls == 0 && (*opened)->Costs().Hits == 1 &&
            (*opened)->Costs().Generations == 0,
        "the constrained replay uses only its small persisted native product");
}

}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-prepared-admission-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Shared(directory);
  BoundedWarm(directory);
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "all isolated persisted products are removed after their consumers retire");
  return outshine::Test::Report();
}
