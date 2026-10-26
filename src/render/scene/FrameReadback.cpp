#include "FrameReadback.h"
#include "SceneRenderer.h"
#include <cstdint>
#include <string>
#include <vector>

namespace outshine::Render {
bool ReadFrame(SceneRenderer &renderer, std::vector<uint8_t> &rgba, std::string &error) {
  const auto published = renderer.PublishedWorld();
  if (!renderer.Drew()) {
    error = "nothing has been drawn yet, so there is no frame to read";
    return false;
  }
  if (renderer.ReadPixels(rgba) != ReadState::Ready) {
    error = renderer.WhyNot();
    return false;
  }
  return true;
}

bool ReadFrame(SceneRenderer &renderer,
               outshine::Buffer buffer,
               std::vector<float> &out,
               std::string &error) {
  const auto published = renderer.PublishedWorld();
  if (!renderer.Drew()) {
    error = "nothing has been drawn yet, so there is no frame to read";
    return false;
  }
  ReadState state = ReadState::Failed;
  switch (buffer) {
    case outshine::Buffer::Colour:
      error = "the displayed picture is read as bytes, not as scene-referred float";
      return false;
    case outshine::Buffer::Linear: state = renderer.ReadSceneLinear(out); break;
    case outshine::Buffer::Depth: state = renderer.ReadDepth(out); break;
    case outshine::Buffer::ShadingNormal: state = renderer.ReadShadingNormal(out); break;
    case outshine::Buffer::SurfaceIdentity: state = renderer.ReadSurfaceIdentity(out); break;
    case outshine::Buffer::Velocity:
      if (!renderer.Plan().Holds(Resource::SceneVelocity)) {
        error = "this plan carries no velocity, so no frame of it has one to read";
        return false;
      }
      state = renderer.ReadSceneVelocity(out);
      break;
  }
  if (state != ReadState::Ready) {
    error = "the frame did not come back from the device";
    return false;
  }
  return true;
}

}
