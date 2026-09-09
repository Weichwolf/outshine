#ifndef OUTSHINE_ENGINE_WORLDINSTANCESINK_H
#define OUTSHINE_ENGINE_WORLDINSTANCESINK_H

#include "DrawSink.h"
#include "WorldPlacement.h"
#include <cstddef>
#include <optional>
#include <span>
#include <type_traits>

namespace outshine {

inline constexpr size_t kMaxGeneratedInstances = 1u << 20u;
enum class InstanceWriteError { Capacity };

class WorldInstanceSink final : public Generators::DrawSink {
public:
  WorldInstanceSink(std::span<WorldInstance> output, const Generators::Tile &region)
      : Output_(output), Region_(region) {}

  [[nodiscard]] bool Add(Generators::BodyId body,
                         Generators::ClusterId cluster,
                         const Generators::Scattered &instance) noexcept override {
    if (Full()) {
      Error_ = InstanceWriteError::Capacity;
      return false;
    }
    Output_[Written_] = {.Body = body.Index(),
                         .Cluster = static_cast<uint32_t>(cluster),
                         .Where = WorldPlacement::From(Region_, instance)};
    ++Written_;
    return true;
  }

  [[nodiscard]] bool Full() const noexcept override { return Written_ == Output_.size(); }

  [[nodiscard]] size_t Written() const noexcept { return Written_; }

  [[nodiscard]] std::optional<InstanceWriteError> Error() const noexcept { return Error_; }

private:
  std::span<WorldInstance> Output_;
  const Generators::Tile &Region_;
  size_t Written_ = 0;
  std::optional<InstanceWriteError> Error_;
};

static_assert(std::is_nothrow_copy_assignable_v<WorldInstance>);

}
#endif
