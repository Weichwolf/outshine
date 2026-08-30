#ifndef RENDER_ROUTING_H
#define RENDER_ROUTING_H

#include "Mask.h"

namespace outshine::Render::Parity {

struct Routing {
  const Mask &Ours;
  const Mask &Theirs;

  const Mask &SurfaceDisagreeing;

  [[nodiscard]] bool ToAppearance(int x, int y) const { return !ToGeometry(x, y); }

  [[nodiscard]] bool ToGeometry(int x, int y) const {
    return Ours.At(x, y) != Theirs.At(x, y) || SurfaceDisagreeing.At(x, y);
  }
};

} // namespace outshine::Render::Parity
#endif
