#ifndef OUTSHINE_RENDER_TERRAIN_CLIPMAPHEIGHTUPLOAD_H
#define OUTSHINE_RENDER_TERRAIN_CLIPMAPHEIGHTUPLOAD_H

#include "device/native/Commands.h"
#include <cstddef>

namespace outshine::Render {
class ClipmapHeightCache;
class ClipmapHeightUpdate;

namespace Native {
class Device;
class Texture;
}

[[nodiscard]] Native::Result<void> UploadClipmapHeights(Native::Device &device,
                                                        Native::Commands &commands,
                                                        Native::Texture &target,
                                                        const ClipmapHeightCache &cache,
                                                        const ClipmapHeightUpdate &update,
                                                        size_t stagingBytesMost);
}
#endif
