#include "Check.h"
#include "Compiled.h"

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  const auto compiled = Compiled::Compile({.Outputs = {Resource::SceneHdr},
                                           .Content = {Stage::Subjects, Stage::Sky}});
  CHECK(compiled.has_value(), "the sky and subject plan compiles");
  if (!compiled) { return Report(); }
  bool irradiance = false;
  bool culling = false;
  for (const auto &pass : (*compiled)->Passes()) {
    for (size_t at = 0; at < pass.Count; ++at) {
      const Stage stage = (*compiled)->Order()[pass.First + at];
      if (stage != Stage::Irradiance && stage != Stage::SubjectCull) { continue; }
      const Resource expected = stage == Stage::Irradiance ? Resource::IrradianceBuffer
                                                          : Resource::ClusterKept;
      bool first = true;
      for (const Resource buffer : pass.Buffers) {
        if (first) {
          CHECK(buffer == expected,
                "SDL compute storage slot zero remains the buffer this kernel writes");
          first = false;
        }
      }
      CHECK(!first, "the compute output has a storage binding");
      irradiance |= stage == Stage::Irradiance;
      culling |= stage == Stage::SubjectCull;
    }
  }
  CHECK(irradiance && culling, "both independent kernels are covered");
  return Report();
}
