#ifndef OUTSHINE_BASE_MATH_ROTATION_H
#define OUTSHINE_BASE_MATH_ROTATION_H

#include "math/Mat4.h"
#include "math/Quat.h"

namespace outshine {
[[nodiscard]] Quat UnitRotationOf(const Mat4 &matrix) noexcept;
}

#endif
