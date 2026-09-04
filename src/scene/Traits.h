#ifndef OUTSHINE_SCENE_TRAITS_H
#define OUTSHINE_SCENE_TRAITS_H

#include <array>
#include <scene/Scene.h>

#include <cstddef>
#include <cstdint>

namespace outshine {

struct Traits {
  static constexpr size_t kMost = 16;
  std::array<uint32_t, kMost> Keys = {{}};
  std::array<double, kMost> Values = {{}};
  size_t Count = 0;

  [[nodiscard]] const double *Named(uint32_t key) const {
    for (size_t at = 0; at < Count; ++at) {
      if (Keys[at] == key) { return &Values[at]; }
    }
    return nullptr;
  }

  struct Trait {
    uint32_t Key = 0;
    double Value = 0.0;
  };

  [[nodiscard]] bool Put(Trait held) {
    const uint32_t key = held.Key;
    const double value = held.Value;
    for (size_t at = 0; at < Count; ++at) {
      if (Keys[at] == key) {
        Values[at] = value;
        return true;
      }
    }
    if (Count == kMost) { return false; }
    Keys[Count] = key;
    Values[Count] = value;
    ++Count;
    return true;
  }
};

} // namespace outshine

#endif
