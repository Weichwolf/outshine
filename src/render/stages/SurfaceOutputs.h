#ifndef OUTSHINE_RENDER_STAGES_SURFACEOUTPUTS_H
#define OUTSHINE_RENDER_STAGES_SURFACEOUTPUTS_H

#include <format>
#include <string>
#include <string_view>

namespace outshine::Render {

struct SurfaceOutputs {
  bool WritesVelocity = false;
  long NormalIndex = -1;
  long IdentityIndex = -1;

  [[nodiscard]] std::string VertexPath(std::string_view name) const {
    return std::format("build/shaders/{}-{}.vert.spv", name, WritesVelocity ? 1 : 0);
  }

  [[nodiscard]] std::string FragmentPath(std::string_view name) const {
    return std::format("build/shaders/{}-{}{}{}.frag.spv",
                       name,
                       WritesVelocity ? 1 : 0,
                       NormalIndex < 0 ? 0 : NormalIndex,
                       IdentityIndex < 0 ? 0 : IdentityIndex);
  }
};

}
#endif
