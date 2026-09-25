#ifndef OUTSHINE_RENDER_STAGES_SUBJECTMATERIALPACKING_H
#define OUTSHINE_RENDER_STAGES_SUBJECTMATERIALPACKING_H

#include <array>
#include <cstddef>

#include "SubjectTypes.h"

namespace outshine::Render {

inline constexpr size_t kSubjectSurfaceScalars = 36;
inline constexpr size_t kSubjectUvMatrixFloats = 6;
inline constexpr size_t kSubjectUvSetFloats = 1;
inline constexpr size_t kSubjectSurfaceFloats =
    kSubjectSurfaceScalars +
    (kSubjectUvMatrixFloats + kSubjectUvSetFloats) * kSubjectMaterialImages;

using PackedSubjectMaterial = std::array<float, kSubjectSurfaceFloats>;

[[nodiscard]] PackedSubjectMaterial PackSubjectMaterial(const SubjectMaterial &material,
                                                        float identity) noexcept;

}

#endif
