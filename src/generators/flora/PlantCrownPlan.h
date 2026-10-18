#ifndef OUTSHINE_GENERATORS_FLORA_PLANTCROWNPLAN_H
#define OUTSHINE_GENERATORS_FLORA_PLANTCROWNPLAN_H

#include "PlantGrowth.h"
#include "PlantShoot.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Generators {

namespace PlantCrownDefaults {
inline constexpr double LeafAngleRad = 1.12, NormalAlignment = 0.3;
}

struct PlantCrownRecipe {
  uint64_t Seed = 0;
  double LengthM = 0, BendRad = 0, PetioleM = 0, FoliageDepthM = 0;
  double LeafAngleRad = PlantCrownDefaults::LeafAngleRad,
         NormalAlignment = PlantCrownDefaults::NormalAlignment;
};

struct PlantCrownLeaf {
  uint64_t Id = 0;
  uint32_t OwnerNode = 0;
  uint8_t Variant = 0;
  PlantLeafPose Pose;
};

class PlantCrownPlan {
public:
  PlantCrownPlan(const PlantCrownPlan &) = delete;
  PlantCrownPlan &operator=(const PlantCrownPlan &) = delete;
  PlantCrownPlan(PlantCrownPlan &&) noexcept = default;
  PlantCrownPlan &operator=(PlantCrownPlan &&) noexcept = default;
  ~PlantCrownPlan() = default;

  struct Summary {
    size_t Nodes = 0, Tips = 0, Shoots = 0, Leaves = 0, IndexBuildBytes = 0, IndexResidentBytes = 0;
    std::array<size_t, 64> Variants{};
  };

  struct LeafDemand {
    uint32_t Node = 0;
    size_t Maximum = 0;
  };

  [[nodiscard]] static std::expected<PlantCrownPlan, std::string_view>
  Create(PlantGrowthPlan growth,
         std::vector<uint8_t> variants,
         PlantCrownRecipe recipe,
         size_t maximumIndexBuildBytes);

  [[nodiscard]] const Summary &Summarize() const noexcept { return Summary_; }

  [[nodiscard]] std::span<const uint32_t> Hosts() const noexcept { return Hosts_; }

  [[nodiscard]] const PlantGrowthPlan &Skeleton() const noexcept { return Growth_; }

  [[nodiscard]] std::expected<size_t, std::string_view>
  MeasureLeaves(LeafDemand demand) const noexcept;
  [[nodiscard]] std::expected<std::vector<PlantCrownLeaf>, std::string_view>
  Leaves(LeafDemand demand) const;

private:
  friend std::expected<std::vector<uint8_t>, std::string_view>
  EncodePlantCrownPlan(const PlantCrownPlan &plan, size_t bytesMost);
  friend std::expected<PlantCrownPlan, std::string_view>
  DecodePlantCrownPlan(std::span<const uint8_t> bytes, size_t bytesMost);
  PlantCrownPlan() = default;
  [[nodiscard]] static bool ValidRecipe(PlantCrownRecipe recipe);
  [[nodiscard]] std::expected<size_t, std::string_view> HostIndex(LeafDemand demand) const noexcept;
  [[nodiscard]] size_t FirstLeaf(size_t host) const noexcept;
  [[nodiscard]] size_t LeafCount(size_t host) const noexcept;
  void IndexHosts();
  PlantGrowthPlan Growth_;
  std::vector<uint8_t> Variants_;
  std::vector<uint32_t> Hosts_;
  PlantCrownRecipe Recipe_;
  Summary Summary_;
};

}
#endif
