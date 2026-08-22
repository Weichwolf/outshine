#include "ParticipatingMedium.h"

#include <cstdio>
#include <numbers>

#include "ShaderFile.h"

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
  char pi[64];
  std::snprintf(pi, sizeof pi, "#define MEDIUM_PI %.17g\n", std::numbers::pi);
  into = std::string("#define MEDIUM_CONST constant\n#define MEDIUM_THREAD thread\n") + pi +
         layout + core + rest;
  return true;
}

} // namespace outshine::Render
