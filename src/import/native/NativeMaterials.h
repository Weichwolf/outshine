#ifndef OUTSHINE_IMPORT_NATIVE_NATIVEMATERIALS_H
#define OUTSHINE_IMPORT_NATIVE_NATIVEMATERIALS_H

#include <string>

#include "scene/Geometry.h"

namespace outshine::Gltf {

class Document;
class Subject;

[[nodiscard]] bool ResolveNativeMaterialImages(const Document &document,
                                               const Subject &subject,
                                               Geometry &geometry,
                                               std::string &error);

}
#endif
