#ifndef OUTSHINE_IMPORT_AXES_H
#define OUTSHINE_IMPORT_AXES_H

namespace outshine::Gltf {

void InEcef(const double gltf[3], double out[3]);

void PlacedInEcef(const double gltf[16], double out[16]);

} // namespace outshine::Gltf
#endif
