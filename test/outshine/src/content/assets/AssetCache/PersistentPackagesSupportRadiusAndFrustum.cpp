#include "content/AssetCache.h"
#include "Check.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <limits>
#include <sqlite3.h>

namespace {
using namespace outshine;
using namespace outshine::Test;

std::string Key(std::string_view name) {
  return Sha256Hex(name.data(), name.size());
}

std::vector<std::string> Keys(const std::vector<AssetRecord> &records) {
  std::vector<std::string> keys;
  for (const auto &record : records) { keys.push_back(record.Key); }
  std::ranges::sort(keys);
  return keys;
}

void SelectsPackages(AssetCache &index) {
  const Box all{.Min = {{-20, -20, -20}}, .Max = {{20, 20, 20}}};
  const std::array<uint8_t, 250> payload{};
  std::array<AssetRecord, 4> records = {{{.Key = Key("terrain"),
                                          .Kind = "terrain",
                                          .Bounds = {.Min = {{-4, -1, -4}}, .Max = {{4, 1, 4}}},
                                          .Package = {},
                                          .OffsetBytes = 0,
                                          .ByteCount = 100,
                                          .Level = 3},
                                         {.Key = Key("roads"),
                                          .Kind = "roads",
                                          .Bounds = {.Min = {{-2, 0, -2}}, .Max = {{2, 2, 2}}},
                                          .Package = {},
                                          .OffsetBytes = 100,
                                          .ByteCount = 50,
                                          .Level = 1},
                                         {.Key = Key("trees"),
                                          .Kind = "vegetation",
                                          .Bounds = {.Min = {{6, 0, 6}}, .Max = {{8, 15, 8}}},
                                          .Package = {},
                                          .OffsetBytes = 150,
                                          .ByteCount = 20,
                                          .Level = 1},
                                         {.Key = Key("houses"),
                                          .Kind = "buildings",
                                          .Bounds = {.Min = {{4, 0, 0}}, .Max = {{5, 20, 1}}},
                                          .Package = {},
                                          .OffsetBytes = 170,
                                          .ByteCount = 80,
                                          .Level = 0,
                                          .Parent = Key("cluster")}}};
  for (auto &record : records) { record.Package = Sha256Hex(payload.data(), payload.size()); }
  CHECK(index.Publish(records, payload).has_value(),
        "one package transaction indexes all native content kinds");
  const auto selected = index.Select({.Bounds = all});
  CHECK(selected && selected->size() == records.size(),
        "unfiltered query retains every package slice");
  const auto level = index.Select({.Bounds = all, .Level = 1});
  CHECK(level && level->size() == 2, "detail selection is independent of generator semantics");
  const auto radius =
      index.Select({.Bounds = all, .Radius = AssetRadius{.Centre = {}, .Metres = 4}});
  CHECK(radius && Keys(*radius) == Keys({records[0], records[1], records[3]}),
        "radius intersects boxes at their nearest point, including the boundary");
  const std::array<std::array<double, 4>, 1> planes = {{{1, 0, 0, -4.5}}};
  const auto frustum = index.Select({.Bounds = all, .Planes = planes});
  CHECK(frustum && Keys(*frustum) == Keys({records[2], records[3]}),
        "frustum keeps crossing and contained boxes while rejecting fully outside packages");
  const auto water = index.Select({.Bounds = all, .Kind = "water"});
  CHECK(water && water->empty(), "a valid empty demand is distinguishable from storage failure");
  const auto house = index.Find(records[3].Key);
  CHECK(house && *house == records[3], "lookup retains exact bounds, parent, byte range and level");
}

void ConservativeEarthBounds(AssetCache &index) {
  constexpr double earth = 6378137;
  constexpr double width = .01;
  AssetRecord record{.Key = Key("earth"),
                     .Kind = "terrain",
                     .Bounds = {.Min = {{earth, earth, earth}},
                                .Max = {{earth + width, earth + width, earth + width}}},
                     .Package = Key("")};
  CHECK(index.Publish(std::span(&record, 1), {}).has_value(),
        "small ECEF asset preserves double bounds");
  const Vec3 tip = record.Bounds.Max;
  const auto boundary = index.Select({.Bounds = {.Min = tip, .Max = tip}, .Kind = "terrain"});
  CHECK(boundary && boundary->size() == 1 && boundary->front() == record,
        "outward-rounded R*Tree bounds cannot lose a centimetre-sized boundary intersection");
  const Vec3 outside{{earth + width * 2, earth, earth}};
  const auto excluded = index.Select({.Bounds = {.Min = outside, .Max = outside}});
  CHECK(excluded && excluded->empty(), "double refinement removes float-rounded false positives");
  record.Bounds.Min[0] -= 100;
  record.Bounds.Max[0] -= 100;
  CHECK(index.Publish(std::span(&record, 1), {}).has_value(),
        "replacement updates one stable asset identity");
  const auto moved = index.Select({.Bounds = {.Min = tip, .Max = tip}});
  CHECK(moved && moved->empty(), "replacement removes obsolete spatial coverage atomically");
  CHECK(index.Remove(record.Key).has_value(),
        "eviction removes metadata and spatial bounds together");
  const auto removed = index.Find(record.Key);
  CHECK(removed && !*removed, "evicted product becomes a miss");
}

void RejectsInvalidInputs(AssetCache &index) {
  AssetRecord good{.Key = Key("valid"),
                   .Kind = "terrain",
                   .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                   .Package = Key("")};
  auto invalid = good;
  invalid.Key = Key("invalid");
  invalid.OffsetBytes = std::numeric_limits<uint64_t>::max();
  const std::array records{good, invalid};
  const auto refused = index.Publish(records, {});
  CHECK(!refused && refused.error() == AssetCacheError::InvalidInput,
        "overflowing byte ranges reject the entire publication");
  const auto absent = index.Find(good.Key);
  CHECK(absent && !*absent, "a rejected batch cannot publish a valid prefix");
  CHECK(!index.Find("bad-key"),
        "malformed identities are invalid input rather than ordinary misses");
  CHECK(!index.Select({.Bounds = {}}), "empty unbounded boxes are refused at the IO boundary");
  const auto radius =
      index.Select({.Bounds = good.Bounds, .Radius = AssetRadius{.Centre = {}, .Metres = -1}});
  CHECK(!radius && radius.error() == AssetCacheError::InvalidInput,
        "negative radius cannot select assets");
  good.Bounds.Min[1] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!index.Publish(std::span(&good, 1), {}),
        "nonfinite bounds never enter persistent metadata");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-asset-cache-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const auto path = (root / "index.sqlite").string();
  auto opened = AssetCache::Open(path);
  CHECK(opened.has_value(), "persistent native package index opens");
  if (!opened) { return Report(); }
  auto index = std::move(*opened);
  SelectsPackages(*index);
  ConservativeEarthBounds(*index);
  RejectsInvalidInputs(*index);
  index.reset();
  auto restarted = AssetCache::Open(path);
  CHECK(restarted && (*restarted)->Find(Key("houses"))->has_value(),
        "a fresh connection finds prepared package metadata without any provider or generator");
  sqlite3 *locked = nullptr;
  CHECK(sqlite3_open(path.c_str(), &locked) == SQLITE_OK &&
            sqlite3_exec(locked, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "independent writer can hold the publication boundary");
  const auto busy = (*restarted)->Remove(Key("houses"));
  CHECK(!busy && busy.error() == AssetCacheError::Busy,
        "contention returns explicitly without blocking retry");
  CHECK((*restarted)->Find(Key("houses"))->has_value(),
        "busy publication preserves existing assets");
  sqlite3_exec(locked, "ROLLBACK; PRAGMA user_version=2", nullptr, nullptr, nullptr);
  sqlite3_close(locked);
  restarted->reset();
  const auto newer = AssetCache::Open(path);
  CHECK(!newer && newer.error() == AssetCacheError::UnsupportedVersion,
        "unknown index formats are reported without silently deleting existing metadata");
  std::filesystem::remove_all(root);
  return Report();
}
