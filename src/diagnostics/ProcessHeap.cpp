#include "Heap.h"
#include <new>
#include <cstddef>

void *operator new(size_t bytes) {
  outshine::Heap::EnableProcessInstrumentation();
  return outshine::Heap::Take("object", bytes);
}

void *operator new[](size_t bytes) {
  outshine::Heap::EnableProcessInstrumentation();
  return outshine::Heap::Take("object array", bytes);
}

void *operator new(size_t bytes, std::align_val_t alignment) {
  outshine::Heap::EnableProcessInstrumentation();
  return outshine::Heap::TakeAligned("object", bytes, static_cast<size_t>(alignment));
}

void *operator new[](size_t bytes, std::align_val_t alignment) {
  outshine::Heap::EnableProcessInstrumentation();
  return outshine::Heap::TakeAligned("object array", bytes, static_cast<size_t>(alignment));
}

void *operator new(size_t bytes, [[maybe_unused]] const std::nothrow_t &neverThrows) noexcept {
  outshine::Heap::EnableProcessInstrumentation();
  return outshine::Heap::TryTake(bytes);
}

void *operator new[](size_t bytes, [[maybe_unused]] const std::nothrow_t &neverThrows) noexcept {
  outshine::Heap::EnableProcessInstrumentation();
  return outshine::Heap::TryTake(bytes);
}

void operator delete(void *block) noexcept {
  outshine::Heap::Return(block);
}

void operator delete[](void *block) noexcept {
  outshine::Heap::Return(block);
}

void operator delete(void *block, [[maybe_unused]] size_t bytes) noexcept {
  outshine::Heap::Return(block);
}

void operator delete[](void *block, [[maybe_unused]] size_t bytes) noexcept {
  outshine::Heap::Return(block);
}

void operator delete(void *block, [[maybe_unused]] std::align_val_t alignment) noexcept {
  outshine::Heap::Return(block);
}

void operator delete[](void *block, [[maybe_unused]] std::align_val_t alignment) noexcept {
  outshine::Heap::Return(block);
}

void operator delete(void *block,
                     [[maybe_unused]] size_t bytes,
                     [[maybe_unused]] std::align_val_t alignment) noexcept {
  outshine::Heap::Return(block);
}

void operator delete[](void *block,
                       [[maybe_unused]] size_t bytes,
                       [[maybe_unused]] std::align_val_t alignment) noexcept {
  outshine::Heap::Return(block);
}

void operator delete(void *block, [[maybe_unused]] const std::nothrow_t &neverThrows) noexcept {
  outshine::Heap::Return(block);
}

void operator delete[](void *block, [[maybe_unused]] const std::nothrow_t &neverThrows) noexcept {
  outshine::Heap::Return(block);
}
