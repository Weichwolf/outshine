#ifndef BODYID_H
#define BODYID_H

#include <cstdint>

namespace outshine::Generators {

class Body;
class OccupancySink;

class BodyId {
public:
  uint32_t Index() const { return Index_; }
  [[nodiscard]] bool Is(const BodyId &other) const { return Index_ == other.Index_; }

private:
  explicit BodyId(uint32_t index) : Index_(index) {}
  friend class Body;
  friend class OccupancySink;

  uint32_t Index_;
};

} // namespace outshine::Generators
#endif
