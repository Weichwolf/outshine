#ifndef OUTSHINE_RENDER_MATERIAL_MATERIALREGISTRATION_H
#define OUTSHINE_RENDER_MATERIAL_MATERIALREGISTRATION_H

#include <cstdint>
#include <memory>

namespace outshine::Render {
struct MaterialSource;

struct MaterialRegistration {
  uint32_t First = 0;
  std::shared_ptr<const MaterialSource> Source;
  [[nodiscard]] bool operator==(const MaterialRegistration &) const noexcept = default;
};
}
#endif
