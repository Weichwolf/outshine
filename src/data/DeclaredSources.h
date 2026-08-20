#ifndef DECLAREDSOURCES_H
#define DECLAREDSOURCES_H

#include <string>

#include "SourceSet.h"

namespace outshine::Data {

struct Declared {

  std::string StarDirectory;

  bool WithUpstreams = true;
};

enum class Registered { Complete, RankClash };

[[nodiscard]] Registered RegisterDeclared(SourceSet &set, const Declared &declared);

}
#endif
