#ifndef OUTSHINE_BASE_FLATMAP_H
#define OUTSHINE_BASE_FLATMAP_H

#include <cstdint>
#include <utility>
#include <vector>

namespace outshine {

inline constexpr uint64_t kFlatMapFree = 0xFFFFFFFFFFFFFFFFULL;
inline constexpr uint32_t kFlatMapFoldBits = 33u;
inline constexpr uint64_t kFlatMapOdd = 0xFF51AFD7ED558CCDULL;

template <typename Value> class FlatMap {
public:
  void Clear() noexcept {
    for (uint64_t &key : Keys_) { key = kFlatMapFree; }
    Held_ = 0;
  }

  [[nodiscard]] bool Empty() const noexcept { return Held_ == 0; }

  [[nodiscard]] size_t Size() const noexcept { return Held_; }

  [[nodiscard]] Value *Find(uint64_t key) noexcept {
    if (Keys_.empty()) { return nullptr; }
    for (size_t at = Slot(key);; at = (at + 1u) & Mask()) {
      if (Keys_[at] == key) { return &Values_[at]; }
      if (Keys_[at] == kFlatMapFree) { return nullptr; }
    }
  }

  [[nodiscard]] const Value *Find(uint64_t key) const noexcept {
    return const_cast<FlatMap *>(this)->Find(key);
  }

  [[nodiscard]] bool Holds(uint64_t key) const noexcept { return Find(key) != nullptr; }

  std::pair<Value *, bool> Emplace(uint64_t key, Value value) {
    if (Held_ * 10u >= Keys_.size() * 7u) { Widen(); }
    for (size_t at = Slot(key);; at = (at + 1u) & Mask()) {
      if (Keys_[at] == key) { return {&Values_[at], false}; }
      if (Keys_[at] == kFlatMapFree) {
        Keys_[at] = key;
        Values_[at] = std::move(value);
        ++Held_;
        return {&Values_[at], true};
      }
    }
  }

  Value &operator[](uint64_t key) { return *Emplace(key, Value{}).first; }

private:
  [[nodiscard]] size_t Mask() const noexcept { return Keys_.size() - 1u; }

  [[nodiscard]] size_t Slot(uint64_t key) const noexcept {
    uint64_t mixed = key;
    mixed ^= mixed >> kFlatMapFoldBits;
    mixed *= kFlatMapOdd;
    mixed ^= mixed >> kFlatMapFoldBits;
    return static_cast<size_t>(mixed) & Mask();
  }

  void Widen() {
    const size_t wanted = Keys_.empty() ? 64u : Keys_.size() * 2u;
    std::vector<uint64_t> keys(wanted, kFlatMapFree);
    std::vector<Value> values(wanted);
    Keys_.swap(keys);
    Values_.swap(values);
    Held_ = 0;
    for (size_t at = 0; at < keys.size(); ++at) {
      if (keys[at] != kFlatMapFree) { (void)Emplace(keys[at], std::move(values[at])); }
    }
  }

  std::vector<uint64_t> Keys_;
  std::vector<Value> Values_;
  size_t Held_ = 0;
};

} // namespace outshine
#endif
