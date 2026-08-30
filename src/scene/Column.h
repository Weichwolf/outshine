#ifndef OUTSHINE_SCENE_COLUMN_H
#define OUTSHINE_SCENE_COLUMN_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <Scene.h>

namespace outshine {

template <class Value> class Column {
public:
  [[nodiscard]] bool Open(const Scene &of) {
    if (of.capacity() == 0) { return false; }
    Bound_ = &of;
    Values_.assign(of.capacity(), Value{});
    Generations_.assign(of.capacity(), 0);
    Held_.assign(of.capacity(), 0);
    return true;
  }

  [[nodiscard]] bool Put(Entity of, const Value &value) {
    if (Bound_ == nullptr || !Bound_->alive(of) || of.Index >= Values_.size()) { return false; }
    Values_[of.Index] = value;
    Generations_[of.Index] = of.Generation;
    Held_[of.Index] = 1;
    return true;
  }

  [[nodiscard]] const Value *Get(Entity of) const {
    if (Bound_ == nullptr || !Bound_->alive(of) || of.Index >= Values_.size()) { return nullptr; }
    if (Held_[of.Index] == 0 || Generations_[of.Index] != of.Generation) { return nullptr; }
    return &Values_[of.Index];
  }

  void Drop(Entity of) {
    if (of.Index < Held_.size() && Generations_[of.Index] == of.Generation) { Held_[of.Index] = 0; }
  }

  template <class Fn> void Each(Fn &&fn) const {
    if (Bound_ == nullptr) { return; }
    for (uint32_t at = 0; at < (uint32_t)Values_.size(); ++at) {
      if (Held_[at] == 0) { continue; }
      const Entity of{at, Generations_[at]};
      if (!Bound_->alive(of)) { continue; }
      fn(of, Values_[at]);
    }
  }

private:
  const Scene *Bound_ = nullptr;
  std::vector<Value> Values_;
  std::vector<uint32_t> Generations_;
  std::vector<uint8_t> Held_;
};

}

#endif
