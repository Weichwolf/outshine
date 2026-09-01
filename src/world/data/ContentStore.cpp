#include "ContentStore.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <system_error>

#include "Sha256.h"

namespace outshine::Data {
namespace {

constexpr size_t kDefaultCapBytes = 2ull << 30;

constexpr const char *kDefaultLeaf = "outshine-content";

[[nodiscard]] std::string DefaultDirectory() {
  std::error_code ec;
  const std::filesystem::path base = std::filesystem::temp_directory_path(ec);
  if (ec) { return std::string(kDefaultLeaf); }
  return (base / kDefaultLeaf).string();
}

} // namespace

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
    if (!it->is_regular_file(ec)) { continue; }
    Entry e;
    e.Path = it->path();
    e.When = it->last_write_time(ec);
    e.Bytes = it->file_size(ec);
    total += e.Bytes;
    entries.push_back(std::move(e));
  }
  if (total <= static_cast<uintmax_t>(CapBytes_)) { return; }
  std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
    return a.When < b.When;
  });
  for (const Entry &e : entries) {
    if (total <= static_cast<uintmax_t>(CapBytes_)) { break; }
    std::error_code removeError;
    if (!std::filesystem::remove(e.Path, removeError)) { continue; }
    total -= e.Bytes;
    Swept_++;
    SweptBytes_ += static_cast<long long>(e.Bytes);
  }
}

bool ContentStore::TryRead(std::string_view key, std::vector<uint8_t> *out) const {
  if (Using_ != Use::On) { return false; }
  const std::string path = Directory_ + "/" + std::string(key);
  std::FILE *f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) {
    Misses_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  std::fseek(f, 0, SEEK_END);
  const long size = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  bool whole = size > 0;
  if (whole) {
    out->resize(static_cast<size_t>(size));
    whole = std::fread(out->data(), 1, static_cast<size_t>(size), f) == static_cast<size_t>(size);
  }
  std::fclose(f);
  if (!whole) {
    out->clear();
    Misses_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  Hits_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

void ContentStore::Keep(std::string_view key, const uint8_t *data, size_t bytes) {
  if (Using_ != Use::On || bytes == 0) { return; }

  const std::string temp = Directory_ + "/." + std::string(key) + "." +
                           std::to_string(TempSerial_.fetch_add(1, std::memory_order_relaxed));
  std::FILE *f = std::fopen(temp.c_str(), "wb");
  if (f == nullptr) {
    WriteFailures_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  const bool written = std::fwrite(data, 1, bytes, f) == bytes;
  const bool closed = std::fclose(f) == 0;
  if (!written || !closed) {
    std::remove(temp.c_str());
    WriteFailures_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  const std::string path = Directory_ + "/" + std::string(key);
  if (std::rename(temp.c_str(), path.c_str()) != 0) {
    std::remove(temp.c_str());
    WriteFailures_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  Writes_.fetch_add(1, std::memory_order_relaxed);
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

} // namespace outshine::Data
