#ifndef GROUNDTABLE_H
#define GROUNDTABLE_H

#include <memory>
#include <vector>

#include "Material.h"
#include "Span.h"

namespace outshine::Generators {

class GroundTable {
public:
  struct Row {
    Material Surface;
    /* [SET] dry soil on rubber, the middle of the 0.5..0.8 band the handbooks give — a declared row
     * that says nothing else says this. */
    float FrictionMu = 0.6f;
    /* The steepest face this class still lies on; above it the ground is bare rock. [SET] vertical,
     * so a row that does not declare a limit imposes none. */
    float SlopeMaxDeg = 90.0f;
  };

  static std::shared_ptr<const GroundTable> Of(Span<const Row> rows);

  size_t Count() const { return Rows_.size(); }
  const Row &At(size_t row) const { return Rows_[row]; }
  size_t HeapBytes() const { return Rows_.capacity() * sizeof(Row); }

private:
  explicit GroundTable(Span<const Row> rows) : Rows_(rows.begin(), rows.end()) {}

  std::vector<Row> Rows_;
};

} // namespace outshine::Generators
#endif
