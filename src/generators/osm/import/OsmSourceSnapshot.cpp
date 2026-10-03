#include "OsmSourceSnapshot.h"

#include <cstddef>

namespace outshine::Generators::Osm {

size_t SourceSnapshot::StorageChargeBytes() const noexcept {
  size_t bytes = sizeof(SourceSnapshot) - sizeof(ElementSet) + Elements.StorageChargeBytes() +
                 Coverage.capacity() * sizeof(Data::SourceCoverage) +
                 Chunks.capacity() * sizeof(ChunkProvenance);
  for (const auto &chunk : Chunks) {
    bytes += chunk.Location.capacity() + chunk.PayloadSha256.capacity() + 2;
  }
  return bytes;
}

}
