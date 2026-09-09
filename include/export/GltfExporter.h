#ifndef OUTSHINE_EXPORT_GLTFEXPORTER_H
#define OUTSHINE_EXPORT_GLTFEXPORTER_H

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace outshine {
class Geometry;

/// Encode a native geometry snapshot into an owned glTF 2.0 binary container.
/// @param geometry Borrowed CPU geometry in native metres, right-handed Y-up coordinates;
/// must not be moved from or concurrently mutated. No references are retained or invalidated.
/// @return Complete GLB bytes, or an owned diagnostic; failure publishes no partial output.
/// Supports triangle meshes, placements, vertex attributes and core metallic-roughness factors
/// plus unlit materials. Images, lights and additional material features are rejected rather
/// than silently discarded. This is a geometry snapshot, not a world or animation serializer.
/// Allocates and performs CPU conversion proportional to input size; preparation operation,
/// not a realtime call. No filesystem/GPU access or thread affinity; independent calls are safe.
/// Allocation failure follows the allocator contract, separately from validation errors.
[[nodiscard]] std::expected<std::vector<uint8_t>, std::string> exportGlb(const Geometry &geometry);
}
#endif
