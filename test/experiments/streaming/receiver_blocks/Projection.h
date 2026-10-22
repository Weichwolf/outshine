#ifndef OUTSHINE_EXPERIMENT_RECEIVER_BLOCK_PROJECTION_H
#define OUTSHINE_EXPERIMENT_RECEIVER_BLOCK_PROJECTION_H
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace ReceiverBlocks {
using Matrix = std::array<double, 16>;

struct Interval {
  double Lo;
  double Hi;
};

using Domain = std::array<Interval, 3>;

struct Light {
  std::array<std::array<Interval, 4>, 3> Numerator;
  std::array<double, 3> Error;
};

class Prepared {
public:
  [[nodiscard]] static std::optional<Prepared> Make(const Matrix &inverse,
                                                    const std::array<double, 3> &eye,
                                                    std::span<const Matrix> lights,
                                                    const Domain &frame) noexcept;
  [[nodiscard]] bool Project(const Domain &box, std::span<Domain> output) const noexcept;

  [[nodiscard]] size_t LightCount() const noexcept { return Lights_.size(); }

private:
  std::vector<Light> Lights_;
  std::array<double, 4> Denominator_{};
  Domain Frame_{};
};
}
#endif
