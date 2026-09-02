#ifndef OUTSHINE_GENERATORS_BASE_GROUNDTABLE_H
#define OUTSHINE_GENERATORS_BASE_GROUNDTABLE_H

#include <span>
#include <memory>
#include <vector>

#include "scene/Material.h"

namespace outshine::Generators {

constexpr float kFrictionUnsaid = 0.6f;
constexpr float kSlopeMaxUnsaidDeg = 90.0f;

class GroundTable {
public:
  struct Row {
    Material Surface;

    float FrictionMu = kFrictionUnsaid;

    float SlopeMaxDeg = kSlopeMaxUnsaidDeg;
  };

  static std::shared_ptr<const GroundTable> Of(std::span<const Row> rows);

  [[nodiscard]] size_t Count() const { return Rows_.size(); }

  [[nodiscard]] const Row &At(size_t row) const { return Rows_[row]; }

  [[nodiscard]] size_t HeapBytes() const { return Rows_.capacity() * sizeof(Row); }

private:
  explicit GroundTable(std::span<const Row> rows) : Rows_(rows.begin(), rows.end()) {}

  std::vector<Row> Rows_;
};

} // namespace outshine::Generators
#endif
