#include "RawSourceCache.h"
#include "CacheUsageIndex.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <span>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <utility>

#include "WriteFileAtomically.h"

namespace outshine::Data {
namespace {

constexpr double kNsPerMs = 1'000'000.0;
constexpr size_t kDefaultCapBytes = 2ull << 30u;

constexpr const char *kDefaultLeaf = "outshine-content";

constexpr size_t kKeyCharacters = 64;

[[nodiscard]] std::string DefaultDirectory() {
  std::error_code ec;
  const std::filesystem::path base = std::filesystem::temp_directory_path(ec);
  if (ec) { return {kDefaultLeaf}; }
  return (base / kDefaultLeaf).string();
}

}

[[nodiscard]] bool RawSourceCache::ValidKey(std::string_view key) {
  return key.size() == kKeyCharacters && std::ranges::all_of(key, [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

namespace {

[[nodiscard]] std::optional<std::vector<uint8_t>> ReadCacheFile(const std::string &path,
                                                                size_t limit) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path, error)) || error) {
    return std::nullopt;
  }
  std::unique_ptr<std::FILE, decltype(&std::fclose)> file(std::fopen(path.c_str(), "rb"),
                                                          &std::fclose);
  if (!file || std::fseek(file.get(), 0, SEEK_END) != 0) { return std::nullopt; }
  const long size = std::ftell(file.get());
  if (size <= 0 || std::cmp_greater(size, limit) || std::fseek(file.get(), 0, SEEK_SET) != 0) {
    return std::nullopt;
  }
  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  if (std::fread(bytes.data(), 1, bytes.size(), file.get()) != bytes.size() ||
      std::ferror(file.get()) != 0) {
    return std::nullopt;
  }
  if (std::feof(file.get()) == 0 && std::fgetc(file.get()) != EOF) { return std::nullopt; }
  if (std::ferror(file.get()) != 0) { return std::nullopt; }
  if (std::fclose(file.release()) != 0) { return std::nullopt; }
  return bytes;
}

}

std::optional<std::vector<uint8_t>> RawSourceCache::ReadEntry(const std::string &path,
                                                              size_t limit) const {
  const auto began = std::chrono::steady_clock::now();
  auto bytes = ReadCacheFile(path, limit);
  const auto ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - began)
          .count();
  ReadNs_.fetch_add(static_cast<uint64_t>(ns), std::memory_order_relaxed);
  ReadCalls_.fetch_add(1, std::memory_order_relaxed);
  if (bytes) { ReadBytes_.fetch_add(bytes->size(), std::memory_order_relaxed); }
  return bytes;
}

RawSourceCache::RawSourceCache(const Config &config)
    : Directory_(config.Directory),
      NeedsDefaultDirectory_(config.Directory.empty()),
      Using_(config.Using),
      CapBytes_(config.CapBytes > 0 ? config.CapBytes : kDefaultCapBytes) {}

RawSourceCache::~RawSourceCache() = default;

const std::string &RawSourceCache::Directory() const {
  if (NeedsDefaultDirectory_) {
    std::call_once(DirectoryInitialization_, [this] { Directory_ = DefaultDirectory(); });
  }
  return Directory_;
}

void RawSourceCache::InitializeStorage() const {
  if (Using_ != Use::On || StorageReady_.load(std::memory_order_acquire)) { return; }
  const std::scoped_lock lock(StorageMutex_);
  if (StorageReady_.load(std::memory_order_relaxed)) { return; }
  std::error_code ec;
  std::filesystem::create_directories(Directory(), ec);
  if (!ec) {
    auto usage =
        CacheUsageIndex::Open((std::filesystem::path(Directory()) / ".usage.sqlite").string());
    if (usage) { Usage_ = std::move(*usage); }
  }
  if (Usage_) {
    StorageReady_.store(true, std::memory_order_release);
  } else if (!UsageWarning_.exchange(true, std::memory_order_relaxed)) {
    std::fputs("source cache: usage index could not be opened\n", stderr);
  }
}

void RawSourceCache::ObserveUsage(std::string_view key, size_t bytes, bool published) const {
  InitializeStorage();
  if (StorageReady_.load(std::memory_order_acquire) && Usage_->Record(key, bytes, published)) {
    return;
  }
  if (!UsageWarning_.exchange(true, std::memory_order_relaxed)) {
    std::fputs("source cache: usage metadata could not be fully recorded\n", stderr);
  }
}

std::optional<std::vector<uint8_t>> RawSourceCache::ReadBytes(std::string_view key,
                                                              size_t mostBytes) const {
  const size_t limit = mostBytes == 0 ? CapBytes_ : std::min(mostBytes, CapBytes_);
  if (!ValidKey(key)) { return std::nullopt; }
  auto bytes = ReadEntry(Directory() + "/" + std::string(key), limit);
  if (bytes) { ObserveUsage(key, bytes->size()); }
  return bytes;
}

RawSourceCache::Entry RawSourceCache::Lookup(std::string_view key, size_t mostBytes) const {
  if (Using_ != Use::On) { return {}; }
  if (auto bytes = ReadBytes(key, mostBytes)) {
    Hits_.fetch_add(1, std::memory_order_relaxed);
    return {.Where = Presence::Bytes, .Bytes = std::move(*bytes)};
  }
  if (ValidKey(key)) {
    const auto status = ReadStatus(key);
    if (status != Presence::Unknown) {
      Hits_.fetch_add(1, std::memory_order_relaxed);
      return {.Where = status, .Bytes = {}};
    }
  }
  Misses_.fetch_add(1, std::memory_order_relaxed);
  return {};
}

bool RawSourceCache::Keep(std::string_view key, const uint8_t *data, size_t bytes) {
  if (Using_ != Use::On) { return false; }
  if (!ValidKey(key) || data == nullptr || bytes == 0 || bytes > CapBytes_) {
    WriteFailures_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  InitializeStorage();
  const auto written =
      WriteEntry(Directory() + "/" + std::string(key), std::as_bytes(std::span(data, bytes)));
  if (!written) {
    WriteFailures_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  ForgetStatus(key);
  ObserveUsage(key, bytes, true);
  Writes_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

bool RawSourceCache::WriteEntry(const std::string &path, std::span<const std::byte> bytes) {
  const auto began = std::chrono::steady_clock::now();
  const auto written = WriteFileAtomically(path, bytes);
  const auto ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - began)
          .count();
  WriteNs_.fetch_add(static_cast<uint64_t>(ns), std::memory_order_relaxed);
  if (written) { WriteBytes_.fetch_add(bytes.size(), std::memory_order_relaxed); }
  return written.has_value();
}

RawSourceCache::Ledger RawSourceCache::Counters() const {
  Ledger out;
  out.Hits = Hits_.load(std::memory_order_relaxed);
  out.Misses = Misses_.load(std::memory_order_relaxed);
  out.Writes = Writes_.load(std::memory_order_relaxed);
  out.WriteFailures = WriteFailures_.load(std::memory_order_relaxed);
  out.ReadCalls = ReadCalls_.load(std::memory_order_relaxed);
  out.ReadBytes = ReadBytes_.load(std::memory_order_relaxed);
  out.WriteBytes = WriteBytes_.load(std::memory_order_relaxed);
  out.ReadMs = static_cast<double>(ReadNs_.load(std::memory_order_relaxed)) / kNsPerMs;
  out.WriteMs = static_cast<double>(WriteNs_.load(std::memory_order_relaxed)) / kNsPerMs;
  return out;
}

}
