#include "ArtifactStore.h"
#include "ArtifactBlocks.h"
#include "Sha256.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string_view>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-artifact-container-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  constexpr size_t budget = 700;
  ArtifactStore store({.Directory = root.string(), .CapBytes = budget});
  const ArtifactLimits limits{.BlockBytes = 8, .EncodedBytesMost = 16};
  const std::string a(64, 'a'), b(64, 'b'), c(64, 'c'), d(64, 'd');
  const std::array<uint8_t, 16> original{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  auto replacement = original;
  replacement.front() = 99;
  const auto append = [](ArtifactStore::Writer &writer, std::span<const uint8_t> bytes) {
    const std::string hash = Sha256Hex(bytes.data(), bytes.size());
    return writer.Append(hash, bytes);
  };
  const auto write = [&](const std::string &key, std::span<const uint8_t> bytes) {
    auto writer = store.Begin(key, limits);
    return writer && append(*writer, bytes.first(8)) && append(*writer, bytes.subspan(8)) &&
           writer->Publish();
  };
  const auto read = [](ArtifactStore::Reader &source) {
    ArtifactBlockReader blocks(source.Manifest(), [&](std::string_view key, size_t size) {
      return source.ReadBlock(key, size);
    });
    std::array<uint8_t, 16> bytes{};
    return blocks.Read(bytes) ? std::optional(bytes) : std::nullopt;
  };
  CHECK(write(a, original) && write(b, original), "complete checked products publish atomically");
  auto held = store.Read(a, limits);
  CHECK(held != nullptr, "reader holds the published file snapshot");
  const auto now = std::filesystem::file_time_type::clock::now();
  std::filesystem::last_write_time(root / (a + ".asset"), now - std::chrono::hours(3));
  std::filesystem::last_write_time(root / (b + ".asset"), now - std::chrono::hours(2));
  auto hot = store.Read(a, limits);
  CHECK(hot && read(*hot) == original, "cache access preserves bytes and refreshes recency");
  CHECK(write(c, original) && store.Trim(), "whole-product eviction enforces retained byte budget");
  CHECK(store.Read(a, limits) && !store.Read(b, limits) && store.Read(c, limits),
        "least recently used product is removed despite an earlier creation time for the hot one");
  CHECK(write(a, replacement), "atomic replacement can publish while an old reader exists");
  CHECK(held && read(*held) == original, "replacement cannot alter an already opened snapshot");
  auto updated = store.Read(a, limits);
  CHECK(updated && read(*updated) == replacement, "a new reader observes the replacement");
  auto retiring = store.Read(a, limits);
  std::filesystem::last_write_time(root / (a + ".asset"), now - std::chrono::hours(4));
  CHECK(write(b, original) && store.Trim() && !store.Read(a, limits),
        "eviction removes a complete product from future lookups");
  CHECK(retiring && read(*retiring) == replacement,
        "an open snapshot survives eviction until its reader releases it");
  {
    auto partial = store.Begin(d, limits);
    CHECK(partial && append(*partial, std::span(original).first(8)), "partial stream is writable");
    CHECK(!store.Read(d, limits), "unpublished data is never a cache hit");
  }
  bool temporary = false;
  size_t bytes = 0;
  for (const auto &entry : std::filesystem::directory_iterator(root)) {
    temporary = temporary || entry.path().extension() == ".tmp";
    if (entry.path().extension() == ".asset") { bytes += entry.file_size(); }
  }
  CHECK(!temporary && bytes <= budget, "aborted staging is removed without growing retained data");
  {
    std::fstream corrupt(root / (b + ".asset"), std::ios::binary | std::ios::in | std::ios::out);
    corrupt.put(static_cast<char>(100));
  }
  auto corrupt = store.Read(b, limits);
  CHECK(corrupt && !read(*corrupt),
        "block checksums reject payload corruption under a valid manifest");
  std::ofstream(root / (d + ".asset"), std::ios::binary) << "truncated";
  CHECK(!store.Read(d, limits), "truncated containers cannot become cache hits");
  ArtifactStore tiny({.Directory = (root / "tiny").string(), .CapBytes = 100});
  auto tooLarge = tiny.Begin(a, limits);
  CHECK(tooLarge && !append(*tooLarge, std::span(original).first(8)) && !tooLarge->Publish(),
        "container metadata and payload must both fit the product budget");
  tooLarge.reset();
  std::filesystem::remove_all(root);
  return Report();
}
