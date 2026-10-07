#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGSURFACEBLOCK_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGSURFACEBLOCK_H

#include "BuildingSurface.h"
#include <expected>
#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

namespace outshine::Generators {

class BuildingSurfaceBlock {
public:
  using Load = std::function<std::expected<std::vector<BuildingSurface>, StructureMeshError>()>;

  explicit BuildingSurfaceBlock(Load load) : Load_(std::move(load)) {}

  [[nodiscard]] std::expected<void, StructureMeshError> Require();
  [[nodiscard]] const BuildingSurface &At(uint32_t index) const noexcept;

private:
  enum class State : uint8_t { Unloaded, Ready, Failed };
  std::atomic<State> State_{State::Unloaded};
  std::mutex Lock_;
  Load Load_;
  std::optional<std::expected<std::vector<BuildingSurface>, StructureMeshError>> Models_;
};

}
#endif
