#ifndef OUTSHINE_WORLD_GENERATORS_GROUNDTABLE_H
#define OUTSHINE_WORLD_GENERATORS_GROUNDTABLE_H

#include <memory>
#include <vector>

#include "Material.h"
#include "Span.h"

namespace outshine::Generators {

class GroundTable {
public:
  struct Row {
    Material Surface;

    float FrictionMu = 0.6f;

    float SlopeMaxDeg = 90.0f;
  };

  static std::shared_ptr<const GroundTable> Of(Span<const Row> rows);

  [[nodiscard]] size_t Count() const { return Rows_.size(); }
  [[nodiscard]] const Row &At(size_t row) const { return Rows_[row]; }
  [[nodiscard]] size_t HeapBytes() const { return Rows_.capacity() * sizeof(Row); }

private:
  explicit GroundTable(Span<const Row> rows) : Rows_(rows.begin(), rows.end()) {}

  std::vector<Row> Rows_;
};

}
#endif
