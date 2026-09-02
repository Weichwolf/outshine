#ifndef OUTSHINE_GENERATORS_BASE_BODYID_H
#define OUTSHINE_GENERATORS_BASE_BODYID_H

#include <cstdint>

namespace outshine::Generators {

class OccupancySink;
struct BodyRange;

class BodyId {
public:
  [[nodiscard]] uint32_t Index() const { return Index_; }

  [[nodiscard]] bool Is(const BodyId &other) const { return Index_ == other.Index_; }

private:
  explicit BodyId(uint32_t index) : Index_(index) {}
  friend struct BodyRange;

  uint32_t Index_;
};

struct BodyRange {
  uint32_t First = 0, Count = 0;

  [[nodiscard]] BodyId Nth(uint32_t at) const { return BodyId(First + at); }
};

} // namespace outshine::Generators
#endif
