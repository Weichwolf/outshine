#ifndef OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTBODYINTERFACES_H
#define OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTBODYINTERFACES_H
#include "GroundPortBodies.h"
#include <expected>
#include <cstddef>
#include <stop_token>

namespace outshine::Generators {
[[nodiscard]] size_t GroundFieldInterfaceBytesMost(size_t faces) noexcept;
[[nodiscard]] std::expected<void, GroundPortBodyError> MarkGroundFieldInterfaces(
    const GroundPortField &field, GroundPortBodies &bodies, const std::stop_token &stop);
}
#endif
