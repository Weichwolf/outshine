#include "OsmSourceSnapshot.h"

#include <cstddef>

namespace outshine::Data {

size_t OsmSourceSnapshot::StorageChargeBytes() const noexcept {
  size_t bytes = sizeof(OsmSourceSnapshot) - sizeof(OsmElements) + Elements.StorageChargeBytes() +
                 Coverage.capacity() * sizeof(SourceCoverage) +
                 Chunks.capacity() * sizeof(OsmChunkProvenance);
  for (const auto &chunk : Chunks) {
    bytes += chunk.Location.capacity() + chunk.PayloadSha256.capacity() + 2;
  }
  return bytes;
}

}
