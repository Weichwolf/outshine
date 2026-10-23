#ifndef OUTSHINE_RENDER_PRISM_PRISMUPLOAD_H
#define OUTSHINE_RENDER_PRISM_PRISMUPLOAD_H

#include "device/native/Device.h"
#include <array>
#include <cstddef>
#include <memory>
#include <span>

namespace outshine::Render {
class UploadQueue;
using PrismBuffers = std::array<std::unique_ptr<Native::Buffer>, 2>;

[[nodiscard]] Native::Result<PrismBuffers> UploadPrismBuffers(Native::Device &device,
                                                              UploadQueue *queue,
                                                              std::span<const std::byte> samples,
                                                              std::span<const std::byte> patches);
}
#endif
