#ifndef OUTSHINE_RENDER_IMPOSTOR_RADIANCEINTEGRAL_H
#define OUTSHINE_RENDER_IMPOSTOR_RADIANCEINTEGRAL_H
#include "Device.h"
#include <memory>

namespace outshine::Render {
class RadianceIntegral {
public:
  [[nodiscard]] Native::Result<void> Configure(Native::Device &device);
  // Padded Float32 sums; caller holds source, scratch and target through submission.
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
