#ifndef OUTSHINE_TRAITS_H
#define OUTSHINE_TRAITS_H

#include <cstddef>
#include <cstdint>

namespace outshine {

// an instance's attributes, resolved ONCE at stand-up: the kind chain's defaults overridden
// by the instance's own, keys interned so a tick sees numbers and never a string
struct Traits {
  static constexpr size_t kMost = 16; // [SET] a game noun's attribute budget; the assembly
                                      // refuses the seventeenth rather than growing
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
