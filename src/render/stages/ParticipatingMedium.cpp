#include "ParticipatingMedium.h"

#include "ShaderFile.h"
#include <string>

namespace outshine::Render {

bool ParticipatingMediumMsl(std::string &into, std::string &error) {
  std::string layout;
  std::string core;
  std::string rest;
  if (!LoadShaderText("src/render/shaders/mediumLayout.msl", layout, error) ||
      !LoadShaderText("src/render/stages/MediumCore.h", core, error) ||
      !LoadShaderText("src/render/shaders/medium.msl", rest, error)) {
    return false;
  }
  into = std::string("#define MEDIUM_CONST constant\n#define MEDIUM_THREAD thread\n") + layout +
         core + rest;
  return true;
}

} // namespace outshine::Render
