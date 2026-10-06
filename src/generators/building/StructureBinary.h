#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTUREBINARY_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTUREBINARY_H

#include "StructureArtifact.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <optional>
#include <type_traits>
#include <vector>

namespace outshine::Generators::StructureBinary {
class Writer {
public:
  Writer() = default;

  Writer(const StructureArtifactSink &sink, size_t blockBytes)
      : Sink(&sink), BlockBytes(blockBytes) {}

  bool Flush() {
    if (Bytes.empty()) { return true; }
    if (Sink == nullptr || !(*Sink)(Bytes)) {
      Failure = StructureArtifactError::WriteFailed;
      return false;
    }
    Bytes.clear();
    return true;
  }

  template <typename T> bool Number(const T &value) {
    if constexpr (std::is_enum_v<T>) {
      return Number(static_cast<std::underlying_type_t<T>>(value));
    } else if constexpr (std::is_same_v<T, bool>) {
      return Number(static_cast<uint8_t>(value));
    } else {
      if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value)) {
          Failure = StructureArtifactError::InvalidScalar;
          return false;
        }
      }
      if (Sink != nullptr) {
        if (BlockBytes < sizeof(T)) { return false; }
        if (Bytes.size() > BlockBytes - sizeof(T) && !Flush()) { return false; }
      } else if (Bytes.size() > kStructureArtifactBytesMost - sizeof(T)) {
        return false;
      }
      using Word = std::conditional_t<sizeof(T) == 8,
                                      uint64_t,
                                      std::conditional_t<sizeof(T) == 4, uint32_t, uint8_t>>;
      static_assert(sizeof(Word) == sizeof(T));
      const auto bits = std::bit_cast<Word>(value);
      for (size_t i = 0; i < sizeof(T); ++i) {
        Bytes.push_back(static_cast<uint8_t>(bits >> (i * 8)));
      }
      return true;
    }
  }

  template <typename T, typename Visit> bool List(const std::vector<T> &items, Visit visit) {
    if (!Number(static_cast<uint64_t>(items.size()))) { return false; }
    return std::ranges::all_of(items, [&](const auto &item) { return visit(*this, item); });
  }

  template <typename T, typename Visit> bool Maybe(const std::optional<T> &item, Visit visit) {
    return Number(item.has_value()) && (!item || visit(*this, *item));
  }

  std::vector<uint8_t> Bytes;
  StructureArtifactError Failure = StructureArtifactError::CapacityExceeded;
  const StructureArtifactSink *Sink = nullptr;
  size_t BlockBytes = 0;
};

class Reader {
public:
  explicit Reader(std::span<const uint8_t> bytes) : Bytes(bytes), Remaining(bytes.size()) {}

  Reader(const StructureArtifactSource &source, StructureArtifactReadLimits limits)
      : Remaining(limits.EncodedBytes), AllocationLeft(limits.ResidentBytesMost), Source(&source) {}

  bool Get(std::span<uint8_t> into) {
    if (into.size() > Remaining) { return false; }
    if (Source != nullptr) {
      if (!(*Source)(into)) { return false; }
    } else {
      std::copy_n(Bytes.begin(), into.size(), into.begin());
      Bytes = Bytes.subspan(into.size());
    }
    Remaining -= into.size();
    return true;
  }

  template <typename T> bool Number(T &value) {
    if constexpr (std::is_enum_v<T>) {
      std::underlying_type_t<T> underlying{};
      if (!Number(underlying)) { return false; }
      value = static_cast<T>(underlying);
      return true;
    } else if constexpr (std::is_same_v<T, bool>) {
      uint8_t underlying = 0;
      if (!Number(underlying) || underlying > 1) { return false; }
      value = underlying != 0;
      return true;
    } else {
      std::array<uint8_t, sizeof(T)> encoded{};
      if (!Get(encoded)) { return false; }
      using Word = std::conditional_t<sizeof(T) == 8,
                                      uint64_t,
                                      std::conditional_t<sizeof(T) == 4, uint32_t, uint8_t>>;
      static_assert(sizeof(Word) == sizeof(T));
      Word bits = 0;
      for (size_t i = 0; i < sizeof(T); ++i) {
        bits |= static_cast<Word>(static_cast<Word>(encoded[i]) << (i * 8));
      }
      value = std::bit_cast<T>(bits);
      if constexpr (std::is_floating_point_v<T>) { return std::isfinite(value); }
      return true;
    }
  }

  template <typename T, typename Visit> bool List(std::vector<T> &items, Visit visit) {
    uint64_t count = 0;
    if (!Number(count) || count > Remaining || count > AllocationLeft / sizeof(T)) { return false; }
    AllocationLeft -= static_cast<size_t>(count) * sizeof(T);
    items.resize(static_cast<size_t>(count));
    for (auto &item : items) {
      if (!visit(*this, item)) { return false; }
    }
    return true;
  }

  template <typename T, typename Visit> bool Maybe(std::optional<T> &item, Visit visit) {
    bool present = false;
    if (!Number(present)) { return false; }
    if (!present) {
      item.reset();
      return true;
    }
    item.emplace();
    return visit(*this, *item);
  }

  std::span<const uint8_t> Bytes;
  size_t Remaining = 0;
  size_t AllocationLeft = kStructureArtifactBytesMost;
  const StructureArtifactSource *Source = nullptr;
};

}
#endif
