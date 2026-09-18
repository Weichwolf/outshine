#ifndef OUTSHINE_IMPORT_MATERIALIMPORT_H
#define OUTSHINE_IMPORT_MATERIALIMPORT_H

#include "MaterialAsset.h"

namespace outshine::Gltf {

class Document;

void ImportMaterialAssets(const Document &document, MaterialAssetSet &out);

}
#endif
