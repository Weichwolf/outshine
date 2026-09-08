#ifndef OUTSHINE_RENDER_DEVICE_GROUNDSTORAGE_H
#define OUTSHINE_RENDER_DEVICE_GROUNDSTORAGE_H

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include "GpuOwned.h"
#include "GpuSubmission.h"

namespace outshine::Render {

class GroundStorage {
public:
  [[nodiscard]] std::expected<void, std::string> Replace(SDL_GPUDevice *device,
                                                         std::span<const uint32_t> classes,
                                                         std::span<const float> palette,
                                                         const GpuSubmission &submission = {});

  [[nodiscard]] SDL_GPUBuffer *Classes() const noexcept { return Classes_.Get(); }

  [[nodiscard]] SDL_GPUBuffer *Palette() const noexcept { return Palette_.Get(); }

  [[nodiscard]] bool Ready() const noexcept { return Classes_ && Palette_; }

private:
  OwnedBuffer Classes_;
  OwnedBuffer Palette_;
};

}
#endif
