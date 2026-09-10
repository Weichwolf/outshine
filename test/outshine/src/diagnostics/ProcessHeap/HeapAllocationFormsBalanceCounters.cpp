#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include "src/base/io/Heap.h"
#include "Check.h"

void operator delete(void *, size_t) noexcept;
void operator delete[](void *, size_t) noexcept;
void operator delete(void *, size_t, std::align_val_t) noexcept;
void operator delete[](void *, size_t, std::align_val_t) noexcept;

namespace {
struct AllocationForm {
  const char *Name;
  void *(*Allocate)(size_t);
  void (*Release)(void *, size_t);
  size_t Alignment;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array forms{
      AllocationForm{.Name = "scalar/default/throwing/ordinary",
                     .Allocate = +[](size_t bytes) { return ::operator new(bytes); },
                     .Release = +[](void *block, size_t) { ::operator delete(block); },
                     .Alignment = alignof(std::max_align_t)},
      AllocationForm{.Name = "scalar/default/throwing/sized",
                     .Allocate = +[](size_t bytes) { return ::operator new(bytes); },
                     .Release = +[](void *block, size_t bytes) { ::operator delete(block, bytes); },
                     .Alignment = alignof(std::max_align_t)},
      AllocationForm{.Name = "scalar/default/nothrow/ordinary",
                     .Allocate = +[](size_t bytes) { return ::operator new(bytes, std::nothrow); },
                     .Release = +[](void *block, size_t) { ::operator delete(block); },
                     .Alignment = alignof(std::max_align_t)},
      AllocationForm{.Name = "scalar/default/nothrow/cleanup",
                     .Allocate = +[](size_t bytes) { return ::operator new(bytes, std::nothrow); },
                     .Release =
                         +[](void *block, size_t) { ::operator delete(block, std::nothrow); },
                     .Alignment = alignof(std::max_align_t)},
      AllocationForm{
          .Name = "scalar/aligned/throwing/ordinary",
          .Allocate = +[](size_t bytes) { return ::operator new(bytes, std::align_val_t{64}); },
          .Release = +[](void *block, size_t) { ::operator delete(block, std::align_val_t{64}); },
          .Alignment = 64},
      AllocationForm{
          .Name = "scalar/aligned/throwing/sized",
          .Allocate = +[](size_t bytes) { return ::operator new(bytes, std::align_val_t{64}); },
          .Release = +[](void *block,
                         size_t bytes) { ::operator delete(block, bytes, std::align_val_t{64}); },
          .Alignment = 64},
      AllocationForm{
          .Name = "scalar/aligned/nothrow/ordinary",
          .Allocate =
              +[](size_t bytes) {
                return ::operator new(bytes, std::align_val_t{64}, std::nothrow);
              },
          .Release = +[](void *block, size_t) { ::operator delete(block, std::align_val_t{64}); },
          .Alignment = 64},
      AllocationForm{.Name = "scalar/aligned/nothrow/cleanup",
                     .Allocate =
                         +[](size_t bytes) {
                           return ::operator new(bytes, std::align_val_t{64}, std::nothrow);
                         },
                     .Release =
                         +[](void *block, size_t) {
                           ::operator delete(block, std::align_val_t{64}, std::nothrow);
                         },
                     .Alignment = 64},
      AllocationForm{.Name = "array/default/throwing/ordinary",
                     .Allocate = +[](size_t bytes) { return ::operator new[](bytes); },
                     .Release = +[](void *block, size_t) { ::operator delete[](block); },
                     .Alignment = alignof(std::max_align_t)},
      AllocationForm{.Name = "array/default/throwing/sized",
                     .Allocate = +[](size_t bytes) { return ::operator new[](bytes); },
                     .Release =
                         +[](void *block, size_t bytes) { ::operator delete[](block, bytes); },
                     .Alignment = alignof(std::max_align_t)},
      AllocationForm{.Name = "array/default/nothrow/ordinary",
                     .Allocate =
                         +[](size_t bytes) { return ::operator new[](bytes, std::nothrow); },
                     .Release = +[](void *block, size_t) { ::operator delete[](block); },
                     .Alignment = alignof(std::max_align_t)},
      AllocationForm{
          .Name = "array/default/nothrow/cleanup",
          .Allocate = +[](size_t bytes) { return ::operator new[](bytes, std::nothrow); },
          .Release = +[](void *block, size_t) { ::operator delete[](block, std::nothrow); },
          .Alignment = alignof(std::max_align_t)},
      AllocationForm{
          .Name = "array/aligned/throwing/ordinary",
          .Allocate = +[](size_t bytes) { return ::operator new[](bytes, std::align_val_t{64}); },
          .Release = +[](void *block, size_t) { ::operator delete[](block, std::align_val_t{64}); },
          .Alignment = 64},
      AllocationForm{
          .Name = "array/aligned/throwing/sized",
          .Allocate = +[](size_t bytes) { return ::operator new[](bytes, std::align_val_t{64}); },
          .Release = +[](void *block,
                         size_t bytes) { ::operator delete[](block, bytes, std::align_val_t{64}); },
          .Alignment = 64},
      AllocationForm{
          .Name = "array/aligned/nothrow/ordinary",
          .Allocate =
              +[](size_t bytes) {
                return ::operator new[](bytes, std::align_val_t{64}, std::nothrow);
              },
          .Release = +[](void *block, size_t) { ::operator delete[](block, std::align_val_t{64}); },
          .Alignment = 64},
      AllocationForm{.Name = "array/aligned/nothrow/cleanup",
                     .Allocate =
                         +[](size_t bytes) {
                           return ::operator new[](bytes, std::align_val_t{64}, std::nothrow);
                         },
                     .Release =
                         +[](void *block, size_t) {
                           ::operator delete[](block, std::align_val_t{64}, std::nothrow);
                         },
                     .Alignment = 64}};
  for (const auto &form : forms) {
    for (const size_t bytes : {size_t{0}, size_t{1}, size_t{257}}) {
      const size_t before = Heap::LiveBytes();
      void *block = form.Allocate(bytes);
      const bool allocated = block != nullptr;
      const size_t during = Heap::LiveBytes();
      const bool aligned = reinterpret_cast<uintptr_t>(block) % form.Alignment == 0;
      form.Release(block, bytes);
      const size_t after = Heap::LiveBytes();
      CHECK(allocated && aligned, form.Name);
      CHECK(during > before && during - before >= bytes, "allocation adds its usable storage");
      CHECK(after == before, "matching deallocation restores the exact prior live-byte count");
      const size_t beforeNull = Heap::LiveBytes();
      form.Release(nullptr, bytes);
      CHECK(Heap::LiveBytes() == beforeNull, "null deallocation preserves accounting");
    }
  }
  CHECK(Heap::ProcessInstrumentationEnabled(),
        "explicit diagnostic module marks measurements available");
  return Report();
}
