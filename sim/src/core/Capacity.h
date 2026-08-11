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

/* What a std::set or std::map costs beyond the values it holds. Both translations link libc++, whose
 * red-black node is three links and the colour ahead of the value; measured against the allocator it
 * is exact to the chunk header, which no column here models and the residual carries. */
template <class Value>
size_t TreeNodeBytes(size_t count) {
  struct Node {
    void *Left, *Right, *Parent;
    bool Black;
    Value Held;
  };
  return count * sizeof(Node);
}

} // namespace outshine
#endif
