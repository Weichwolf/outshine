#ifndef OUTSHINE_BASE_MEMORY_BYTELEASE_H
#define OUTSHINE_BASE_MEMORY_BYTELEASE_H
#include "MemoryBudget.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace outshine {
class ByteBlock {
public:
  ByteBlock(ByteBlock &&) noexcept = default;
  ByteBlock &operator=(ByteBlock &&other) noexcept;
  ByteBlock(const ByteBlock &) = delete;
  ByteBlock &operator=(const ByteBlock &) = delete;
  ~ByteBlock() = default;
  [[nodiscard]] static std::optional<ByteBlock>
  Adopt(std::vector<uint8_t> &bytes, MemoryBudget::Reservation &reservation) noexcept;

  [[nodiscard]] std::span<const uint8_t> Bytes() const noexcept { return Data_; }

  [[nodiscard]] size_t CapacityBytes() const noexcept { return Data_.capacity(); }

private:
  ByteBlock(std::vector<uint8_t> &&bytes, MemoryBudget::Reservation &&reservation) noexcept;
  MemoryBudget::Reservation Reservation_;
  std::vector<uint8_t> Data_;
};

class ByteLease {
public:
  ByteLease() = default;
  explicit ByteLease(std::shared_ptr<const ByteBlock> block) noexcept;
  // Failure preserves both inputs; successful adoption transfers ownership without copying.
  [[nodiscard]] static std::optional<ByteLease> Adopt(std::vector<uint8_t> &bytes,
                                                      MemoryBudget::Reservation &reservation);
  [[nodiscard]] std::span<const uint8_t> Bytes() const noexcept;
  [[nodiscard]] size_t CapacityBytes() const noexcept;

private:
  std::shared_ptr<const ByteBlock> Block_;
};
}
#endif
