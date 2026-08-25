#ifndef OUTSHINE_BASE_IO_STACKPROBE_H
#define OUTSHINE_BASE_IO_STACKPROBE_H

#include <atomic>
#include <cstddef>

namespace outshine {

class StackProbe {
public:

  enum class Purpose { Frame, Class, Tile, Region };
  static constexpr int kPurposeCount = 4;

  static void Enter(Purpose purpose);

  static void Mark();

  static size_t PeakBytes(Purpose purpose);
  static size_t CapacityBytes(Purpose purpose);

  static size_t FloorBytes(Purpose purpose);
  static size_t LimitBytes(Purpose purpose);
  static const char *Name(Purpose purpose);
};

}
#endif
