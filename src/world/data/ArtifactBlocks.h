#ifndef OUTSHINE_WORLD_DATA_ARTIFACTBLOCKS_H
#define OUTSHINE_WORLD_DATA_ARTIFACTBLOCKS_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Data {

inline constexpr size_t kArtifactManifestBytesMost = 1024 * 1024;

struct ArtifactLimits {
  size_t BlockBytes = 4 * 1024 * 1024;
  size_t EncodedBytesMost = 0;
};

struct ArtifactBlock {
  std::string Key;
  uint64_t Bytes = 0;
};

struct ArtifactManifest {
  std::vector<ArtifactBlock> Blocks;
  uint64_t Bytes = 0;
};

[[nodiscard]] std::optional<std::vector<uint8_t>> EncodeArtifactManifest(
    const ArtifactManifest &manifest, std::string_view key, ArtifactLimits limits);
[[nodiscard]] std::optional<ArtifactManifest>
DecodeArtifactManifest(std::span<const uint8_t> bytes, std::string_view key, ArtifactLimits limits);

class ArtifactBlockWriter {
public:
  using Publish = std::function<bool(std::string_view, std::span<const uint8_t>)>;
  ArtifactBlockWriter(ArtifactLimits limits, Publish publish);
  [[nodiscard]] bool Append(std::span<const uint8_t> bytes);

  [[nodiscard]] const ArtifactManifest &Manifest() const noexcept { return Manifest_; }

private:
  ArtifactLimits Limits_;
  Publish Publish_;
  ArtifactManifest Manifest_;
};

class ArtifactBlockReader {
public:
  using Load = std::function<std::optional<std::vector<uint8_t>>(std::string_view, size_t)>;
  ArtifactBlockReader(ArtifactManifest manifest, Load load);
  [[nodiscard]] bool Read(std::span<uint8_t> into);

private:
  ArtifactManifest Manifest_;
  Load Load_;
  std::vector<uint8_t> Block_;
  size_t Next_ = 0;
  size_t Offset_ = 0;
};

}
#endif
