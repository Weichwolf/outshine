#include "GroundTable.h"
#include <memory>

namespace outshine::Generators {

std::shared_ptr<const GroundTable> GroundTable::Of(Span<const Row> rows) {
  if (rows.Empty()) { return nullptr; }
  return std::shared_ptr<const GroundTable>(new GroundTable(rows));
}

} // namespace outshine::Generators
