#include "CameraState.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;

  CameraState camera;
  CHECK(camera.IsUnbound() && camera.NeedsBinding(), "a new camera has no renderer binding");
  camera.MarkBound();
  CHECK(!camera.NeedsBinding(), "a published camera remembers its binding");

  Viewpoint requested;
  requested.EyeM = {{3.0, 4.0, 5.0}};
  camera.Override(requested);
  CHECK(camera.HasOverride() && camera.Override().EyeM == requested.EyeM,
        "an explicit camera is retained as one complete value");
  CHECK(camera.NeedsBinding(), "a changed camera invalidates its renderer binding");

  camera.Prepare(camera.Override(), true, 7);
  camera.AdvanceOrbit(2.5);
  CameraState replacement = camera;
  CHECK(replacement.Prepared().HasExplicitCamera && replacement.Prepared().FramedParts == 7,
        "world replacement copies the prepared view as one state");
  CHECK(replacement.OrbitDegrees() == 2.5 && replacement.NeedsBinding(),
        "world replacement copies orbit and binding state");

  replacement.FrameSubject();
  CHECK(!replacement.HasOverride() && replacement.NeedsBinding(),
        "automatic framing clears the override and invalidates the binding");
  return Report();
}
