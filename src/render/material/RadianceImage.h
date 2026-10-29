#ifndef OUTSHINE_RENDER_MATERIAL_RADIANCEIMAGE_H
#define OUTSHINE_RENDER_MATERIAL_RADIANCEIMAGE_H

#include "native/Device.h"
#include <memory>
#include <cstdint>

namespace outshine::Render {
enum class RadianceEncoding : uint8_t { Texels, Integral };

// Premultiplied scene-linear radiance/coverage, consumed on the producing device.
struct RadianceImage {
  Native::Device *Device = nullptr;
  std::shared_ptr<Native::Texture> Texture;
  std::shared_ptr<Native::Sampler> Sampling;
  Native::Completion Produced;
  RadianceEncoding Encoding = RadianceEncoding::Texels;
};
}
#endif
