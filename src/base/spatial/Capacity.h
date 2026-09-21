#ifndef OUTSHINE_BASE_SPATIAL_CAPACITY_H
#define OUTSHINE_BASE_SPATIAL_CAPACITY_H

#include <cstddef>
#include <vector>

namespace outshine {

template <class T> size_t CapacityBytes(const std::vector<T> &v) {
  return v.capacity() * sizeof(T);
}

}
#endif
