#include "ParticipatingMedium.h"

#include "ShaderFile.h"

namespace outshine::Render {

bool ParticipatingMediumMsl(std::string &into, std::string &error) {
  return LoadShaderText("src/render/shaders/medium.msl", into, error);
}

} // namespace outshine::Render
