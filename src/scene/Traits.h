#ifndef OUTSHINE_SCENE_TRAITS_H
#define OUTSHINE_SCENE_TRAITS_H

#include <cstddef>
#include <cstdint>

namespace outshine {

struct Traits {
  static constexpr size_t kMost = 16;
  uint32_t Keys[kMost] = {};
  double Values[kMost] = {};
  size_t Count = 0;

  [[nodiscard]] const double *Named(uint32_t key) const {
    for (size_t at = 0; at < Count; ++at) {
      if (Keys[at] == key) { return &Values[at]; }
    }
    return nullptr;
  }

  [[nodiscard]] bool Put(uint32_t key, double value) {
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

}

#endif
