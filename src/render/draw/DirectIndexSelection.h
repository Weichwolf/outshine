#ifndef OUTSHINE_RENDER_DRAW_DIRECTINDEXSELECTION_H
#define OUTSHINE_RENDER_DRAW_DIRECTINDEXSELECTION_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace outshine::Render {

struct DirectIndexSelection {
  static constexpr size_t kWordCount = 512;
  static constexpr size_t kRootsMost = kWordCount * 32;
  std::array<uint32_t, kWordCount> Words{};
  bool Any = false;

  void Include(size_t root) noexcept {
    Words[root / 32] |= uint32_t{1} << (root % 32);
    Any = true;
  }

  [[nodiscard]] bool Includes(size_t root) const noexcept {
    return Any && root < kRootsMost && (Words[root / 32] & (uint32_t{1} << (root % 32))) != 0;
  }
};

}
#endif
