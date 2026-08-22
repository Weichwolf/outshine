#ifndef SHADERFILE_H
#define SHADERFILE_H

#include <string>
#include <string_view>

namespace outshine::Render {

[[nodiscard]] bool LoadShaderText(std::string_view treePath, std::string &into,
                                  std::string &error);

} // namespace outshine::Render

#endif
