#ifndef OUTSHINE_RENDER_STAGES_SCENETARGETS_H
#define OUTSHINE_RENDER_STAGES_SCENETARGETS_H

#include "native/Graphics.h"

namespace outshine::Render {

inline constexpr Native::Format kVelocityFormat = Native::Format::Rg16Float;

#define SCENE_FLOAT(name, value) inline constexpr float name = value;
#include "SceneConstants.inc"
#undef SCENE_FLOAT

inline Native::ColourTarget VelocityTarget(bool writes) {
  Native::ColourTarget target{.Texels = kVelocityFormat, .Rgb = {}, .Alpha = {}};
  if (!writes) { target.Write = {}; }
  return target;
}

}
#endif
