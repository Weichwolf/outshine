#ifndef OUTSHINE_RENDER_TERRAIN_PACKEDTERRAINCLASSIFICATION_H
#define OUTSHINE_RENDER_TERRAIN_PACKEDTERRAINCLASSIFICATION_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine {
class TerrainClassificationMap;
}

namespace outshine::Render {
class PackedTerrainClassification {
public:
  explicit PackedTerrainClassification(const TerrainClassificationMap &classes);

  [[nodiscard]] std::span<const uint32_t> Words() const noexcept { return Words_; }

  [[nodiscard]] uint64_t Digest() const noexcept { return Digest_; }

  [[nodiscard]] double PackMs() const noexcept { return PackMs_; }

  [[nodiscard]] size_t HeapBytes() const noexcept { return Words_.capacity() * sizeof(uint32_t); }

private:
  std::vector<uint32_t> Words_;
  uint64_t Digest_;
  double PackMs_;
};
}
#endif
