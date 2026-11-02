#ifndef OUTSHINE_RENDER_DEVICE_FIELDPAGEFEEDBACK_H
#define OUTSHINE_RENDER_DEVICE_FIELDPAGEFEEDBACK_H
#include "FieldPageTable.h"
#include "native/Commands.h"
#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace outshine::Render {
struct alignas(16) FieldPageFeedbackPixel {
  std::array<float, 4> Words{};
};

static_assert(sizeof(FieldPageFeedbackPixel) == 16 && alignof(FieldPageFeedbackPixel) == 16 &&
              offsetof(FieldPageFeedbackPixel, Words) == 0);

[[nodiscard]] Native::Result<FieldPageFeedbackPixel>
EncodeFieldPageFeedback(FieldPageAddress address);
[[nodiscard]] Native::Result<std::optional<FieldPageAddress>>
DecodeFieldPageFeedback(FieldPageFeedbackPixel pixel);
[[nodiscard]] Native::Result<size_t> DecodeFieldPageFeedback(std::span<const std::byte> bytes,
                                                             std::span<FieldPageAddress> into);
}
#endif
