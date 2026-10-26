#ifndef OUTSHINE_BASE_MATH_ROTATION_H
#define OUTSHINE_BASE_MATH_ROTATION_H

#include "Mat4.h"
#include "Quat.h"

namespace outshine {
[[nodiscard]] Quat UnitRotationOf(const Mat4 &matrix) noexcept;
}

#endif
