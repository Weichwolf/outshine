#ifndef OUTSHINE_RENDER_STAGES_METALROUGHBRDF_H
#define OUTSHINE_RENDER_STAGES_METALROUGHBRDF_H

#include <array>
#include <cmath>

#include "math/Units.h"

namespace outshine::Render {

[[nodiscard]] inline double BrdfVisibility(double nl, double nv, double a2) {
  return 0.5 /
         (nl * std::sqrt(nv * nv * (1.0 - a2) + a2) + nv * std::sqrt(nl * nl * (1.0 - a2) + a2));
}

}
#endif
