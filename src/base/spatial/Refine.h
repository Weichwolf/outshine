#ifndef OUTSHINE_BASE_SPATIAL_REFINE_H
#define OUTSHINE_BASE_SPATIAL_REFINE_H

#include <cstdint>
#include <span>
#include <vector>

namespace outshine {

inline constexpr uint32_t kNoVertex = 0xffffffffu;

[[nodiscard]] inline uint64_t EdgeKey(uint32_t a, uint32_t b) {
  return a < b ? (static_cast<uint64_t>(a) << 32U) | b : (static_cast<uint64_t>(b) << 32U) | a;
}

inline void Divide(std::span<const uint32_t, 3> face,
                   std::span<const uint32_t, 3> cut,
                   std::vector<uint32_t> &into) {
  const auto lay = [&into](uint32_t a, uint32_t b, uint32_t c) {
    into.push_back(a);
    into.push_back(b);
    into.push_back(c);
  };
  const int cuts =
      (cut[0] != kNoVertex ? 1 : 0) + (cut[1] != kNoVertex ? 1 : 0) + (cut[2] != kNoVertex ? 1 : 0);
  if (cuts == 0) {
    lay(face[0], face[1], face[2]);
    return;
  }
  if (cuts == 3) {
    lay(face[0], cut[0], cut[2]);
    lay(cut[0], face[1], cut[1]);
    lay(cut[2], cut[1], face[2]);
    lay(cut[0], cut[1], cut[2]);
    return;
  }
  if (cuts == 1) {
    for (int edge = 0; edge < 3; ++edge) {
      if (cut[edge] == kNoVertex) { continue; }
      lay(face[edge], cut[edge], face[(edge + 2) % 3]);
      lay(cut[edge], face[(edge + 1) % 3], face[(edge + 2) % 3]);
    }
    return;
  }
  for (int edge = 0; edge < 3; ++edge) {
    if (cut[edge] != kNoVertex) { continue; }
    lay(face[edge], face[(edge + 1) % 3], cut[(edge + 1) % 3]);
    lay(face[edge], cut[(edge + 1) % 3], cut[(edge + 2) % 3]);
    lay(cut[(edge + 2) % 3], cut[(edge + 1) % 3], face[(edge + 2) % 3]);
    return;
  }
}

} // namespace outshine
#endif
