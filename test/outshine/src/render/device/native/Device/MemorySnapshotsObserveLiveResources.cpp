#include "Device.h"
#include "MemoryStatus.h"
#include "Resources.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <print>
#include <string>

int main() {
  using namespace outshine::Render::Native;
  using namespace outshine::Test;
  auto device = CreateDevice({.Validation = true});
  CHECK(device, "a real native device opens without a scene or window");
  if (!device) {
    std::println(stderr, "{}", device.error().Message);
    return Report();
  }
  auto &gpu = **device;
  const auto before = gpu.ReadMemoryStatus();
  constexpr size_t bytes = size_t{1024} * 1024;
  auto buffer =
      gpu.CreateBuffer({.Bytes = bytes, .Use = BufferUse::CopySource | BufferUse::CopyDestination});
  auto texture = gpu.CreateTexture({.Width = 64,
                                    .Height = 64,
                                    .Texels = Format::Rgba8Unorm,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination});
  CHECK(buffer && texture, "real device buffer and texture resources exist during observation");
  if (!buffer || !texture) { return Report(); }
  const auto during = gpu.ReadMemoryStatus();
  buffer->reset();
  texture->reset();
  const auto after = gpu.ReadMemoryStatus();
  CHECK(before.CollectionNs && during.CollectionNs && after.CollectionNs,
        "each observation includes its own collection cost, outside frame work");
#ifdef __APPLE__
  CHECK(before.ResourceAllocatedBytes && during.ResourceAllocatedBytes &&
            after.ResourceAllocatedBytes && during.RecommendedWorkingSetBytes &&
            *during.RecommendedWorkingSetBytes > 0 && during.UnifiedMemory.has_value(),
        "Metal reports actual resource allocations, a performance recommendation and UMA support");
  CHECK(during.ResourceAllocatedBytes && *during.ResourceAllocatedBytes >= bytes &&
            during.Heaps.empty(),
        "live Metal resource bytes include the actual buffer without fabricated Vulkan heaps");
#else
  CHECK(!during.ResourceAllocatedBytes && !during.RecommendedWorkingSetBytes &&
            !during.UnifiedMemory && !during.Heaps.empty(),
        "Vulkan heap observations do not impersonate Metal resource or topology counters");
  for (const auto &heap : during.Heaps) {
    CHECK(heap.CapacityBytes > 0 &&
              heap.EstimatedUsageBytes.has_value() == heap.EstimatedBudgetBytes.has_value(),
          "each physical heap has capacity and consistently available optional driver estimates");
  }
#endif
  const auto value = [](const auto &field) {
    return field ? std::to_string(*field) : std::string("unknown");
  };
  std::println("GPU_MEMORY resource_bytes before={} live={} released={}",
               value(before.ResourceAllocatedBytes),
               value(during.ResourceAllocatedBytes),
               value(after.ResourceAllocatedBytes));
  return Report();
}
