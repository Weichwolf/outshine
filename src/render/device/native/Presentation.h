#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_PRESENTATION_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_PRESENTATION_H

#include "Resources.h"

namespace outshine::Render::Native {

class PresentFrame {
public:
  virtual ~PresentFrame() = default;
  PresentFrame(const PresentFrame &) = delete;
  PresentFrame &operator=(const PresentFrame &) = delete;

  [[nodiscard]] virtual const Texture &Image() const noexcept = 0;

protected:
  PresentFrame() = default;
};

}
#endif
