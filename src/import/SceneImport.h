#ifndef OUTSHINE_IMPORT_SCENEIMPORT_H
#define OUTSHINE_IMPORT_SCENEIMPORT_H

#include <string>

#include "SceneAsset.h"

namespace outshine::Gltf {

class Document;

[[nodiscard]] bool ImportSceneAsset(const Document &document, SceneAsset &out, std::string &error);

}
#endif
