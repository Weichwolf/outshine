#include "ArtifactStore.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <system_error>
#include <utility>

namespace outshine::Data {
namespace {
using File = std::unique_ptr<std::FILE, decltype(&std::fclose)>;
constexpr std::string_view kFooterMagic = "OSAFILE1";
constexpr size_t kFooterBytes = sizeof(uint64_t) + kFooterMagic.size();
constexpr size_t kAttempts = 64;

bool ValidKey(std::string_view key) {
  return key.size() == 64 && std::ranges::all_of(key, [](char value) {
           return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
         });
}

std::string Path(const std::string &directory, std::string_view key) {
  return directory + "/" + std::string(key) + ".asset";
}

bool Write(std::FILE *file, std::span<const uint8_t> bytes) {
  return std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size() && std::ferror(file) == 0;
}

void Touch(const std::string &path) {
  std::error_code error;
  std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now(), error);
}

std::optional<ArtifactManifest>
ReadManifest(std::FILE *file, std::string_view key, ArtifactLimits limits, size_t capBytes) {
  if (std::fseek(file, 0, SEEK_END) != 0) { return std::nullopt; }
  const long end = std::ftell(file);
  if (end < 0 || std::cmp_less(end, kFooterBytes) || std::cmp_greater(end, capBytes) ||
      std::fseek(file, -static_cast<long>(kFooterBytes), SEEK_END) != 0) {
    return std::nullopt;
  }
  std::array<uint8_t, kFooterBytes> footer{};
  if (std::fread(footer.data(), 1, footer.size(), file) != footer.size() ||
      !std::equal(kFooterMagic.begin(), kFooterMagic.end(), footer.begin() + sizeof(uint64_t))) {
    return std::nullopt;
  }
  uint64_t count = 0;
  for (size_t i = 0; i < sizeof(count); ++i) { count |= uint64_t{footer[i]} << (8 * i); }
  const auto available = static_cast<uint64_t>(end) - kFooterBytes;
  if (count > kArtifactManifestBytesMost || count > available ||
      std::fseek(file, static_cast<long>(available - count), SEEK_SET) != 0) {
    return std::nullopt;
  }
  std::vector<uint8_t> bytes(static_cast<size_t>(count));
  if (std::fread(bytes.data(), 1, bytes.size(), file) != bytes.size()) { return std::nullopt; }
  auto manifest = DecodeArtifactManifest(bytes, key, limits);
  if (!manifest || manifest->Bytes != available - count || std::fseek(file, 0, SEEK_SET) != 0) {
    return std::nullopt;
  }
  return manifest;
}
}

struct ArtifactStore::Reader::State {
  File Input{nullptr, &std::fclose};
  ArtifactManifest Manifest;
  size_t Next = 0;
};

ArtifactStore::Reader::Reader(std::unique_ptr<State> state) : State_(std::move(state)) {}

ArtifactStore::Reader::~Reader() = default;

const ArtifactManifest &ArtifactStore::Reader::Manifest() const noexcept {
  return State_->Manifest;
}

std::optional<std::vector<uint8_t>> ArtifactStore::Reader::ReadBlock(std::string_view key,
                                                                     size_t bytes) {
  if (State_->Next == State_->Manifest.Blocks.size()) { return std::nullopt; }
  const auto &block = State_->Manifest.Blocks[State_->Next];
  if (block.Key != key || block.Bytes != bytes) { return std::nullopt; }
  std::vector<uint8_t> result(bytes);
  if (std::fread(result.data(), 1, result.size(), State_->Input.get()) != result.size()) {
    return std::nullopt;
  }
  ++State_->Next;
  return result;
}

struct ArtifactStore::Writer::State {
  File Output{nullptr, &std::fclose};
  std::string Temporary;
  std::string Destination;
  std::string Key;
  ArtifactLimits Limits;
  ArtifactManifest Manifest;
  size_t CapBytes = 0;
  bool Failed = false;

  ~State() {
    Output.reset();
    if (!Temporary.empty()) {
      std::error_code error;
      std::filesystem::remove(Temporary, error);
    }
  }
};

ArtifactStore::Writer::Writer(std::unique_ptr<State> state) : State_(std::move(state)) {}

ArtifactStore::Writer::~Writer() = default;

bool ArtifactStore::Writer::Append(std::string_view key, std::span<const uint8_t> bytes) {
  auto &state = *State_;
  if (state.Failed || !state.Output) { return false; }
  const size_t count = state.Manifest.Blocks.size() + 1;
  const size_t metadata = 88 + count * 72 + 64 + kFooterBytes;
  if (!ValidKey(key) || bytes.empty() || bytes.size() > state.Limits.BlockBytes ||
      state.Manifest.Bytes > state.Limits.EncodedBytesMost ||
      bytes.size() > state.Limits.EncodedBytesMost - state.Manifest.Bytes ||
      metadata > state.CapBytes || state.Manifest.Bytes > state.CapBytes - metadata ||
      bytes.size() > state.CapBytes - metadata - state.Manifest.Bytes ||
      metadata - kFooterBytes > kArtifactManifestBytesMost || !Write(state.Output.get(), bytes)) {
    state.Failed = true;
    return false;
  }
  state.Manifest.Blocks.push_back({.Key = std::string(key), .Bytes = bytes.size()});
  state.Manifest.Bytes += bytes.size();
  return true;
}

bool ArtifactStore::Writer::Publish() {
  auto &state = *State_;
  if (state.Failed || !state.Output) { return false; }
  const auto manifest = EncodeArtifactManifest(state.Manifest, state.Key, state.Limits);
  if (!manifest) { return false; }
  std::array<uint8_t, kFooterBytes> footer{};
  const auto size = static_cast<uint64_t>(manifest->size());
  for (size_t i = 0; i < sizeof(size); ++i) { footer[i] = static_cast<uint8_t>(size >> (8 * i)); }
  std::ranges::copy(kFooterMagic, footer.begin() + sizeof(size));
  if (!Write(state.Output.get(), *manifest) || !Write(state.Output.get(), footer)) {
    state.Failed = true;
    return false;
  }
  if (std::fclose(state.Output.release()) != 0) { return false; }
  std::error_code error;
  std::filesystem::rename(state.Temporary, state.Destination, error);
  if (error) { return false; }
  state.Temporary.clear();
  return true;
}

ArtifactStore::ArtifactStore(Config config) : Config_(std::move(config)) {}

bool ArtifactStore::Enabled() const noexcept {
  return !Config_.Directory.empty() && Config_.CapBytes > 0;
}

const std::string &ArtifactStore::Directory() const noexcept {
  return Config_.Directory;
}

std::unique_ptr<ArtifactStore::Reader> ArtifactStore::Read(std::string_view key,
                                                           ArtifactLimits limits) const {
  if (!Enabled() || !ValidKey(key)) { return {}; }
  const std::string path = Path(Config_.Directory, key);
  std::error_code error;
  if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path, error)) || error) {
    return {};
  }
  File file(std::fopen(path.c_str(), "rb"), &std::fclose);
  if (!file) { return {}; }
  auto manifest = ReadManifest(file.get(), key, limits, Config_.CapBytes);
  if (!manifest) { return {}; }
  auto state = std::make_unique<Reader::State>();
  state->Input = std::move(file);
  state->Manifest = std::move(*manifest);
  Touch(path);
  return std::unique_ptr<Reader>(new Reader(std::move(state)));
}

std::unique_ptr<ArtifactStore::Writer> ArtifactStore::Begin(std::string_view key,
                                                            ArtifactLimits limits) {
  if (!Enabled() || !ValidKey(key) || limits.BlockBytes == 0 || limits.EncodedBytesMost == 0) {
    return {};
  }
  std::error_code error;
  std::filesystem::create_directories(Config_.Directory, error);
  if (error) { return {}; }
  auto state = std::make_unique<Writer::State>();
  state->Destination = Path(Config_.Directory, key);
  state->Key = key;
  state->Limits = limits;
  state->CapBytes = Config_.CapBytes;
  for (size_t attempt = 0; attempt < kAttempts; ++attempt) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string temporary = state->Destination + "." + std::to_string(stamp) + "." +
                                  std::to_string(Serial_.fetch_add(1)) + ".tmp";
    state->Output.reset(std::fopen(temporary.c_str(), "wbx"));
    if (state->Output) {
      state->Temporary = temporary;
      return std::unique_ptr<Writer>(new Writer(std::move(state)));
    }
    if (errno != EEXIST) { return {}; }
  }
  return {};
}

bool ArtifactStore::Trim() const {
  struct Entry {
    std::filesystem::path Path;
    std::filesystem::file_time_type Used;
    uintmax_t Bytes = 0;
  };

  std::error_code error;
  std::vector<Entry> entries;
  uintmax_t total = 0;
  for (std::filesystem::directory_iterator at(Config_.Directory, error), end; !error && at != end;
       at.increment(error)) {
    if (at->path().extension() != ".asset" || !ValidKey(at->path().stem().string()) ||
        !std::filesystem::is_regular_file(at->symlink_status(error)) || error) {
      continue;
    }
    const auto size = at->file_size(error);
    if (error || size > std::numeric_limits<uintmax_t>::max() - total) { return false; }
    const auto used = at->last_write_time(error);
    if (error) { return false; }
    entries.push_back({.Path = at->path(), .Used = used, .Bytes = size});
    total += size;
  }
  if (error) { return false; }
  std::ranges::sort(entries, [](const Entry &a, const Entry &b) { return a.Used < b.Used; });
  for (const auto &entry : entries) {
    if (total <= Config_.CapBytes) { break; }
    const auto used = std::filesystem::last_write_time(entry.Path, error);
    if (error) { return false; }
    if (used != entry.Used) { continue; }
    if (!std::filesystem::remove(entry.Path, error) || error) { return false; }
    total -= entry.Bytes;
  }
  return total <= Config_.CapBytes;
}
}
