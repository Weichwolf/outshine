#ifndef OUTSHINE_RENDER_STAGES_NORMALUPLOAD_H
#define OUTSHINE_RENDER_STAGES_NORMALUPLOAD_H

#include "SubjectTypes.h"
#include <cstdint>

namespace outshine::Render {

static_assert(sizeof(uint32_t) == sizeof(float));

struct NormalUpload {
  SubjectStream Source;

  static void Write(const void *carrying, float *into, uint32_t words);
};

}
#endif
