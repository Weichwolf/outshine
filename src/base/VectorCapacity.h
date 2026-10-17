#ifndef OUTSHINE_BASE_VECTORCAPACITY_H
#define OUTSHINE_BASE_VECTORCAPACITY_H

#include <algorithm>
#include <cstddef>
#include <vector>

namespace outshine {
template <class T> void GrowCapacity(std::vector<T> &into, size_t minimum) {
  if (minimum <= into.capacity()) { return; }
  const size_t growth = std::min(into.capacity(), into.max_size() - into.capacity());
  into.reserve(std::max(minimum, into.capacity() + growth));
}
}
#endif
