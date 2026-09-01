#ifndef OUTSHINE_IMPORT_AXES_H
#define OUTSHINE_IMPORT_AXES_H

#include "math/Mat4.h"
#include "math/Vec3.h"
#include "math/Vec3.h"

namespace outshine::Gltf {

void InEcef(const Vec3 &gltf, Vec3 &out);

void PlacedInEcef(const Mat4 &gltf, Mat4 &out);

} // namespace outshine::Gltf
#endif
