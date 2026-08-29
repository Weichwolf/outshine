#ifndef OUTSHINE_CONTENT_GLTF_AXES_H
#define OUTSHINE_CONTENT_GLTF_AXES_H

namespace outshine::Gltf {

void InEcef(const double gltf[3], double out[3]);

void PlacedInEcef(const double gltf[16], double out[16]);

}
#endif
