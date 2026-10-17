#ifndef OUTSHINE_RENDER_STAGES_GROUNDPAGEUPLOAD_H
#define OUTSHINE_RENDER_STAGES_GROUNDPAGEUPLOAD_H

#include "GroundLattice.h"

namespace outshine::Render {
[[nodiscard]] Native::Result<void> CopyHeightPageLayers(Native::Commands &commands,
                                                        const Native::Texture &source,
                                                        Native::Texture &destination);
[[nodiscard]] Native::Result<void> UploadHeightPages(Native::Device &device,
                                                     Native::Commands &commands,
                                                     Native::Texture &atlas,
                                                     std::span<const PageId> pages,
                                                     std::span<const HeightPageView> nodes);
}
#endif
