#include "ByteLease.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine {
ByteBlock::ByteBlock(std::vector<uint8_t> &&bytes, MemoryBudget::Reservation &&reservation) noexcept
    : Reservation_(std::move(reservation)), Data_(std::move(bytes)) {}

ByteBlock &ByteBlock::operator=(ByteBlock &&other) noexcept {
  if (this != &other) {
    Data_ = std::move(other.Data_);
    Reservation_ = std::move(other.Reservation_);
  }
  return *this;
}

std::optional<ByteBlock> ByteBlock::Adopt(std::vector<uint8_t> &bytes,
                                          MemoryBudget::Reservation &reservation) noexcept {
  if (reservation.Bytes() != bytes.capacity()) { return std::nullopt; }
  return ByteBlock(std::move(bytes), std::move(reservation));
}

ByteLease::ByteLease(std::shared_ptr<const ByteBlock> block) noexcept : Block_(std::move(block)) {}

std::optional<ByteLease> ByteLease::Adopt(std::vector<uint8_t> &bytes,
                                          MemoryBudget::Reservation &reservation) {
  auto block = ByteBlock::Adopt(bytes, reservation);
  if (!block) { return std::nullopt; }
  return ByteLease(std::make_shared<ByteBlock>(std::move(*block)));
}

std::span<const uint8_t> ByteLease::Bytes() const noexcept {
  return Block_ ? Block_->Bytes() : std::span<const uint8_t>{};
}

size_t ByteLease::CapacityBytes() const noexcept {
  return Block_ ? Block_->CapacityBytes() : 0;
}
}
