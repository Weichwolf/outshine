#ifndef OUTSHINE_IMPORT_SKELETONIMPORT_H
#define OUTSHINE_IMPORT_SKELETONIMPORT_H

#include <string>
#include <vector>

#include "Skeleton.h"

namespace outshine::Gltf {

class Document;

[[nodiscard]] bool
ImportSkeletons(const Document &document, std::vector<Skeleton> &out, std::string &error);

}
#endif
