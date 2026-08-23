#ifndef OUTSHINE_RENDER_STAGES_SHADERFILE_H
#define OUTSHINE_RENDER_STAGES_SHADERFILE_H

#include <string>
#include <string_view>

namespace outshine::Render {

[[nodiscard]] bool LoadShaderText(std::string_view treePath, std::string &into,
                                  std::string &error);

}

#endif
