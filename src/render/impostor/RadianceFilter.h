#ifndef OUTSHINE_RENDER_IMPOSTOR_RADIANCEFILTER_H
#define OUTSHINE_RENDER_IMPOSTOR_RADIANCEFILTER_H

#include "Device.h"
#include "Extent.h"
#include "RadianceImage.h"
#include <memory>

namespace outshine::Render {
class RadianceFilter {
public:
  [[nodiscard]] Native::Result<void> Configure(Native::Device &device);
  [[nodiscard]] Native::Result<std::shared_ptr<const RadianceImage>>
  Apply(Native::Device &device, const Native::Texture &source, Extent target);
  // Area-weighted linear premultiplied HDR/coverage reduction; caller holds images until
  // completion.
  [[nodiscard]] Native::Result<void> Encode(Native::Commands &commands,
                                            const Native::Texture &source,
                                            Native::Texture &destination) const;

private:
  Native::Device *Device_ = nullptr;
  std::unique_ptr<Native::ComputePipeline> Pipeline_, AreaPipeline_;
  std::shared_ptr<Native::Sampler> Sampler_;
};
}
#endif
