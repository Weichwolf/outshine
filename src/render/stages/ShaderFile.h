#ifndef OUTSHINE_RENDER_STAGES_SHADERFILE_H
#define OUTSHINE_RENDER_STAGES_SHADERFILE_H

#include <string>
#include <string_view>

namespace outshine::Render {

class ShaderText {
public:
  ShaderText &Begins();
  ShaderText &Reads(std::string_view treePath);
  ShaderText &Adds(std::string_view text);

  [[nodiscard]] explicit operator bool() const { return Why_.empty(); }

  [[nodiscard]] const std::string &Why() const { return Why_; }

  [[nodiscard]] std::string Take();
  [[nodiscard]] std::string Take(std::string &error);

private:
  std::string Held_;
  std::string Why_;
};

} // namespace outshine::Render

#endif
