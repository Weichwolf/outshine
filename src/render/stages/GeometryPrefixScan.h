#ifndef OUTSHINE_RENDER_STAGES_GEOMETRYPREFIXSCAN_H
#define OUTSHINE_RENDER_STAGES_GEOMETRYPREFIXSCAN_H

#include "native/Device.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace outshine::Render {
class GeometryPrefixScan {
public:
  struct Source {
    const Native::Buffer *Addresses = nullptr;
    const Native::Buffer *Kept = nullptr;
    const Native::Buffer *Visible = nullptr;
    const Native::Buffer *Slots = nullptr;
    const Native::Buffer *Arguments = nullptr;
    uint32_t LogicalCount = 0;
  };

  [[nodiscard]] Native::Result<void> Configure(Native::Device &device);
  [[nodiscard]] Native::Result<void> Encode(Native::Commands &commands, Source inputs);

private:
  struct Level {
    std::unique_ptr<Native::Buffer> Prefix, Tail;
    uint32_t Count = 0;
  };

  [[nodiscard]] Native::Result<size_t> Prepare(uint32_t count);
  Native::Device *Device_ = nullptr;
  std::array<std::unique_ptr<Native::ComputePipeline>, 4> Programs_;
  std::vector<Level> Levels_;
};
}
#endif
