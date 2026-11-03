#ifndef OUTSHINE_RENDER_TERRAIN_CLIPMAPNORMALPASS_H
#define OUTSHINE_RENDER_TERRAIN_CLIPMAPNORMALPASS_H
#include "device/native/Device.h"
#include <memory>

namespace outshine::Render {
class ClipmapHeightCache;
class ClipmapHeightUpdate;

class ClipmapNormalPass {
public:
  [[nodiscard]] Native::Result<void> Configure(Native::Device &device);
  [[nodiscard]] Native::Result<void> Encode(Native::Commands &commands,
                                            const Native::Texture &heights,
                                            Native::Texture &normals,
                                            const ClipmapHeightCache &cache,
                                            const ClipmapHeightUpdate &update) const;

private:
  std::unique_ptr<Native::ComputePipeline> Pipeline_;
  std::unique_ptr<Native::Sampler> Sampling_;
};
}
#endif
