#include "AssetCache.h"
#include "AssetCacheState.h"
#include "AssetPackageCodec.h"
#include "Sha256.h"
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <print>
#include <ratio>
#include <span>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <zlib.h>

namespace {
using namespace outshine;
using Clock = std::chrono::steady_clock;
using Json = nlohmann::ordered_json;
using Report = std::expected<Json, const char *>;
constexpr size_t kBytesMost = size_t{256} * 1024 * 1024;

struct Input {
  AssetRecord Wanted;
  std::vector<AssetRecord> Records;
  std::vector<uint8_t> Bytes;
};

double Milliseconds(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

std::expected<std::vector<uint8_t>, const char *> StoredBytes(sqlite3 *database,
                                                              sqlite3_stmt *metadata) {
  const auto count = sqlite3_column_int64(metadata, 1);
  if (count <= 0 || std::cmp_greater(count, kBytesMost)) {
    return std::unexpected("input stored bytes exceed the declared comparison bound");
  }
  sqlite3_blob *raw = nullptr;
  const int opened = sqlite3_blob_open(
      database, "main", "packages", "bytes", sqlite3_column_int64(metadata, 0), 0, &raw);
  const std::unique_ptr<sqlite3_blob, decltype(&sqlite3_blob_close)> blob(raw, &sqlite3_blob_close);
  if (opened != SQLITE_OK || sqlite3_blob_bytes(blob.get()) != count) {
    return std::unexpected("input blob cannot be opened at its declared size");
  }
  std::vector<uint8_t> bytes(static_cast<size_t>(count));
  if (sqlite3_blob_read(blob.get(), bytes.data(), static_cast<int>(count), 0) != SQLITE_OK) {
    return std::unexpected("input blob read failed");
  }
  if (std::cmp_not_equal(sqlite3_column_int64(metadata, 2),
                         crc32_z(0, bytes.data(), bytes.size()))) {
    return std::unexpected("input package CRC mismatch");
  }
  return bytes;
}

std::expected<Input, const char *> ReadInput(const char *path, std::string_view key) {
  sqlite3 *raw = nullptr;
  const int opened =
      sqlite3_open_v2(path, &raw, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, &sqlite3_close);
  if (opened != SQLITE_OK) { return std::unexpected("read-only input database could not open"); }
  AssetSql::Statement wanted;
  const auto query = "SELECT " + std::string(AssetSql::kColumns) + " FROM assets a WHERE a.key=?";
  if (!AssetSql::Prepare(database.get(), query.c_str(), wanted) ||
      !AssetSql::Text(wanted.Value, 1, key) || sqlite3_step(wanted.Value) != SQLITE_ROW) {
    return std::unexpected("input record is missing");
  }
  auto record = AssetSql::Read(wanted.Value);
  if (!record) { return std::unexpected("input record is invalid"); }
  sqlite3_reset(wanted.Value);
  AssetSql::Statement metadata;
  if (!AssetSql::Prepare(
          database.get(),
          "SELECT rowid,length(bytes),crc,codec,native_bytes FROM packages WHERE key=?",
          metadata) ||
      !AssetSql::Text(metadata.Value, 1, record->Package) ||
      sqlite3_step(metadata.Value) != SQLITE_ROW) {
    return std::unexpected("input package metadata is missing");
  }
  const auto stored = sqlite3_column_int64(metadata.Value, 1);
  const auto codec = sqlite3_column_int64(metadata.Value, 3);
  const auto declared = sqlite3_column_int64(metadata.Value, 4);
  const auto native = codec == 0 && declared == 0 ? stored : declared;
  if (codec < 0 || codec > 1 || native <= 0 || std::cmp_greater(native, kBytesMost)) {
    return std::unexpected("input codec or native bytes exceed the declared comparison bound");
  }
  auto bytes = StoredBytes(database.get(), metadata.Value);
  if (!bytes) { return std::unexpected(bytes.error()); }
  auto decoded =
      AssetSql::Decompress(std::move(*bytes), static_cast<int>(codec), static_cast<size_t>(native));
  if (!decoded || Sha256Hex(decoded->data(), decoded->size()) != record->Package) {
    return std::unexpected("input native package digest mismatch");
  }
  Input input{.Wanted = std::move(*record), .Records = {}, .Bytes = std::move(*decoded)};
  AssetSql::Statement members;
  const auto select = "SELECT " + std::string(AssetSql::kColumns) +
                      " FROM assets a WHERE a.package=? ORDER BY a.key";
  if (!AssetSql::Prepare(database.get(), select.c_str(), members) ||
      !AssetSql::Text(members.Value, 1, input.Wanted.Package)) {
    return std::unexpected("input package member query failed");
  }
  int step = SQLITE_OK;
  while ((step = sqlite3_step(members.Value)) == SQLITE_ROW) {
    auto member = AssetSql::Read(members.Value);
    if (!member) { return std::unexpected("input package member is invalid"); }
    input.Records.push_back(std::move(*member));
  }
  if (step != SQLITE_DONE || input.Records.empty() ||
      input.Wanted.OffsetBytes > input.Bytes.size() ||
      input.Wanted.ByteCount > input.Bytes.size() - input.Wanted.OffsetBytes) {
    return std::unexpected("input package members or selected slice are incomplete");
  }
  return input;
}

Report Write(const char *source, std::string_view key, const char *destination) {
  auto input = ReadInput(source, key);
  if (!input) { return std::unexpected(input.error()); }
  std::error_code error;
  if (std::filesystem::exists(destination, error) || error) {
    return std::unexpected("cold destination must not exist");
  }
  const auto wanted = input->Wanted;
  const auto nativeBytes = input->Bytes.size();
  const auto members = input->Records.size();
  const auto slice =
      std::span(input->Bytes)
          .subspan(static_cast<size_t>(wanted.OffsetBytes), static_cast<size_t>(wanted.ByteCount));
  const auto digest = Sha256Hex(slice.data(), slice.size());
  const auto began = Clock::now();
  auto cache = AssetCache::Open(destination);
  const double openMs = Milliseconds(began);
  if (!cache) { return std::unexpected("cold destination could not open"); }
  const auto prepareAt = Clock::now();
  auto prepared = (*cache)->PreparePublication(std::move(input->Records), std::move(input->Bytes));
  const double prepareMs = Milliseconds(prepareAt);
  if (!prepared) { return std::unexpected("native publication preparation failed"); }
  sqlite3_int64 baseline = 0;
  sqlite3_int64 peak = 0;
  if (sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &baseline, &peak, 1) != SQLITE_OK ||
      baseline <= 0) {
    return std::unexpected("SQLite allocation counters unavailable");
  }
  const auto publishAt = Clock::now();
  const auto published = (*cache)->Publish(*prepared);
  const double publishMs = Milliseconds(publishAt);
  sqlite3_int64 current = 0;
  if (!published || sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &current, &peak, 0) != SQLITE_OK) {
    return std::unexpected("native publication failed");
  }
  const auto closeAt = Clock::now();
  cache->reset();
  const double closeMs = Milliseconds(closeAt);
  return Json{{"mode", "write"},
              {"key", wanted.Key},
              {"kind", wanted.Kind},
              {"package", wanted.Package},
              {"slice_sha256", digest},
              {"records", members},
              {"native_bytes", nativeBytes},
              {"stored_bytes", prepared->StoredBytes()},
              {"prepared_capacity_bytes", prepared->CapacityBytes()},
              {"sqlite_baseline_bytes", baseline},
              {"sqlite_peak_bytes", peak},
              {"sqlite_extra_bytes", peak - baseline},
              {"open_ms", openMs},
              {"prepare_ms", prepareMs},
              {"publish_ms", publishMs},
              {"close_ms", closeMs}};
}

Report Read(const char *path, std::string_view key, std::string_view expected) {
  const auto began = Clock::now();
  auto cache = AssetCache::Open(path);
  const double openMs = Milliseconds(began);
  if (!cache) { return std::unexpected("fresh offline cache could not open"); }
  const auto planAt = Clock::now();
  const auto plan = (*cache)->PlanRead(key, kBytesMost);
  const double planMs = Milliseconds(planAt);
  if (!plan || !*plan) { return std::unexpected("fresh offline package is not planned"); }
  const auto readAt = Clock::now();
  const auto loaded = (*cache)->Load(key, **plan);
  const double readMs = Milliseconds(readAt);
  if (!loaded || !*loaded) { return std::unexpected("fresh offline native hit is missing"); }
  const auto verifyAt = Clock::now();
  const auto bytes = (**loaded).Bytes();
  if (Sha256Hex(bytes.data(), bytes.size()) != expected) {
    return std::unexpected("fresh offline slice digest mismatch");
  }
  const double verifyMs = Milliseconds(verifyAt);
  const auto package = (**loaded).Record().Package;
  const auto closeAt = Clock::now();
  cache->reset();
  return Json{{"mode", "read"},
              {"key", key},
              {"package", package},
              {"slice_bytes", bytes.size()},
              {"slice_sha256", expected},
              {"package_bytes_most", (**plan).PackageBytesMost},
              {"buffer_bytes_most", (**plan).BufferBytesMost},
              {"open_ms", openMs},
              {"plan_ms", planMs},
              {"read_ms", readMs},
              {"verify_ms", verifyMs},
              {"close_ms", Milliseconds(closeAt)}};
}
}

int main(int argc, char *const *argv) {
  if (argc != 5) {
    std::println(stderr,
                 "usage: native-package-io write input.sqlite key new.sqlite | read cache.sqlite "
                 "key slice-sha256");
    return 2;
  }
  if (sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 1) != SQLITE_OK) {
    std::println(stderr, "native package IO failed: SQLite allocation accounting unavailable");
    return 1;
  }
  Report report = std::unexpected("unknown native package IO operation");
  const std::string_view mode(argv[1]);
  if (mode == "write") { report = Write(argv[2], argv[3], argv[4]); }
  if (mode == "read") { report = Read(argv[2], argv[3], argv[4]); }
  if (!report) {
    std::println(stderr, "native package IO failed: {}", report.error());
    return 1;
  }
  std::println("{}", report->dump());
  return 0;
}
