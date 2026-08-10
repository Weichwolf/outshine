#ifndef CAPACITY_H
#define CAPACITY_H

#include <cstddef>
#include <vector>

namespace outshine {

/* Capacity, not size: a pool that grew and shrank still holds the block, and a ledger that reported
 * the shrunk figure would not add up against the heap. */
template <class T>
size_t CapacityBytes(const std::vector<T> &v) {
  return v.capacity() * sizeof(T);
}

} // namespace outshine
#endif
