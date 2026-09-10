#ifndef OUTSHINE_WORLD_ENTITY_COLUMN_H
#define OUTSHINE_WORLD_ENTITY_COLUMN_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include <span>
#include <type_traits>

#include <world/EntityRegistry.h>

namespace outshine {

template <class Value> class Column {
public:
  [[nodiscard]] bool Open(const EntityRegistry &of) {
    if (of.capacity() == 0) { return false; }
    Bound_ = &of;
    Values_.assign(of.capacity(), Value{});
    Entities_.assign(of.capacity(), kNoEntity);
    return true;
  }

  [[nodiscard]] bool Put(Entity of, const Value &value) {
    if (Bound_ == nullptr || !Bound_->alive(of) || of.Index >= Values_.size()) { return false; }
    Values_[of.Index] = value;
    Entities_[of.Index] = of;
    return true;
  }

  struct Replacement {
    Entity Owner = kNoEntity;
    Value Data{};
  };

  [[nodiscard]] bool Replace(std::span<const Replacement> replacements) noexcept
    requires std::is_trivially_copy_assignable_v<Value>
  {
    static_assert(std::is_nothrow_copy_assignable_v<Value>);
    for (const auto &replacement : replacements) {
      if (Get(replacement.Owner) == nullptr) { return false; }
    }
    for (const auto &replacement : replacements) {
      Values_[replacement.Owner.Index] = replacement.Data;
    }
    return true;
  }

  [[nodiscard]] const Value *Get(Entity of) const {
    if (Bound_ == nullptr || !Bound_->alive(of) || of.Index >= Values_.size()) { return nullptr; }
    if (Entities_[of.Index] != of) { return nullptr; }
    return &Values_[of.Index];
  }

  void Drop(Entity of) {
    if (of.Index < Entities_.size() && Entities_[of.Index] == of) {
      Entities_[of.Index] = kNoEntity;
    }
  }

  template <class Fn> void Each(Fn &&fn) const {
    if (Bound_ == nullptr) { return; }
    for (uint32_t at = 0; at < static_cast<uint32_t>(Values_.size()); ++at) {
      const Entity of = Entities_[at];
      if (!Bound_->alive(of)) { continue; }
      fn(of, Values_[at]);
    }
  }

private:
  const EntityRegistry *Bound_ = nullptr;
  std::vector<Value> Values_;
  std::vector<Entity> Entities_;
};

}

#endif
