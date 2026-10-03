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

[[nodiscard]] std::string DefaultDirectory() {
  std::error_code ec;
  const std::filesystem::path base = std::filesystem::temp_directory_path(ec);
  if (ec) { return {kDefaultLeaf}; }
  return (base / kDefaultLeaf).string();
}

}

[[nodiscard]] bool ContentStore::ValidKey(std::string_view key) {
  return key.size() == kKeyCharacters && std::ranges::all_of(key, [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

[[nodiscard]] std::optional<std::vector<uint8_t>> ContentStore::ReadEntry(const std::string &path,
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
      std::fgetc(file.get()) != EOF || std::ferror(file.get()) != 0) {
    return std::nullopt;
  }
  if (std::fclose(file.release()) != 0) { return std::nullopt; }
  return bytes;
}

std::string ContentKey(const SourceDecl &decl, const Address &at) {
  std::string subject = decl.Id;
  subject += '\n';
  subject += std::to_string(decl.Version);
  subject += '\n';
  subject += decl.Revision;
  subject += '\n';
  subject += Name(decl.Kind);
  subject += '\n';
  subject += at.Text();
  if (!decl.Endpoint.empty()) {
    subject += '\n';
    subject += decl.Endpoint;
  }
  if (!decl.PayloadSha256.empty()) {
    subject += '\n';
    subject += decl.PayloadSha256;
  }
  return Sha256Hex(subject);
}

std::string ContentKey(const SourceDecl &decl,
                       const Address &at,
                       const std::optional<ByteRange> &range,
                       std::string_view entityTag) {
  auto object = ContentKey(decl, at);
  if (!range) { return object; }
  std::string subject =
      object + "/r/" + std::to_string(range->First) + "/" + std::to_string(range->Length) + "/";
  subject += entityTag;
  return Sha256Hex(subject);
}

std::string SourceKey(const SourceDecl &decl) {
  std::string subject = decl.Id;
  subject += '\n';
  subject += std::to_string(decl.Version);
  subject += '\n';
  subject += decl.Revision;
  subject += '\n';
  subject += Name(decl.Kind);
  subject += '\n';
  subject += Name(decl.Wire);
  subject += '\n';
  subject += decl.Endpoint;
  if (!decl.PayloadSha256.empty()) {
    subject += '\n';
    subject += decl.PayloadSha256;
  }
  return Sha256Hex(subject);
}

ContentStore::ContentStore(const Config &config)
    : Directory_(config.Directory.empty() ? DefaultDirectory() : config.Directory),
      Using_(config.Using),
      CapBytes_(config.CapBytes > 0 ? config.CapBytes : kDefaultCapBytes),
      AbsenceEntries_(std::clamp(config.AbsenceEntries, size_t{1}, MaximumAbsenceEntries)),
      UtcSeconds_(config.UtcSeconds) {
  if (Using_ != Use::On) { return; }
  std::error_code ec;
  std::filesystem::create_directories(Directory_, ec);

  LoadAbsences();
}

bool ContentStore::Trim() {
  if (Using_ != Use::On) { return true; }
  std::error_code ec;

  struct StoredFile {
    std::filesystem::path Path;
    std::filesystem::file_time_type When;
    uintmax_t Bytes = 0;
  };

  std::vector<StoredFile> entries;
  uintmax_t total = 0;
  for (std::filesystem::directory_iterator it(Directory_, ec), end; !ec && it != end;
       it.increment(ec)) {
    if (!ValidKey(it->path().filename().string())) { continue; }
    if (!std::filesystem::is_regular_file(it->symlink_status(ec)) || ec) { continue; }
    StoredFile e;
    e.Path = it->path();
    e.When = it->last_write_time(ec);
    if (ec) { break; }
    e.Bytes = it->file_size(ec);
    if (ec || e.Bytes > static_cast<uintmax_t>(std::numeric_limits<long long>::max()) - total) {
      return false;
    }
    total += e.Bytes;
    entries.push_back(std::move(e));
  }
  if (ec) { return false; }
  if (total <= static_cast<uintmax_t>(CapBytes_)) { return true; }
  std::ranges::sort(entries,
                    [](const StoredFile &a, const StoredFile &b) { return a.When < b.When; });
  for (const StoredFile &e : entries) {
    if (total <= static_cast<uintmax_t>(CapBytes_)) { break; }
    std::error_code removeError;
    if (!std::filesystem::remove(e.Path, removeError)) { continue; }
    total -= e.Bytes;
    Swept_++;
    SweptBytes_ += static_cast<long long>(e.Bytes);
  }
  return total <= static_cast<uintmax_t>(CapBytes_);
}

std::optional<std::vector<uint8_t>> ContentStore::Read(std::string_view key,
                                                       size_t mostBytes) const {
  if (Using_ != Use::On) { return std::nullopt; }
  auto kept = ReadBytes(key, mostBytes);
  if (!kept) {
    Misses_.fetch_add(1, std::memory_order_relaxed);
    return std::nullopt;
  }
  Hits_.fetch_add(1, std::memory_order_relaxed);
  return kept;
}

std::optional<std::vector<uint8_t>> ContentStore::ReadBytes(std::string_view key,
                                                            size_t mostBytes) const {
  const size_t limit = mostBytes == 0 ? CapBytes_ : std::min(mostBytes, CapBytes_);
  return ValidKey(key) ? ReadEntry(Directory_ + "/" + std::string(key), limit) : std::nullopt;
}

ContentStore::Entry ContentStore::Lookup(std::string_view key, size_t mostBytes) const {
  if (Using_ != Use::On) { return {}; }
  if (auto bytes = ReadBytes(key, mostBytes)) {
    Hits_.fetch_add(1, std::memory_order_relaxed);
    return {.Where = Presence::Bytes, .Bytes = std::move(*bytes)};
  }
  if (ValidKey(key) && HasAbsence(key)) {
    Hits_.fetch_add(1, std::memory_order_relaxed);
    return {.Where = Presence::Absent, .Bytes = {}};
  }
  Misses_.fetch_add(1, std::memory_order_relaxed);
  return {};
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
  ForgetAbsence(key);
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
