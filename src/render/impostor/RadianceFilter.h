#ifndef OUTSHINE_RENDER_IMPOSTOR_RADIANCEFILTER_H
#define OUTSHINE_RENDER_IMPOSTOR_RADIANCEFILTER_H

#include "Device.h"
#include <memory>

namespace outshine::Render {
class RadianceFilter {
public:
  [[nodiscard]] Native::Result<void> Configure(Native::Device &device);
  // Linear premultiplied HDR/coverage; caller owns both images through submission.
  [[nodiscard]] Native::Result<void> Encode(Native::Commands &commands,
                                            const Native::Texture &source,
                                            Native::Texture &destination) const;

private:
  Native::Device *Device_ = nullptr;
  std::unique_ptr<Native::ComputePipeline> Pipeline_;
  std::unique_ptr<Native::Sampler> Sampler_;
};
}
#endif
