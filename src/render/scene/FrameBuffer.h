#ifndef OUTSHINE_RENDER_SCENE_FRAMEBUFFER_H
#define OUTSHINE_RENDER_SCENE_FRAMEBUFFER_H

namespace outshine {
enum class Buffer {
  Colour,
  Linear,
  Depth,
  ShadingNormal,
  SurfaceIdentity,

  Velocity
};
}
#endif
