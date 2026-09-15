#ifndef OUTSHINE_BASE_FLATMAP_H
#define OUTSHINE_BASE_FLATMAP_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace outshine {

inline constexpr uint32_t kFlatMapFoldBits = 33u;
inline constexpr uint64_t kFlatMapOdd = 0xFF51AFD7ED558CCDULL;

struct FlatMapIdentityHash {
  [[nodiscard]] constexpr uint64_t operator()(uint64_t key) const noexcept { return key; }
};

enum class FlatMapError { AllocationFailed, CapacityExceeded };

template <typename Value, typename Key = uint64_t, typename Hasher = FlatMapIdentityHash>
class FlatMap {
  static_assert(std::is_nothrow_default_constructible_v<Hasher> &&
                std::is_nothrow_invocable_r_v<uint64_t, Hasher, const Key &>);
  static_assert(noexcept(std::declval<const Key &>() == std::declval<const Key &>()));
  static_assert(std::is_nothrow_default_constructible_v<Key> &&
                std::is_nothrow_default_constructible_v<Value>);
  static_assert(std::is_nothrow_copy_constructible_v<Key> &&
                std::is_nothrow_move_constructible_v<Key> &&
                std::is_nothrow_copy_assignable_v<Key> &&
                std::is_nothrow_move_assignable_v<Value> &&
                std::is_nothrow_move_constructible_v<Value>);

public:
  FlatMap() = default;
  FlatMap(const FlatMap &) = delete;
  FlatMap &operator=(const FlatMap &) = delete;

  FlatMap(FlatMap &&other) noexcept
      : Slots_(std::move(other.Slots_)),
        Capacity_(std::exchange(other.Capacity_, 0)),
        Epoch_(other.Epoch_),
        Held_(std::exchange(other.Held_, 0)) {}

  FlatMap &operator=(FlatMap &&other) noexcept {
    if (this != &other) {
      Slots_ = std::move(other.Slots_);
      Capacity_ = std::exchange(other.Capacity_, 0);
      Epoch_ = other.Epoch_;
      Held_ = std::exchange(other.Held_, 0);
    }
    return *this;
  }

  void Clear() noexcept {
    if (++Epoch_ == 0u) {
      for (size_t at = 0; at < Capacity_; ++at) { Slots_.get()[at].Epoch = 0u; }
      Epoch_ = 1u;
    }
    Held_ = 0;
  }

  [[nodiscard]] bool Empty() const noexcept { return Held_ == 0; }

  [[nodiscard]] size_t Size() const noexcept { return Held_; }

  [[nodiscard]] size_t HeapBytes() const noexcept { return Capacity_ * sizeof(Slot); }

  [[nodiscard]] Value *Find(const Key &key) noexcept {
    if (!Slots_) { return nullptr; }
    for (size_t at = Where(key, Capacity_ - 1);; at = (at + 1u) & (Capacity_ - 1)) {
      Slot &one = Slots_.get()[at];
      if (one.Epoch != Epoch_) { return nullptr; }
      if (one.StoredKey == key) { return &one.Held; }
    }
  }

  [[nodiscard]] const Value *Find(const Key &key) const noexcept {
    return const_cast<FlatMap *>(this)->Find(key);
  }

  [[nodiscard]] bool Holds(const Key &key) const noexcept { return Find(key) != nullptr; }

  [[nodiscard]] std::expected<std::pair<Value *, bool>, FlatMapError>
  Emplace(Key key, Value value) noexcept {
    const size_t threshold = (Capacity_ / 10u) * 7u + (Capacity_ % 10u * 7u + 9u) / 10u;
    if (Held_ >= threshold) {
      if (Value *existing = Find(key)) { return std::pair{existing, false}; }
      if (const auto grown = Widen(); !grown) { return std::unexpected(grown.error()); }
    }
    auto inserted = Insert(Slots_.get(), Capacity_, key, std::move(value));
    Held_ += inserted.second ? 1u : 0u;
    return inserted;
  }

private:
  struct Slot {
    Key StoredKey{};
    uint32_t Epoch = 0;
    Value Held{};
  };

  struct ReleaseSlots {
    size_t Count = 0;

    void operator()(Slot *slots) const noexcept {
      std::destroy_n(slots, Count);
      ::operator delete(slots, std::align_val_t{alignof(Slot)});
    }
  };

  using SlotStorage = std::unique_ptr<Slot, ReleaseSlots>;

  [[nodiscard]] static size_t Where(const Key &key, size_t mask) noexcept {
    uint64_t mixed = Hasher{}(key);
    mixed ^= mixed >> kFlatMapFoldBits;
    mixed *= kFlatMapOdd;
    mixed ^= mixed >> kFlatMapFoldBits;
    return static_cast<size_t>(mixed) & mask;
  }

  [[nodiscard]] std::pair<Value *, bool>
  Insert(Slot *slots, size_t capacity, const Key &key, Value &&value) noexcept {
    const size_t mask = capacity - 1;
    for (size_t at = Where(key, mask);; at = (at + 1u) & mask) {
      Slot &one = slots[at];
      if (one.Epoch != Epoch_) {
        one.StoredKey = key;
        one.Held = std::move(value);
        one.Epoch = Epoch_;
        return {&one.Held, true};
      }
      if (one.StoredKey == key) { return {&one.Held, false}; }
    }
  }

  [[nodiscard]] std::expected<void, FlatMapError> Widen() noexcept {
    constexpr size_t limit = std::numeric_limits<size_t>::max() / sizeof(Slot);
    if (Capacity_ > limit / 2u) { return std::unexpected(FlatMapError::CapacityExceeded); }
    const size_t wanted = Capacity_ == 0 ? 64u : Capacity_ * 2u;
    if (wanted > limit) { return std::unexpected(FlatMapError::CapacityExceeded); }
    SlotStorage next(static_cast<Slot *>(::operator new(
                         wanted * sizeof(Slot), std::align_val_t{alignof(Slot)}, std::nothrow)),
                     ReleaseSlots{wanted});
    if (!next) { return std::unexpected(FlatMapError::AllocationFailed); }
    std::uninitialized_value_construct_n(next.get(), wanted);
    for (size_t at = 0; at < Capacity_; ++at) {
      Slot &one = Slots_.get()[at];
      if (one.Epoch == Epoch_) {
        (void)Insert(next.get(), wanted, one.StoredKey, std::move(one.Held));
      }
    }
    Slots_ = std::move(next);
    Capacity_ = wanted;
    return {};
  }

  SlotStorage Slots_{nullptr, ReleaseSlots{}};
  size_t Capacity_ = 0;
  uint32_t Epoch_ = 1;
  size_t Held_ = 0;
};

}
#endif
