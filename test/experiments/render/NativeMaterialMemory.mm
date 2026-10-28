#include "NativeMaterialMemory.h"
#import <Metal/Metal.h>

namespace outshine::Test {
std::optional<uint64_t> MaterialDeviceAllocatedBytes() noexcept {
  @autoreleasepool {
    const id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) { return std::nullopt; }
    return [device currentAllocatedSize];
  }
}
}
