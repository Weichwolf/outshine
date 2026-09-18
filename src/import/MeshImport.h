#ifndef OUTSHINE_IMPORT_MESHIMPORT_H
#define OUTSHINE_IMPORT_MESHIMPORT_H

#include <string>

#include "MeshAsset.h"

namespace outshine::Gltf {

class Document;

[[nodiscard]] bool
ImportMeshAssets(const Document &document, MeshAssetSet &out, std::string &error);

}
#endif
