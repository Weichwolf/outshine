#include "SurfaceState.h"
#include <scene/Material.h>
#include "Check.h"

#if __has_include(<scene/SurfaceState.h>)
#error Renderer routing must not remain in the installed public material API
#endif

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Material material;
  auto state = StateOf(material);
  CHECK(state.Kind() == SurfaceKind::Opaque && state.WritesDepth() && !state.Blends(),
        "opaque material selects depth-writing opaque rendering");
  CHECK(CullsBackFaces(state, Winding::Trusted) && !CullsBackFaces(state, Winding::Unknown),
        "culling requires both material policy and trusted winding");
  material.Alpha = AlphaMode::Masked;
  material.CoverageCut = 0.25f;
  state = StateOf(material);
  CHECK(state.Kind() == SurfaceKind::Masked && state.CoverageCut() == 0.25f &&
            state.WritesDepth() && !state.Blends(),
        "masked surface keeps its declared cutoff and writes surviving fragment depth");
  material.Alpha = AlphaMode::Blended;
  material.DoubleSided = true;
  material.Emission = {{1, 0, 0}};
  state = StateOf(material);
  CHECK(state.Kind() == SurfaceKind::Blended && state.Blends() && !state.WritesDepth() &&
            !state.CullsBack() && state.Emits(),
        "transparent double-sided emissive surface retains its routing flags");
  return Report();
}
