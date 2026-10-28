#include "MaterialRegistration.h"
#include "MaterialSource.h"
#include "MaterialBinding.h"
#include <cstdint>

namespace outshine::Render {
uint32_t MaterialRegistration::At(uint32_t offset) const noexcept {
  return Source && offset < Source->RegisteredRows.size() ? Source->RegisteredRows[offset]
                                                          : kNoMaterialSlot;
}
}
