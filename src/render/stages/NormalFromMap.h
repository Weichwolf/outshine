#ifndef OUTSHINE_RENDER_STAGES_NORMALFROMMAP_H
#define OUTSHINE_RENDER_STAGES_NORMALFROMMAP_H

#include <string>

#include "ShaderFile.h"

namespace outshine::Render {

[[nodiscard]] inline std::string NormalFromMapMsl(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/normalFromMap.msl", body, error)) { return {}; }
  return body;
}

} // namespace outshine::Render
#endif
