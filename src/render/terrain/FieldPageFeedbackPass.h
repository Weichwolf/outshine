#ifndef OUTSHINE_RENDER_TERRAIN_FIELDPAGEFEEDBACKPASS_H
#define OUTSHINE_RENDER_TERRAIN_FIELDPAGEFEEDBACKPASS_H
#include "ReadbackTransfer.h"
#include "native/Device.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

namespace outshine::Render {
struct FieldPageFeedbackOptions {
  uint32_t Stride = 8;
  uint64_t StorageBytesMost = 0;
};

class FieldPageFeedbackPass final {
public:
  [[nodiscard]] Native::Result<void> Configure(Native::Device &device,
                                               const Native::Texture &source,
                                               FieldPageFeedbackOptions options);
  [[nodiscard]] Native::Result<bool> Encode(Native::Commands &commands, uint32_t phase);
  // Start after the producing frame submission on the same queue.
  [[nodiscard]] Native::Result<bool> BeginReadback();
  // At most one completed holder may remain with the consumer while the next is pending.
  [[nodiscard]] Native::Result<std::optional<ReadbackTransfer>> Take();

  [[nodiscard]] size_t Samples() const noexcept { return Samples_; }

  [[nodiscard]] uint64_t HeldBytes() const noexcept;

private:
  struct Recorded {};

  Native::Device *Device_ = nullptr;
  const Native::Texture *Source_ = nullptr;
  uint32_t Stride_ = 0;
  size_t Samples_ = 0;
  std::unique_ptr<Native::ComputePipeline> Pipeline_;
  std::unique_ptr<Native::Sampler> Sampling_;
  std::unique_ptr<Native::Buffer> Output_;
  std::variant<std::monostate, Recorded, ReadbackTransfer> State_;
};
}
#endif
