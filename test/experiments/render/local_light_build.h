#ifndef OUTSHINE_EXPERIMENT_LOCAL_LIGHT_BUILD_H
#define OUTSHINE_EXPERIMENT_LOCAL_LIGHT_BUILD_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <span>
#include <utility>
#include <vector>

namespace LightBuild {
#ifndef LOCAL_LIGHT_BUILD_COUNTER
#define LOCAL_LIGHT_BUILD_COUNTER 1
#endif
inline constexpr bool kWorkCounters = LOCAL_LIGHT_BUILD_COUNTER != 0;

inline void CountWork(uint64_t &counter) {
  if constexpr (kWorkCounters) { ++counter; }
}

using Vector = std::array<double, 3>;
using Light = std::array<float, 16>;
using Pair = std::pair<uint32_t, uint32_t>;
static_assert(sizeof(Light) == 64);
enum class Method : uint8_t { Sort, Scatter, Hierarchy };

struct Camera {
  std::array<double, 16> Values;
  std::array<uint32_t, 4> Grid;
  bool Perspective;
  [[nodiscard]] Vector View(const Light &light) const;
};

struct Bounds {
  std::array<int, 3> First{}, Last{};
  bool Empty = true;
};

[[nodiscard]] Bounds
Project(const Camera &camera, std::span<const double> edges, const Vector &position, double radius);
[[nodiscard]] bool Intersects(const Camera &camera,
                              std::span<const double> edges,
                              const Vector &position,
                              double radius,
                              const Bounds &cells);

class Memory final : public std::pmr::memory_resource {
public:
  size_t Allocations = 0, AllocatedBytes = 0, LiveBytes = 0, PeakBytes = 0;
  void Reset();

private:
  void *do_allocate(size_t bytes, size_t alignment) override;
  void do_deallocate(void *pointer, size_t bytes, size_t alignment) override;
  [[nodiscard]] bool do_is_equal(const memory_resource &other) const noexcept override;
};

struct Builder {
  Memory Heap;
  std::pmr::vector<Pair> Pairs{&Heap};
  std::pmr::vector<uint32_t> Counts{&Heap}, Cursor{&Heap}, Globals{&Heap}, Words{&Heap};
  uint64_t Tested = 0, BlockTests = 0, Assignments = 0;
  size_t GrowthRetainedBytes = 0;
  bool Overflow = false;

  void Build(std::span<const Light> lights,
             const Camera &camera,
             std::span<const double> edges,
             size_t indexBytes,
             Method method);

private:
  template <typename T> void RetainGrowth(const std::pmr::vector<T> &data, size_t needed) {
    if constexpr (kWorkCounters) {
      if (needed > data.capacity()) { GrowthRetainedBytes += data.size() * sizeof(T); }
    }
  }

  void Gather(std::span<const Light> lights,
              const Camera &camera,
              std::span<const double> edges,
              size_t indexBytes,
              Method method);
  void GatherLight(const Camera &camera,
                   std::span<const double> edges,
                   const Vector &position,
                   double radius,
                   Method method,
                   uint32_t light);
  void GatherBlock(const Camera &camera,
                   std::span<const double> edges,
                   uint32_t light,
                   const Bounds &cells,
                   const Vector &position,
                   double radius);
  void Pack(std::span<const double> edges, bool sort);
};
}
#endif
