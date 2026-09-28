#include "Check.h"
#include "SceneRenderer.h"

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  SceneRenderer renderer;
  const auto before = renderer.LastSubmittedCamera();
  CHECK(before.Serial == 0, "an unsubmitted renderer has no camera frame");
  CameraBasis prepared;
  prepared.EyeM = {{123, 456, 789}};
  prepared.Forward = {{1, 0, 0}};
  prepared.Right = {{0, 0, 1}};
  renderer.SetCamera(prepared, Lens{});
  CHECK(!renderer.RenderFrame(), "an unconfigured device refuses the frame before submission");
  const auto &after = renderer.LastSubmittedCamera();
  CHECK(after.Serial == before.Serial && after.Basis.EyeM == before.Basis.EyeM &&
            after.Basis.Forward == before.Basis.Forward && after.Basis.Up == before.Basis.Up,
        "a failed draw preserves the prior submitted camera instead of publishing a future pose");
  return Report();
}
