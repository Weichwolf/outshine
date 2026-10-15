#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_COMPUTE_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_COMPUTE_H

#include "Bindings.h"
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace outshine::Render::Native {

struct ComputeLayout {
  std::array<uint32_t, 3> LocalSize = {1, 1, 1};
  std::vector<ShaderBinding> Bindings;
};

class ComputePipeline {
public:
  virtual ~ComputePipeline() = default;
  ComputePipeline(const ComputePipeline &) = delete;
  ComputePipeline &operator=(const ComputePipeline &) = delete;

  [[nodiscard]] const ComputeLayout &Layout() const noexcept { return Layout_; }

protected:
  explicit ComputePipeline(ComputeLayout layout) : Layout_(std::move(layout)) {}

private:
  ComputeLayout Layout_;
};

struct Workgroups {
  uint32_t X = 1;
  uint32_t Y = 1;
  uint32_t Z = 1;
};

}
#endif
