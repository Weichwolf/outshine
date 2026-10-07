#ifndef OUTSHINE_CONTENT_ASSETS_GEOMETRYASSETARCHIVE_H
#define OUTSHINE_CONTENT_ASSETS_GEOMETRYASSETARCHIVE_H

#include "ByteArchive.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace outshine::Content {
class GeometryAssetWriter {
public:
  explicit GeometryAssetWriter(size_t most) : Out(most) {}

  template <class... T> bool operator()(const T &...value) { return (Value(value) && ...); }

  template <class T> bool Value(const T &value) {
    if constexpr (std::is_enum_v<T>) {
      return Out.Number(static_cast<uint32_t>(value));
    } else if constexpr (std::is_same_v<T, bool>) {
      return Out.Number(static_cast<uint8_t>(value));
    } else {
      return Out.Number(value);
    }
  }

  template <class T, size_t N> bool Value(const std::array<T, N> &value) {
    return std::ranges::all_of(value, [this](const auto &one) { return this->Value(one); });
  }

  bool Text(std::string_view text) {
    return text.size() <= std::numeric_limits<uint32_t>::max() &&
           Out.Number(static_cast<uint32_t>(text.size())) &&
           Out.Put({reinterpret_cast<const uint8_t *>(text.data()), text.size()});
  }

  template <class T> bool Array(std::span<const T> values) {
    if (values.size() > std::numeric_limits<uint32_t>::max() ||
        !Out.Number(static_cast<uint32_t>(values.size()))) {
      return false;
    }
    if constexpr (std::endian::native == std::endian::little) {
      return Out.Put({reinterpret_cast<const uint8_t *>(values.data()), values.size_bytes()});
    } else {
      for (const T value : values) {
        if (!Out.Number(value)) { return false; }
      }
      return true;
    }
  }

  ByteWriter Out;
};

class GeometryAssetReader {
public:
  explicit GeometryAssetReader(std::span<const uint8_t> bytes) : In(bytes) {}

  template <class... T> bool operator()(T &...value) { return (Value(value) && ...); }

  template <class T> bool Value(T &value) {
    if constexpr (std::is_enum_v<T>) {
      uint32_t encoded = 0;
      if (!In.Number(encoded)) { return false; }
      using Underlying = std::underlying_type_t<T>;
      if (encoded > static_cast<uint64_t>(std::numeric_limits<Underlying>::max())) { return false; }
      value = static_cast<T>(encoded);
      return true;
    } else if constexpr (std::is_same_v<T, bool>) {
      uint8_t encoded = 0;
      if (!In.Number(encoded) || encoded > 1) { return false; }
      value = encoded != 0;
      return true;
    } else {
      return In.Number(value);
    }
  }

  template <class T, size_t N> bool Value(std::array<T, N> &value) {
    return std::ranges::all_of(value, [this](auto &one) { return this->Value(one); });
  }

  bool Text(std::string &text) {
    uint32_t length = 0;
    if (!In.Number(length)) { return false; }
    const auto bytes = In.Take(length);
    if (!bytes) { return false; }
    text.assign(reinterpret_cast<const char *>(bytes->data()), bytes->size());
    return true;
  }

  template <class T> bool Array(std::vector<T> &values) {
    uint32_t count = 0;
    if (!In.Number(count) || count > In.Remaining() / sizeof(T)) { return false; }
    values.resize(count);
    if constexpr (std::endian::native == std::endian::little) {
      const auto bytes = In.Take(static_cast<size_t>(count) * sizeof(T));
      if (count != 0) { std::memcpy(values.data(), bytes->data(), bytes->size()); }
    } else {
      for (auto &value : values) {
        if (!In.Number(value)) { return false; }
      }
    }
    return true;
  }

  ByteReader In;
};
}
#endif
