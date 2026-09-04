#ifndef OUTSHINE_BASE_FLATMAP_H
#define OUTSHINE_BASE_FLATMAP_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace outshine {

inline constexpr uint32_t kFlatMapFoldBits = 33u;
inline constexpr uint64_t kFlatMapOdd = 0xFF51AFD7ED558CCDULL;

template <typename Value> class FlatMap {
public:
  void Clear() noexcept {
    if (++Epoch_ == 0u) {
      for (Slot &one : Slots_) { one.Epoch = 0u; }
      Epoch_ = 1u;
    }
    Held_ = 0;
  }

  [[nodiscard]] bool Empty() const noexcept { return Held_ == 0; }

  [[nodiscard]] size_t Size() const noexcept { return Held_; }

  [[nodiscard]] size_t HeapBytes() const noexcept { return Slots_.capacity() * sizeof(Slot); }

  [[nodiscard]] Value *Find(uint64_t key) noexcept {
    if (Slots_.empty()) { return nullptr; }
    for (size_t at = Where(key);; at = (at + 1u) & Mask()) {
      Slot &one = Slots_[at];
      if (one.Epoch != Epoch_) { return nullptr; }
      if (one.Key == key) { return &one.Held; }
    }
  }

  [[nodiscard]] const Value *Find(uint64_t key) const noexcept {
    return const_cast<FlatMap *>(this)->Find(key);
  }

  [[nodiscard]] bool Holds(uint64_t key) const noexcept { return Find(key) != nullptr; }

  std::pair<Value *, bool> Emplace(uint64_t key, Value value) {
    if (Held_ * 10u >= Slots_.size() * 7u) { Widen(); }
    for (size_t at = Where(key);; at = (at + 1u) & Mask()) {
      Slot &one = Slots_[at];
      if (one.Epoch != Epoch_) {
        one.Key = key;
        one.Epoch = Epoch_;
        one.Held = std::move(value);
        ++Held_;
        return {&one.Held, true};
      }
      if (one.Key == key) { return {&one.Held, false}; }
    }
  }

  Value &operator[](uint64_t key) { return *Emplace(key, Value{}).first; }

private:
  struct Slot {
    uint64_t Key = 0;
    uint32_t Epoch = 0;
    Value Held{};
  };

  [[nodiscard]] size_t Mask() const noexcept { return Slots_.size() - 1u; }

  [[nodiscard]] size_t Where(uint64_t key) const noexcept {
    uint64_t mixed = key;
    mixed ^= mixed >> kFlatMapFoldBits;
    mixed *= kFlatMapOdd;
    mixed ^= mixed >> kFlatMapFoldBits;
    return static_cast<size_t>(mixed) & Mask();
  }

  void Widen() {
    const size_t wanted = Slots_.empty() ? 64u : Slots_.size() * 2u;
    std::vector<Slot> slots(wanted);
    Slots_.swap(slots);
    Held_ = 0;
    for (size_t at = 0; at < slots.size(); ++at) {
      if (slots[at].Epoch == Epoch_) { (void)Emplace(slots[at].Key, std::move(slots[at].Held)); }
    }
  }

  std::vector<Slot> Slots_;
  uint32_t Epoch_ = 1;
  size_t Held_ = 0;
};

} // namespace outshine
#endif
