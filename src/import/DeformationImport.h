#ifndef OUTSHINE_IMPORT_DEFORMATIONIMPORT_H
#define OUTSHINE_IMPORT_DEFORMATIONIMPORT_H

#include <string>

#include "DeformationAsset.h"

namespace outshine::Gltf {

class Document;

[[nodiscard]] bool
ImportDeformations(const Document &document, DeformationAsset &out, std::string &error);

}
#endif
