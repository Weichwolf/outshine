#ifndef OUTSHINE_RENDER_GPUFRAMESAMPLE_H
#define OUTSHINE_RENDER_GPUFRAMESAMPLE_H

#include <cstdint>

namespace outshine::Render {

struct GpuFrameSample {
  uint64_t Device = 0;
  uint64_t Submission = 0;
  double ElapsedMs = 0;
};

}

#endif
