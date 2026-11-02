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
  long BaseIndex = -1;
  long MetalRoughIndex = -1;
  bool FieldPages = false;
  long FieldFeedbackIndex = -1;

  [[nodiscard]] std::string VertexPath(std::string_view name) const {
    return std::format("{}-{}.vert.spv", name, WritesVelocity ? 1 : 0);
  }

  [[nodiscard]] std::string FragmentPath(std::string_view name) const {
    const auto pageSuffix = FieldFeedbackIndex >= 0 ? std::format("-f{}", FieldFeedbackIndex) : "";
    const auto source =
        FieldPages ? std::format("{}-pages{}", name, pageSuffix) : std::string(name);
    if (BaseIndex >= 0 && MetalRoughIndex >= 0) {
      return std::format("{}-{}{}{}{}{}.frag.spv",
                         source,
                         WritesVelocity ? 1 : 0,
                         NormalIndex < 0 ? 0 : NormalIndex,
                         IdentityIndex < 0 ? 0 : IdentityIndex,
                         BaseIndex,
                         MetalRoughIndex);
    }
    return std::format("{}-{}{}{}.frag.spv",
                       source,
                       WritesVelocity ? 1 : 0,
                       NormalIndex < 0 ? 0 : NormalIndex,
                       IdentityIndex < 0 ? 0 : IdentityIndex);
  }
};

}
#endif
