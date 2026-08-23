#ifndef OUTSHINE_CORE_IO_HEAPPROBE_H
#define OUTSHINE_CORE_IO_HEAPPROBE_H

#include <cstddef>

namespace outshine {

class HeapProbe {
public:

  static size_t LiveBytes();

  static size_t BreakBytes();

  static size_t PeakLiveBytes();

  static double SampleCostMs();

  static size_t Sample();
};

}
#endif
