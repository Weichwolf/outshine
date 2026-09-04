#include "ParticipatingMedium.h"

#include "ShaderFile.h"
#include <string>

namespace outshine::Render {

ShaderText &ParticipatingMedium(ShaderText &into) {
  return into.Adds("#define MEDIUM_CONST constant\n#define MEDIUM_THREAD thread\n")
      .Reads("src/render/shaders/mediumLayout.msl")
      .Reads("src/render/stages/MediumCore.h")
      .Reads("src/render/shaders/medium.msl");
}

} // namespace outshine::Render
