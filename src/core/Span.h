#ifndef OUTSHINE_CORE_SPAN_H
#define OUTSHINE_CORE_SPAN_H

#include <cassert>
#include <cstddef>
#include <type_traits>

namespace outshine {

template <typename T>
class Span {
public:
  constexpr Span() = default;
  constexpr Span(T *data, size_t size) : Data_(data), Size_(size) {}
  template <size_t N>
  constexpr Span(T (&array)[N]) : Data_(array), Size_(N) {}
  template <typename U, typename = std::enable_if_t<std::is_same<const U, T>::value>>
  constexpr Span(const Span<U> &other) : Data_(other.Data()), Size_(other.Size()) {}

  constexpr T *Data() const { return Data_; }
  constexpr size_t Size() const { return Size_; }
  [[nodiscard]] constexpr bool Empty() const { return Size_ == 0; }
  constexpr size_t Bytes() const { return Size_ * sizeof(T); }

  T &operator[](size_t i) const {
    assert(i < Size_);
    return Data_[i];
  }
  constexpr T *begin() const { return Data_; }
  constexpr T *end() const { return Data_ + Size_; }

  Span Sub(size_t first, size_t count) const {
    assert(first <= Size_ && count <= Size_ - first);
    return Span(Data_ + first, count);
  }

private:
  T *Data_ = nullptr;
  size_t Size_ = 0;
};

}
#endif
