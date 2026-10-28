#ifndef OUTSHINE_RENDER_MATERIAL_RADIANCEIMAGE_H
#define OUTSHINE_RENDER_MATERIAL_RADIANCEIMAGE_H

#include "native/Device.h"
#include <memory>

namespace outshine::Render {
// Premultiplied scene-linear radiance/coverage, consumed on the producing device.
struct RadianceImage {
  Native::Device *Device = nullptr;
  std::shared_ptr<Native::Texture> Texture;
  std::shared_ptr<Native::Sampler> Sampling;
  Native::Completion Produced;
};
}
#endif
