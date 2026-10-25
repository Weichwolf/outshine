#ifndef OUTSHINE_RENDER_PRISM_PRISMDRAWING_H
#define OUTSHINE_RENDER_PRISM_PRISMDRAWING_H

#include "PrismResidency.h"
#include <span>

namespace outshine::Render {

[[nodiscard]] Native::Result<void> EncodePrism(const PrismProduct &product,
                                               Native::Commands &commands,
                                               const Native::GraphicsPipeline &pipeline,
                                               std::span<const Native::Binding> bindings,
                                               Native::Viewport area,
                                               Native::Scissor clip);

[[nodiscard]] Native::Result<void> EncodePrismDepth(const PrismResidency &residency,
                                                    Native::Commands &commands,
                                                    const Native::GraphicsPipeline &pipeline,
                                                    const PrismDepthView &view,
                                                    Native::Viewport area,
                                                    Native::Scissor clip);
}
#endif
