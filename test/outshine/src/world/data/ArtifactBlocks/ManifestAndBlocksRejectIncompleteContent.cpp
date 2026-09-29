#include "ArtifactBlocks.h"
#include "Sha256.h"
#include "Check.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  const ArtifactLimits limits{.BlockBytes = 4, .EncodedBytesMost = 11};
  std::map<std::string, std::vector<uint8_t>, std::less<>> blocks;
  ArtifactBlockWriter writer(limits, [&](std::string_view key, std::span<const uint8_t> bytes) {
    blocks[std::string(key)] = {bytes.begin(), bytes.end()};
    return true;
  });
  const std::vector<uint8_t> content{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  CHECK(writer.Append(std::span(content).first(4)) &&
            writer.Append(std::span(content).subspan(4, 4)) &&
            writer.Append(std::span(content).subspan(8)),
        "bounded blocks publish complete content");
  CHECK(!writer.Append(std::span(content).first(1)), "encoded byte budget rejects extra content");
  const auto key = Sha256Hex("test-producer-and-inputs");
  auto encoded = EncodeArtifactManifest(writer.Manifest(), key, limits);
  CHECK(encoded.has_value(), "complete manifest encodes");
  if (!encoded) { return Report(); }
  auto manifest = DecodeArtifactManifest(*encoded, key, limits);
  CHECK(manifest && manifest->Bytes == content.size() && manifest->Blocks.size() == 3,
        "manifest preserves total size and all block references");
  if (!manifest) { return Report(); }
  auto load = [&](std::string_view blockKey, size_t most) -> std::optional<std::vector<uint8_t>> {
    const auto found = blocks.find(blockKey);
    if (found == blocks.end() || found->second.size() > most) { return std::nullopt; }
    return found->second;
  };
  ArtifactBlockReader reader(*manifest, load);
  std::vector<uint8_t> decoded(content.size());
  CHECK(reader.Read(std::span(decoded).first(5)) && reader.Read(std::span(decoded).subspan(5)) &&
            decoded == content,
        "reads may cross block boundaries without losing bytes");
  uint8_t extra = 0;
  CHECK(!reader.Read(std::span(&extra, 1)), "read cannot run past the manifest");
  const auto middleKey = manifest->Blocks[1].Key;
  blocks[middleKey][0] ^= 1;
  ArtifactBlockReader corrupted(*manifest, load);
  CHECK(!corrupted.Read(decoded), "block checksum rejects changed bytes despite intact manifest");
  blocks.erase(middleKey);
  ArtifactBlockReader missing(*manifest, load);
  CHECK(!missing.Read(decoded), "missing block cannot become a complete product");
  CHECK(!DecodeArtifactManifest(*encoded, Sha256Hex("other-input"), limits),
        "input identity must match");
  (*encoded)[encoded->size() / 2] ^= 1;
  CHECK(!DecodeArtifactManifest(*encoded, key, limits), "manifest checksum detects corruption");
  ArtifactBlockWriter refused(limits,
                              [](std::string_view, std::span<const uint8_t>) { return false; });
  CHECK(!refused.Append(content) && refused.Manifest().Blocks.empty(),
        "oversized block does not enter the manifest");
  CHECK(!refused.Append(std::span(content).first(4)) && refused.Manifest().Blocks.empty(),
        "failed block publication does not enter the manifest");
  return Report();
}
