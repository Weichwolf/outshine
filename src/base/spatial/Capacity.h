#ifndef OUTSHINE_BASE_SPATIAL_CAPACITY_H
#define OUTSHINE_BASE_SPATIAL_CAPACITY_H

#include <cstddef>
#include <vector>

namespace outshine {

template <class T> size_t CapacityBytes(const std::vector<T> &v) {
  return v.capacity() * sizeof(T);
}

template <class Value> size_t TreeNodeBytes(size_t count) {
  struct Node {
    void *Left, *Right, *Parent;
    bool Black;
    Value Held;
  };

  return count * sizeof(Node);
}

} // namespace outshine
#endif
