#ifndef OUTSHINE_CONTENT_GEOMETRYASSET_H
#define OUTSHINE_CONTENT_GEOMETRYASSET_H

#include "scene/Geometry.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine {

/// Encode an owned native scene product without device handles or source-format dependencies.
/// Preserves names, images, every material factor/map, mesh attributes, placements and lights.
/// Integers and IEEE floats use little endian; schema changes require a new format version.
/// Borrowed immutable geometry must outlive the call; no thread affinity or internal locking.
/// @param geometry Complete well-formed product or a canonical empty scene.
/// @param bytesMost Maximum encoded payload size; failure produces no partial asset.
/// @return Owned bytes, or absence for malformed geometry or an exceeded byte limit.
/// Payload stays within bytesMost; allocated vector capacity may exceed payload size.
/// Validation and encoding are linear in content.
[[nodiscard]] std::optional<std::vector<uint8_t>> EncodeGeometryAsset(const Geometry &geometry,
                                                                      size_t bytesMost);

/// Decode a native scene asset through Geometry's normal validation and ownership boundary.
/// No pointer, padding, GPU resource or provider state is persisted. Trailing bytes are invalid.
/// @param bytes Borrowed encoded payload, used only during this call.
/// @param bytesMost Maximum encoded payload size, checked before allocating product storage.
/// @return Independent owned geometry or absence for corruption, unsupported schema or limit.
/// Temporary attribute buffers and owned storage are proportional to the bounded input;
/// this limit is not an exact peak-residency budget. May allocate; no IO or thread affinity.
[[nodiscard]] std::optional<Geometry> DecodeGeometryAsset(std::span<const uint8_t> bytes,
                                                          size_t bytesMost);

}
#endif
