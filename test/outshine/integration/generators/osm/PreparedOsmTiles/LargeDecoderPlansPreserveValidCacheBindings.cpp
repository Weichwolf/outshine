#include "BinaryValueArchive.h"
#include "Check.h"
#include "MvtWireFixture.h"
#include "OpenAssetStore.h"
#include "OsmCacheFixture.h"
#include "PreparedOsmTiles.h"
#include <sqlite3.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using Product = std::shared_ptr<const OsmField>;
constexpr size_t kFormerHeapQuota = size_t{128} * 1024 * 1024;
constexpr size_t kPackageMost = size_t{64} * 1024 * 1024;

Product Await(PreparedOsmTiles &service, const OsmField &definition, Tasks &compute) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  do {
    auto delivery = service.Acquire({.X = 1, .Y = 1}, definition);
    if (!delivery) { return {}; }
    if (delivery->Status == PreparedOsmTiles::State::Ready) { return std::move(delivery->Product); }
    (void)compute.AwaitCompletion(0.001);
  } while (std::chrono::steady_clock::now() < deadline);
  return {};
}

std::string Binding(const std::filesystem::path &path) {
  sqlite3 *raw = nullptr;
  const auto opened = sqlite3_open_v2(path.string().c_str(), &raw, SQLITE_OPEN_READONLY, nullptr);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
  if (opened != SQLITE_OK) { return {}; }
  sqlite3_stmt *query = nullptr;
  const auto prepared = sqlite3_prepare_v2(
      raw, "SELECT request_key FROM asset_requests LIMIT 2", -1, &query, nullptr);
  const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(query,
                                                                             sqlite3_finalize);
  if (prepared != SQLITE_OK || sqlite3_step(query) != SQLITE_ROW) { return {}; }
  const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(query, 0));
  const std::string result(text, static_cast<size_t>(sqlite3_column_bytes(query, 0)));
  return sqlite3_step(query) == SQLITE_DONE ? result : std::string{};
}

std::vector<uint8_t> ExtendKeys(std::span<const uint8_t> native, uint32_t count) {
  BinaryValueWriter marker(64);
  if (!marker(uint32_t{1}) || !marker.Text("render_height")) { return {}; }
  const auto match = std::ranges::search(native, marker.Out.Bytes());
  if (match.empty()) { return {}; }
  const auto prefix = static_cast<size_t>(match.end() - native.begin());
  const auto keyAt = static_cast<size_t>(match.begin() - native.begin());
  const auto most = native.size() + size_t{count} * (sizeof(uint32_t) + 8);
  BinaryValueWriter writer(most);
  writer.Out.Reserve(most);
  if (!writer.Out.Put(native.first(prefix)) || !writer.Out.NumberAt(keyAt, count + 1)) {
    return {};
  }
  for (uint32_t key = 0; key < count; ++key) {
    std::array<char, 8> name{};
    const auto [end, error] = std::to_chars(name.data(), name.data() + name.size(), key, 16);
    if (error != std::errc{} ||
        !writer.Text(std::string_view(name.data(), static_cast<size_t>(end - name.data())))) {
      return {};
    }
  }
  if (!writer.Out.Put(native.subspan(prefix))) { return {}; }
  return std::move(writer.Out).TakeBytes();
}

struct Seeded {
  AssetRecord Record;
  std::vector<uint8_t> Bytes;
  std::string Request;
};

std::optional<Seeded> Seed(const std::filesystem::path &directory,
                           const std::shared_ptr<AssetCache> &store,
                           Tasks &compute) {
  std::atomic_uint calls{0};
  Test::OsmCache::NoTransport transport;
  auto session = Test::OsmCache::OpenSession(
      std::make_unique<Test::OsmCache::VectorSource>("r1", Test::Mvt::BuildingTile(), calls));
  if (!session) { return std::nullopt; }
  auto opened = PreparedOsmTiles::Open(store, *session, transport, compute);
  if (!opened) { return std::nullopt; }
  const std::array<std::string, 1> layers{"buildings"};
  const OsmField definition(2, layers, MvtSchema::OpenMapTiles);
  if (!Await(**opened, definition, compute)) { return std::nullopt; }
  AssetQuery query;
  query.Bounds.Min = {{-1e9, -1e9, -1e9}};
  query.Bounds.Max = {{1e9, 1e9, 1e9}};
  query.Kind = "osm-tile";
  query.Level = 2;
  const auto rows = store->Select(query);
  if (!rows || rows->size() != 1) { return std::nullopt; }
  auto native = store->Load(rows->front().Key, kPackageMost);
  if (!native || !*native) { return std::nullopt; }
  const auto bytes = (**native).Bytes();
  return Seeded{.Record = rows->front(),
                .Bytes = {bytes.begin(), bytes.end()},
                .Request = Binding(directory / "assets.sqlite")};
}

void Exercise(const std::filesystem::path &directory) {
  Tasks compute(1);
  const auto store = Test::OpenAssetStore(directory);
  CHECK(store, "the isolated native asset store opens");
  if (!store) { return; }
  auto seed = Seed(directory, store, compute);
  CHECK(seed && !seed->Request.empty(), "the real producer seeds one native request binding");
  if (!seed || seed->Request.empty()) { return; }
  const auto base = OsmField::NativeTileDecodeBytes(seed->Bytes);
  const auto one = OsmField::NativeTileDecodeBytes(ExtendKeys(seed->Bytes, 1));
  CHECK(base && one && *one > *base && *base < kFormerHeapQuota,
        "the actual native size scanner determines the fixture's per-key plan growth");
  if (!base || !one || *one <= *base || *base >= kFormerHeapQuota) { return; }
  const auto count = static_cast<uint32_t>((kFormerHeapQuota - *base) / (*one - *base) + 1);
  const auto large = ExtendKeys(seed->Bytes, count);
  const auto planned = OsmField::NativeTileDecodeBytes(large);
  CHECK(planned && *planned > kFormerHeapQuota && large.size() < kPackageMost,
        "valid serialized input expands beyond the former arbitrary decoder quota");
  if (!planned || *planned <= kFormerHeapQuota || large.size() >= kPackageMost) { return; }
  seed->Record.Package.clear();
  seed->Record.OffsetBytes = 0;
  seed->Record.ByteCount = large.size();
  seed->Record.RequestKey = seed->Request;
  CHECK(store->Remove(seed->Record.Key), "the synthetic native replacement owns its isolated key");
  const std::array records{seed->Record};
  CHECK(store->Publish(records, large), "a valid native key table is stored without a heap quota");
  std::atomic_uint calls{0};
  Test::OsmCache::NoTransport transport;
  auto session = Test::OsmCache::OpenSession(
      std::make_unique<Test::OsmCache::VectorSource>("r1", std::vector<uint8_t>{}, calls));
  CHECK(session, "the fresh consumer's provider would return different data if accessed");
  if (!session) { return; }
  auto opened = PreparedOsmTiles::Open(store, *session, transport, compute);
  CHECK(opened, "the fresh native consumer has runtime admission");
  if (!opened) { return; }
  const std::array<std::string, 1> layers{"buildings"};
  const OsmField definition(2, layers, MvtSchema::OpenMapTiles);
  const auto product = Await(**opened, definition, compute);
  CHECK(product && product->KeyCount() == count + 1 && product->Features().size() == 1 &&
            product->Features().front().ProviderFeatureId == 7 &&
            product->Num(product->Features().front(), "render_height", 0) == 19,
        "the admitted decoder preserves every native key and the original building");
  const auto costs = (*opened)->Costs();
  CHECK(calls == 0 && costs.Hits == 1 && costs.Misses == 0 && costs.Writes == 0 &&
            costs.Generations == 0,
        "a large valid hit performs no source read, repair or regeneration");
  const auto binding = store->FindRequest(seed->Request);
  CHECK(binding && *binding && (**binding).Key == seed->Record.Key,
        "memory demand does not replace the valid stored request binding");
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-tests.native-osm-decoder-plan-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Exercise(directory);
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the generated fixture is removed after all native owners retire");
  return outshine::Test::Report();
}
