#ifndef OUTSHINE_SCENE_COLUMN_H
#define OUTSHINE_SCENE_COLUMN_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <scene/Scene.h>

namespace outshine {

template <class Value> class Column {
public:
  [[nodiscard]] bool Open(const Scene &of) {
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
  const Scene *Bound_ = nullptr;
  std::vector<Value> Values_;
  std::vector<Entity> Entities_;
};

}

#endif
