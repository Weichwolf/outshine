#ifndef OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTBODYIDENTITY_H
#define OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTBODYIDENTITY_H
#include "GroundPortBodies.h"
#include <array>
#include <cstdint>

namespace outshine::Generators {
[[nodiscard]] std::array<uint8_t, 32> GroundPortBodyContentBinding(const GroundPortField &field,
                                                                   const GroundPortBodies &bodies);
}
#endif
