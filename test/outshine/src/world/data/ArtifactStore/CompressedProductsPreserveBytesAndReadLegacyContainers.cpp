#include "ArtifactStore.h"
#include "ArtifactBlocks.h"
#include "Sha256.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
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
                    ("outshine-compressed-artifacts-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  constexpr size_t blockBytes = 65536;
  constexpr size_t diskBytes = 4096;
  const ArtifactLimits limits{.BlockBytes = blockBytes, .EncodedBytesMost = 2 * blockBytes};
  ArtifactStore store({.Directory = root.string(), .CapBytes = diskBytes});
  std::vector<uint8_t> first(blockBytes, 17), second(blockBytes, 29);
  const auto firstKey = Sha256Hex(first.data(), first.size());
  const auto secondKey = Sha256Hex(second.data(), second.size());
  const std::string productKey(64, 'a');
  auto writer = store.Begin(productKey, limits);
  CHECK(writer && writer->Append(firstKey, first) && writer->Append(secondKey, second) &&
            writer->Publish() && store.Trim(),
        "lossless blocks allow a product larger than its physical disk budget");
  const auto path = root / (productKey + ".asset");
  CHECK(std::filesystem::file_size(path) <= diskBytes,
        "the complete container including metadata fits the declared disk quota");
  auto reader = store.Read(productKey, limits);
  CHECK(reader && reader->Manifest().Bytes == first.size() + second.size(),
        "decoded byte accounting remains independent of compressed disk occupancy");
  CHECK(reader && reader->ReadBlock(firstKey, first.size()) == first &&
            reader->ReadBlock(secondKey, second.size()) == second,
        "compression preserves every byte and block boundary");
  CHECK(reader && !reader->ReadBlock(secondKey, second.size()),
        "the manifest bounds the readable block sequence");
  CHECK(!store.Read(productKey, {.BlockBytes = blockBytes / 2, .EncodedBytesMost = 2 * blockBytes}),
        "compressed input cannot bypass decoded block limits");
  CHECK(!store.Read(productKey, {.BlockBytes = blockBytes, .EncodedBytesMost = blockBytes}),
        "compressed input cannot bypass the decoded product limit");
  {
    std::fstream corrupt(path, std::ios::binary | std::ios::in | std::ios::out);
    const std::array<char, 8> excessive{-1, -1, -1, -1, -1, -1, -1, -1};
    corrupt.write(excessive.data(), excessive.size());
  }
  auto excessive = store.Read(productKey, limits);
  CHECK(excessive && !excessive->ReadBlock(firstKey, first.size()),
        "an attacker-controlled stored length is rejected before allocating its buffer");

  const std::string legacyKey(64, 'b');
  const std::array<uint8_t, 4> legacyBytes{3, 1, 4, 1};
  const auto legacyBlock = Sha256Hex(legacyBytes.data(), legacyBytes.size());
  const ArtifactManifest legacyManifest{
      .Blocks = {{.Key = legacyBlock, .Bytes = legacyBytes.size()}}, .Bytes = legacyBytes.size()};
  const auto metadata = EncodeArtifactManifest(legacyManifest, legacyKey, limits);
  CHECK(metadata.has_value(), "legacy manifest is valid independently of the container writer");
  if (metadata) {
    std::ofstream legacy(root / (legacyKey + ".asset"), std::ios::binary);
    const auto put = [&](std::span<const uint8_t> bytes) {
      legacy.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    };
    put(legacyBytes);
    put(*metadata);
    std::array<uint8_t, 8> length{};
    for (size_t i = 0; i < length.size(); ++i) {
      length[i] = static_cast<uint8_t>(static_cast<uint64_t>(metadata->size()) >> (8 * i));
    }
    put(length);
    legacy.write("OSAFILE1", 8);
  }
  auto legacy = store.Read(legacyKey, limits);
  const std::vector<uint8_t> expected(legacyBytes.begin(), legacyBytes.end());
  CHECK(legacy && legacy->ReadBlock(legacyBlock, legacyBytes.size()) == expected,
        "pre-compression containers remain readable without cache deletion or rebaking");
  std::filesystem::remove_all(root);
  return Report();
}
