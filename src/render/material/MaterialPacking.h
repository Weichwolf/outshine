#ifndef OUTSHINE_RENDER_MATERIAL_MATERIALPACKING_H
#define OUTSHINE_RENDER_MATERIAL_MATERIALPACKING_H

#include <array>
#include <cstddef>

#include "MaterialBinding.h"

namespace outshine::Render {

inline constexpr size_t kMaterialScalarCount = 40;
inline constexpr size_t kMaterialUvMatrixFloats = 6;
inline constexpr size_t kMaterialUvSetFloats = 1;
inline constexpr size_t kMaterialRecordFloatCount =
    kMaterialScalarCount + (kMaterialUvMatrixFloats + kMaterialUvSetFloats) * kMaterialTextureCount;

using GpuMaterialRecord = std::array<float, kMaterialRecordFloatCount>;

[[nodiscard]] GpuMaterialRecord PackMaterial(const MaterialBinding &material,
                                             float identity) noexcept;

}

#endif
