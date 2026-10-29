#ifndef OUTSHINE_RENDER_IMPOSTOR_RADIANCEINTEGRAL_H
#define OUTSHINE_RENDER_IMPOSTOR_RADIANCEINTEGRAL_H
#include "Device.h"
#include "Extent.h"
#include <optional>
#include <memory>

namespace outshine::Render {
class RadianceIntegral {
public:
  [[nodiscard]] static std::optional<Extent> StorageExtent(Extent raw,
                                                           uint32_t textureLimit) noexcept;
  [[nodiscard]] Native::Result<void> Configure(Native::Device &device);
  // Adjacent hi/lo RGBA32F pairs, 2*(raw.Width+1) by raw.Height+1.
  // Caller holds source, scratch and target through submission.
  [[nodiscard]] Native::Result<void> Encode(Native::Commands &commands,
                                            const Native::Texture &source,
                                            Native::Texture &scratch,
                                            Native::Texture &target) const;

private:
  Native::Device *Device_ = nullptr;
  std::unique_ptr<Native::ComputePipeline> Rows_, Columns_;
  std::shared_ptr<Native::Sampler> Sampling_;
};
}
#endif
