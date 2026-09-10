#include "ContentStore.h"

#include <algorithm>
#include <cstdint>
#include <atomic>
#include <cstdio>
#include <memory>
#include <span>
#include <limits>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <utility>

#include "Sha256.h"
#include "WriteFileAtomically.h"

namespace outshine::Data {
namespace {

constexpr size_t kDefaultCapBytes = 2ull << 30u;

constexpr const char *kDefaultLeaf = "outshine-content";

constexpr size_t kKeyCharacters = 64;

[[nodiscard]] bool ValidKey(std::string_view key) {
  return key.size() == kKeyCharacters && std::ranges::all_of(key, [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

[[nodiscard]] std::optional<std::vector<uint8_t>> ReadEntry(const std::string &path, size_t limit) {
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
      std::fgetc(file.get()) != EOF || std::ferror(file.get()) != 0) {
    return std::nullopt;
  }
  if (std::fclose(file.release()) != 0) { return std::nullopt; }
  return bytes;
}

[[nodiscard]] std::string DefaultDirectory() {
  std::error_code ec;
  const std::filesystem::path base = std::filesystem::temp_directory_path(ec);
  if (ec) { return {kDefaultLeaf}; }
  return (base / kDefaultLeaf).string();
}

}

std::string ContentKey(const SourceDecl &decl, const Address &at) {
  std::string subject = decl.Id;
  subject += '\n';
  subject += std::to_string(decl.Version);
  subject += '\n';
  subject += Name(decl.Kind);
  subject += '\n';
  subject += at.Text();
  return Sha256Hex(subject);
}

ContentStore::ContentStore(const Config &config)
    : Directory_(config.Directory.empty() ? DefaultDirectory() : config.Directory),
      Using_(config.Using),
      CapBytes_(config.CapBytes > 0 ? config.CapBytes : kDefaultCapBytes) {
  if (Using_ != Use::On) { return; }
  std::error_code ec;
  std::filesystem::create_directories(Directory_, ec);

  struct Entry {
    std::filesystem::path Path;
    std::filesystem::file_time_type When;
    uintmax_t Bytes = 0;
  };

  std::vector<Entry> entries;
  uintmax_t total = 0;
  for (std::filesystem::directory_iterator it(Directory_, ec), end; !ec && it != end;
       it.increment(ec)) {
    if (!ValidKey(it->path().filename().string())) { continue; }
    if (!std::filesystem::is_regular_file(it->symlink_status(ec)) || ec) { continue; }
    Entry e;
    e.Path = it->path();
    e.When = it->last_write_time(ec);
    if (ec) { break; }
    e.Bytes = it->file_size(ec);
    if (ec || e.Bytes > static_cast<uintmax_t>(std::numeric_limits<long long>::max()) - total) {
      return;
    }
    total += e.Bytes;
    entries.push_back(std::move(e));
  }
  if (total <= static_cast<uintmax_t>(CapBytes_)) { return; }
  std::ranges::sort(entries, [](const Entry &a, const Entry &b) { return a.When < b.When; });
  for (const Entry &e : entries) {
    if (total <= static_cast<uintmax_t>(CapBytes_)) { break; }
    std::error_code removeError;
    if (!std::filesystem::remove(e.Path, removeError)) { continue; }
    total -= e.Bytes;
    Swept_++;
    SweptBytes_ += static_cast<long long>(e.Bytes);
  }
}

std::optional<std::vector<uint8_t>> ContentStore::Read(std::string_view key,
                                                       size_t mostBytes) const {
  if (Using_ != Use::On) { return std::nullopt; }
  const size_t limit = mostBytes == 0 ? CapBytes_ : std::min(mostBytes, CapBytes_);
  auto kept = ValidKey(key) ? ReadEntry(Directory_ + "/" + std::string(key), limit) : std::nullopt;
  if (!kept) {
    Misses_.fetch_add(1, std::memory_order_relaxed);
    return std::nullopt;
  }
  Hits_.fetch_add(1, std::memory_order_relaxed);
  return kept;
}

bool ContentStore::Keep(std::string_view key, const uint8_t *data, size_t bytes) {
  if (Using_ != Use::On) { return false; }
  if (!ValidKey(key) || data == nullptr || bytes == 0 || bytes > CapBytes_) {
    WriteFailures_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  const auto written = WriteFileAtomically(Directory_ + "/" + std::string(key),
                                           std::as_bytes(std::span(data, bytes)));
  if (!written) {
    WriteFailures_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  Writes_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

ContentStore::Ledger ContentStore::Counters() const {
  Ledger out;
  out.Hits = Hits_.load(std::memory_order_relaxed);
  out.Misses = Misses_.load(std::memory_order_relaxed);
  out.Writes = Writes_.load(std::memory_order_relaxed);
  out.WriteFailures = WriteFailures_.load(std::memory_order_relaxed);
  out.Swept = Swept_;
  out.SweptBytes = SweptBytes_;
  return out;
}

}
