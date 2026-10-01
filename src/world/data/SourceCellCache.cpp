#include "ContentStore.h"

#include "Sha256.h"
#include "WriteFileAtomically.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace outshine::Data {
namespace {
constexpr std::string_view kCellReceipt = "outshine-source-cell-v1\n";
constexpr size_t kSha256HexChars = 64;
constexpr size_t kReceiptMost = kCellReceipt.size() + 2 * (kSha256HexChars + 1);

std::string
CellReceiptDirectory(const ContentStore &store, const SourceDecl &decl, GeoCellId cell) {
  std::string path = store.Directory() + "/cells/" + SourceKey(decl);
  for (int level = cell.Level - 1; level >= 0; --level) {
    const auto bit = static_cast<unsigned>(level);
    path += '/';
    path += static_cast<char>('0' + (((cell.X >> bit) & 1u) << 1u) + ((cell.Y >> bit) & 1u));
  }
  return path;
}

std::array<GeoCellId, 4> Children(GeoCellId cell) {
  return {{{.Level = cell.Level + 1, .X = cell.X * 2, .Y = cell.Y * 2},
           {.Level = cell.Level + 1, .X = cell.X * 2, .Y = cell.Y * 2 + 1},
           {.Level = cell.Level + 1, .X = cell.X * 2 + 1, .Y = cell.Y * 2},
           {.Level = cell.Level + 1, .X = cell.X * 2 + 1, .Y = cell.Y * 2 + 1}}};
}

bool CacheableOsmCell(const SourceDecl &decl, GeoCellId cell) {
  return cell.Valid() && decl.Kind == DataKind::OriginalOsm && decl.How == Scheme::GeodeticGrid &&
         decl.Keeps == Cacheability::Forever;
}

bool EnsureCellDirectory(const ContentStore &store, const std::string &path, bool create) {
  auto current = std::filesystem::path(store.Directory());
  const auto relative = std::filesystem::path(path).lexically_relative(current);
  for (const auto &part : relative) {
    current /= part;
    std::error_code error;
    if (create) { (void)std::filesystem::create_directory(current, error); }
    if (error || !std::filesystem::is_directory(std::filesystem::symlink_status(current, error)) ||
        error) {
      return false;
    }
  }
  return true;
}
}

bool ContentStore::WriteCellReceipt(const SourceDecl &decl,
                                    GeoCellId cell,
                                    std::span<const uint8_t> bytes) {
  const std::string path = CellReceiptDirectory(*this, decl, cell);
  if (!EnsureCellDirectory(*this, path, true)) {
    WriteFailures_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  const std::string receipt = std::string(kCellReceipt) +
                              ContentKey(decl, Address::AtGeoCell(cell)) + '\n' +
                              Sha256Hex(bytes.data(), bytes.size()) + '\n';
  if (!WriteFileAtomically(path + "/receipt", std::as_bytes(std::span(receipt)))) {
    WriteFailures_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  return true;
}

bool ContentStore::KeepCell(const SourceDecl &decl,
                            GeoCellId cell,
                            std::span<const uint8_t> bytes) {
  if (!CacheableOsmCell(decl, cell) || !Enabled()) { return false; }
  return Keep(ContentKey(decl, Address::AtGeoCell(cell)), bytes.data(), bytes.size()) &&
         WriteCellReceipt(decl, cell, bytes);
}

std::optional<std::vector<uint8_t>> ContentStore::ReadVerifiedCellBytes(const SourceDecl &decl,
                                                                        GeoCellId cell) const {
  const auto path = CellReceiptDirectory(*this, decl, cell);
  if (!EnsureCellDirectory(*this, path, false)) { return std::nullopt; }
  const auto receipt = ReadEntry(path + "/receipt", kReceiptMost);
  if (!receipt) { return std::nullopt; }
  const std::string_view record(reinterpret_cast<const char *>(receipt->data()), receipt->size());
  const std::string key = ContentKey(decl, Address::AtGeoCell(cell));
  const std::string prefix = std::string(kCellReceipt) + key + '\n';
  if (!record.starts_with(prefix) || record.size() != prefix.size() + kSha256HexChars + 1 ||
      record.back() != '\n') {
    return std::nullopt;
  }
  const auto digest = record.substr(prefix.size(), kSha256HexChars);
  if (!ValidKey(digest)) { return std::nullopt; }
  auto bytes = ReadBytes(key, decl.MaximumPayloadBytes);
  if (!bytes || Sha256Hex(bytes->data(), bytes->size()) != digest) { return std::nullopt; }
  return bytes;
}

ContentStore::Entry ContentStore::LookupCell(const SourceDecl &decl, GeoCellId cell) {
  if (!CacheableOsmCell(decl, cell) || !Enabled()) { return {}; }
  if (auto bytes = ReadVerifiedCellBytes(decl, cell)) {
    Hits_.fetch_add(1, std::memory_order_relaxed);
    return {.Where = Presence::Bytes, .Bytes = std::move(*bytes)};
  }
  std::error_code error;
  const auto receipt =
      std::filesystem::symlink_status(CellReceiptDirectory(*this, decl, cell) + "/receipt", error);
  if (std::filesystem::exists(receipt)) {
    Misses_.fetch_add(1, std::memory_order_relaxed);
    return {};
  }
  auto kept = Lookup(ContentKey(decl, Address::AtGeoCell(cell)), decl.MaximumPayloadBytes);
  if (kept.Where == Presence::Bytes) { (void)WriteCellReceipt(decl, cell, kept.Bytes); }
  return kept;
}

bool ContentStore::HasCompleteChildCoverage(const SourceDecl &decl,
                                            GeoCellId cell,
                                            size_t probesMost) const {
  if (!CacheableOsmCell(decl, cell) || !Enabled() || cell.Level == GeoCellId::MaximumLevel ||
      ReadBytes(ContentKey(decl, Address::AtGeoCell(cell)), decl.MaximumPayloadBytes)) {
    return false;
  }
  const auto covered = [&](const auto &self, GeoCellId leaf) -> bool {
    if (probesMost == 0) { return false; }
    --probesMost;
    std::error_code error;
    if (!std::filesystem::is_directory(
            std::filesystem::symlink_status(CellReceiptDirectory(*this, decl, leaf), error))) {
      return false;
    }
    if (ReadVerifiedCellBytes(decl, leaf)) { return true; }
    if (leaf.Level == GeoCellId::MaximumLevel) { return false; }
    return std::ranges::all_of(Children(leaf), [&](const auto child) { return self(self, child); });
  };
  return std::ranges::all_of(Children(cell),
                             [&](const auto child) { return covered(covered, child); });
}
}
