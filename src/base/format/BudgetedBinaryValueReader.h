#ifndef OUTSHINE_BASE_FORMAT_BUDGETEDBINARYVALUEREADER_H
#define OUTSHINE_BASE_FORMAT_BUDGETEDBINARYVALUEREADER_H

#include "BinaryValueArchive.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine {
template <class Error> class BudgetedBinaryValueReader {
public:
  BudgetedBinaryValueReader(std::span<const uint8_t> bytes, size_t capacity)
      : Values_(bytes), Remaining_(capacity) {}

  template <class... T> bool operator()(T &...value) { return Values_(value...); }

  [[nodiscard]] size_t RemainingBytes() const noexcept { return Values_.In.Remaining(); }

  [[nodiscard]] std::optional<std::span<const uint8_t>> TakeBytes(size_t count) {
    return Values_.In.Take(count);
  }

  [[nodiscard]] Error Failure() const noexcept { return Error_; }

  [[nodiscard]] bool Charge(size_t bytes) noexcept {
    if (bytes > Remaining_) {
      Error_ = Error::CapacityExceeded;
      return false;
    }
    Remaining_ -= bytes;
    return true;
  }

  bool Text(std::string &text) {
    uint32_t count = 0;
    if (!Values_(count)) { return false; }
    const auto bytes = TakeBytes(count);
    if (!bytes || !Charge(static_cast<size_t>(count) + 1)) { return false; }
    text.assign(reinterpret_cast<const char *>(bytes->data()), bytes->size());
    return text.capacity() <= count || Charge(text.capacity() - count);
  }

  template <class T, class Read> bool List(std::vector<T> &items, size_t wireBytes, Read read) {
    uint32_t count = 0;
    if (!Values_(count) || count > RemainingBytes() / wireBytes) { return false; }
    if (count > Remaining_ / sizeof(T) || !Charge(static_cast<size_t>(count) * sizeof(T))) {
      Error_ = Error::CapacityExceeded;
      return false;
    }
    items.resize(count);
    return (items.capacity() == count || Charge((items.capacity() - count) * sizeof(T))) &&
           std::ranges::all_of(items, read);
  }

private:
  BinaryValueReader Values_;
  size_t Remaining_;
  Error Error_ = Error::Malformed;
};

}
#endif
