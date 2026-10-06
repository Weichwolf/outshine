#ifndef OUTSHINE_BASE_FORMAT_BYTEARCHIVE_H
#define OUTSHINE_BASE_FORMAT_BYTEARCHIVE_H

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>
#include <utility>
#include <algorithm>

namespace outshine {

template <class T> [[nodiscard]] T LittleEndian(T value) noexcept {
  static_assert(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>);
  if constexpr (std::endian::native == std::endian::little || sizeof(T) == 1) {
    return value;
  } else {
    using Word = std::conditional_t<sizeof(T) == 8,
                                    uint64_t,
                                    std::conditional_t<sizeof(T) == 4, uint32_t, uint16_t>>;
    static_assert(sizeof(T) == sizeof(Word));
    return std::bit_cast<T>(std::byteswap(std::bit_cast<Word>(value)));
  }
}

class ByteWriter {
public:
  explicit ByteWriter(size_t most) : Most_(most) {}

  template <class T> bool Number(T value) {
    const auto encoded = LittleEndian(value);
    return Put({reinterpret_cast<const uint8_t *>(&encoded), sizeof(T)});
  }

  bool Put(std::span<const uint8_t> bytes) {
    if (bytes.size() > Most_ - Bytes_.size()) { return false; }
    Bytes_.insert(Bytes_.end(), bytes.begin(), bytes.end());
    return true;
  }

  void Reserve(size_t count) { Bytes_.reserve(std::min(count, Most_)); }

  [[nodiscard]] std::span<const uint8_t> Bytes() const noexcept { return Bytes_; }

  [[nodiscard]] std::vector<uint8_t> TakeBytes() && { return std::move(Bytes_); }

private:
  std::vector<uint8_t> Bytes_;
  size_t Most_;
};

class ByteReader {
public:
  explicit ByteReader(std::span<const uint8_t> bytes) : Bytes_(bytes) {}

  template <class T> bool Number(T &value) {
    const auto bytes = Take(sizeof(T));
    if (!bytes) { return false; }
    std::memcpy(&value, bytes->data(), sizeof(T));
    value = LittleEndian(value);
    return true;
  }

  [[nodiscard]] std::optional<std::span<const uint8_t>> Take(size_t count) {
    if (count > Bytes_.size()) { return std::nullopt; }
    const auto out = Bytes_.first(count);
    Bytes_ = Bytes_.subspan(count);
    return out;
  }

  [[nodiscard]] size_t Remaining() const noexcept { return Bytes_.size(); }

private:
  std::span<const uint8_t> Bytes_;
};

}
#endif
