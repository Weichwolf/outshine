#ifndef OUTSHINE_GENERATORS_FLORA_PLANTPLAN_H
#define OUTSHINE_GENERATORS_FLORA_PLANTPLAN_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace outshine::Generators {

struct PlantAxis {
  uint64_t Id = 0;
  int64_t Parent = -1;
  std::array<int16_t, 7> Path{-1, -1, -1, -1, -1, -1, -1};
  std::array<double, 3> OriginM{};
  std::array<double, 9> Frame{};
  double LengthM = 0;
  double Bend = 0;
  double ParentParameter = 0;
  std::array<double, 2> RadiiM{};
};

struct PlantLeaf {
  uint64_t Id = 0;
  uint64_t Axis = 0;
  uint8_t Variant = 0;
  std::array<double, 3> OriginM{}, NodeM{};
  std::array<double, 9> Frame{};
  double StemParameter = 0;
};

class PlantPlan {
public:
  struct LeafDemand {
    uint64_t Axis = 0;
    size_t Maximum = 0;
  };

  struct Summary {
    size_t Axes = 0, Leaves = 0;
    std::span<const size_t> Variants;
    std::span<const size_t> AxisRows;
  };

  [[nodiscard]] static std::expected<PlantPlan, std::string_view>
  Create(std::vector<PlantAxis> axes, std::vector<PlantLeaf> leaves, size_t variants);
  [[nodiscard]] const PlantAxis *Axis(uint64_t id) const noexcept;
  [[nodiscard]] std::expected<Summary, std::string_view> Summarize(uint64_t id) const noexcept;
  [[nodiscard]] std::expected<std::span<const PlantLeaf>, std::string_view>
  Leaves(LeafDemand demand) const noexcept;

  [[nodiscard]] std::span<const PlantAxis> Axes() const noexcept { return Axes_; }

private:
  struct Index {
    size_t Parent = 0, FirstLeaf = 0, LeafCount = 0, BranchBegin = 0;
    size_t Axes = 1, Leaves = 0;
  };

  [[nodiscard]] bool IndexAxes();
  [[nodiscard]] bool IndexLeaves(size_t variants);
  std::vector<PlantAxis> Axes_;
  std::vector<PlantLeaf> Leaves_;
  std::vector<Index> Index_;
  std::vector<size_t> Traversal_;
  std::unordered_map<uint64_t, size_t> ById_;
  std::vector<size_t> Variants_;
  size_t VariantCount_ = 0;
};

}
#endif
