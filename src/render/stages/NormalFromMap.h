#ifndef OUTSHINE_RENDER_STAGES_NORMALFROMMAP_H
#define OUTSHINE_RENDER_STAGES_NORMALFROMMAP_H

#include <string>

#include "ShaderFile.h"

namespace outshine::Render {

inline ShaderText &NormalFromMap(ShaderText &into) {
  return into.Reads("src/render/shaders/normalFromMap.msl");
}

[[nodiscard]] inline std::string NormalFromMapMsl(std::string &error) {
  ShaderText source;
  return NormalFromMap(source).Take(error);
}

} // namespace outshine::Render
#endif
