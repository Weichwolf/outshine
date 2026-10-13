#include "NormalUpload.h"
#include "math/Octahedral.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace outshine::Render {

void NormalUpload::Write(const void *carrying, float *into, uint32_t words) {
  const auto &source = static_cast<const NormalUpload *>(carrying)->Source;
  std::vector<float> scratch;
  const float *normals = source.From;
  if (normals == nullptr) {
    scratch.resize(size_t{words} * 3u);
    source.Writes(source.Carrying, scratch.data(), words * 3u);
    normals = scratch.data();
  }
  for (uint32_t at = 0; at < words; ++at) {
    const size_t first = size_t{at} * 3u;
    const uint32_t packed =
        PackedPair(OctFolded({{normals[first], normals[first + 1u], normals[first + 2u]}}));
    std::memcpy(into + at, &packed, sizeof(packed));
  }
}

}
