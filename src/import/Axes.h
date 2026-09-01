#ifndef OUTSHINE_IMPORT_AXES_H
#define OUTSHINE_IMPORT_AXES_H

#include "math/Vec3.h"
#include "math/Vec3.h"

namespace outshine::Gltf {

void InEcef(const Vec3 &gltf, Vec3 &out);

void PlacedInEcef(const double gltf[16], double out[16]);

} // namespace outshine::Gltf
#endif
