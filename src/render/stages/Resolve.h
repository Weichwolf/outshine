#ifndef OUTSHINE_RENDER_STAGES_RESOLVE_H
#define OUTSHINE_RENDER_STAGES_RESOLVE_H

#include "Compiled.h"

namespace outshine::Render {

struct DisplayOptions {
  float Exposure = 1.0f;
  Transfer Curve = Transfer::Filmic;

  bool Temporal = false;
};

} // namespace outshine::Render
#endif
