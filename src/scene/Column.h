#ifndef OUTSHINE_SCENE_COLUMN_H
#define OUTSHINE_SCENE_COLUMN_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Store.h"

namespace outshine {

template <class Value>
class Column {
public:
  [[nodiscard]] bool Open(size_t capacity) {
    if (capacity == 0) { return false; }
    Values_.assign(capacity, Value{});
    Generations_.assign(capacity, 0);
    Held_.assign(capacity, false);
    return true;
  }

  [[nodiscard]] bool Put(Entity of, const Store &in, const Value &value) {
    if (!in.Alive(of) || of.Index >= Values_.size()) { return false; }
    Values_[of.Index] = value;
    Generations_[of.Index] = of.Generation;
    Held_[of.Index] = true;
    return true;
  }

  [[nodiscard]] const Value *Get(Entity of, const Store &in) const {
    if (!in.Alive(of) || of.Index >= Values_.size()) { return nullptr; }
    if (!Held_[of.Index] || Generations_[of.Index] != of.Generation) { return nullptr; }
    return &Values_[of.Index];
  }

  void Drop(Entity of) {
    if (of.Index < Held_.size() && Generations_[of.Index] == of.Generation) {
      Held_[of.Index] = false;
    }
  }

private:
  std::vector<Value> Values_;
  std::vector<uint32_t> Generations_;
  std::vector<bool> Held_;
};

} // namespace outshine

#endif
