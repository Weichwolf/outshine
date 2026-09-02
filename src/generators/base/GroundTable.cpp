#include <span>
#include "GroundTable.h"
#include <memory>

namespace outshine::Generators {

std::shared_ptr<const GroundTable> GroundTable::Of(std::span<const Row> rows) {
  if (rows.empty()) { return nullptr; }
  return std::shared_ptr<const GroundTable>(new GroundTable(rows));
}

} // namespace outshine::Generators
