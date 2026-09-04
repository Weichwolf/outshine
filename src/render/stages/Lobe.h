#ifndef OUTSHINE_RENDER_STAGES_LOBE_H
#define OUTSHINE_RENDER_STAGES_LOBE_H

namespace outshine::Render {

struct Slant {
  double Cosine = 0.0;
  double Roughness = 0.0;
};

struct Grazing {
  double NoL = 0.0;
  double NoV = 0.0;
};

} // namespace outshine::Render
#endif
