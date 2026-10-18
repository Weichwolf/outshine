#include "PlantCrownAsset.h"
#include "PlantCrownPlan.h"
#include "PlantGrowth.h"
#include "PlantSkeleton.h"
#include "BinaryValueArchive.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint64_t kFormat = 0x000131574f524350ULL;
constexpr size_t kStatistics = 8;
constexpr size_t kHeaderBytes = 2 * sizeof(uint64_t) + 6 * sizeof(uint32_t) + 6 * sizeof(double) +
                                kStatistics * sizeof(uint64_t);
constexpr size_t kNodeWireBytes = 3 * sizeof(double) + sizeof(int32_t);
constexpr size_t kMaximumLeaves = 100000;

struct Counts {
  uint32_t Nodes = 0, Hosts = 0, Leaves = 0, Unreached = 0, Tips = 0;
};

bool Consume(size_t &remaining, size_t count, size_t unit) {
  if (count > remaining / unit) { return false; }
  remaining -= count * unit;
  return true;
}

struct ByteLimits {
  size_t Encoded = 0, Decoded = 0;
};

bool Fits(Counts counts, ByteLimits limits) {
  if (limits.Encoded < kHeaderBytes || counts.Nodes < 2 ||
      counts.Nodes > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
      counts.Hosts == 0 || counts.Hosts >= counts.Nodes || counts.Leaves > kMaximumLeaves ||
      counts.Tips == 0 || counts.Tips > counts.Hosts) {
    return false;
  }
  size_t remaining = limits.Encoded - kHeaderBytes;
  size_t decoded = limits.Decoded;
  return Consume(remaining, counts.Nodes, kNodeWireBytes) &&
         Consume(remaining, counts.Hosts, sizeof(uint32_t)) &&
         Consume(remaining, counts.Leaves, sizeof(uint8_t)) &&
         Consume(remaining, counts.Unreached, sizeof(uint32_t)) && remaining == 0 &&
         Consume(decoded, counts.Nodes, sizeof(PlantGrowthPoint) + sizeof(int32_t)) &&
         Consume(decoded, counts.Hosts, sizeof(uint32_t)) &&
         Consume(decoded, counts.Leaves, sizeof(uint8_t)) &&
         Consume(decoded, counts.Unreached, sizeof(uint32_t));
}

bool Header(BinaryValueWriter &out,
            const PlantCrownRecipe &recipe,
            Counts counts,
            const PlantGrowthPlan &growth) {
  const auto &stats = growth.Stats;
  const std::array<uint64_t, kStatistics> encoded{stats.Iterations,
                                                  stats.Consumed,
                                                  stats.RejectedEndpoints,
                                                  stats.WorkingBytes,
                                                  stats.NearestQueries,
                                                  stats.RadiusQueries,
                                                  stats.DistanceChecks,
                                                  stats.PartitionComparisons};
  return out(kFormat,
             counts.Nodes,
             counts.Hosts,
             counts.Leaves,
             counts.Unreached,
             counts.Tips,
             recipe.Seed,
             recipe.LengthM,
             recipe.BendRad,
             recipe.PetioleM,
             recipe.FoliageDepthM,
             recipe.LeafAngleRad,
             recipe.NormalAlignment,
             growth.Stop,
             encoded);
}

bool Header(BinaryValueReader &in,
            PlantCrownRecipe &recipe,
            Counts &counts,
            PlantGrowthPlan &growth) {
  uint64_t format = 0;
  std::array<uint64_t, kStatistics> stats{};
  if (!in(format,
          counts.Nodes,
          counts.Hosts,
          counts.Leaves,
          counts.Unreached,
          counts.Tips,
          recipe.Seed,
          recipe.LengthM,
          recipe.BendRad,
          recipe.PetioleM,
          recipe.FoliageDepthM,
          recipe.LeafAngleRad,
          recipe.NormalAlignment,
          growth.Stop,
          stats) ||
      format != kFormat ||
      std::to_underlying(growth.Stop) > std::to_underlying(PlantGrowthStop::OccupiedEndpoints) ||
      std::ranges::any_of(std::span(stats).first(4), [](uint64_t value) {
        return value > std::numeric_limits<size_t>::max();
      })) {
    return false;
  }
  growth.Stats = {.Iterations = static_cast<size_t>(stats[0]),
                  .Consumed = static_cast<size_t>(stats[1]),
                  .RejectedEndpoints = static_cast<size_t>(stats[2]),
                  .WorkingBytes = static_cast<size_t>(stats[3]),
                  .NearestQueries = stats[4],
                  .RadiusQueries = stats[5],
                  .DistanceChecks = stats[6],
                  .PartitionComparisons = stats[7]};
  return true;
}

bool ReadArrays(BinaryValueReader &in,
                PlantGrowthPlan &growth,
                std::vector<uint32_t> &hosts,
                std::vector<uint8_t> &variants,
                Counts counts) {
  growth.PositionsM.resize(counts.Nodes);
  growth.Parents.resize(counts.Nodes);
  for (size_t row = 0; row < counts.Nodes; ++row) {
    if (!in(growth.PositionsM[row], growth.Parents[row])) { return false; }
  }
  hosts.resize(counts.Hosts);
  for (auto &host : hosts) {
    if (!in(host)) { return false; }
  }
  variants.resize(counts.Leaves);
  for (auto &variant : variants) {
    if (!in(variant)) { return false; }
  }
  growth.Unreached.resize(counts.Unreached);
  for (auto &source : growth.Unreached) {
    if (!in(source)) { return false; }
  }
  return true;
}
}

std::expected<std::vector<uint8_t>, std::string_view>
EncodePlantCrownPlan(const PlantCrownPlan &plan, size_t bytesMost) {
  const auto &growth = plan.Growth_;
  if (growth.PositionsM.size() > std::numeric_limits<uint32_t>::max() ||
      growth.Unreached.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected("crown plan exceeds native count representation");
  }
  const Counts counts{.Nodes = static_cast<uint32_t>(growth.PositionsM.size()),
                      .Hosts = static_cast<uint32_t>(plan.Hosts_.size()),
                      .Leaves = static_cast<uint32_t>(plan.Variants_.size()),
                      .Unreached = static_cast<uint32_t>(growth.Unreached.size()),
                      .Tips = static_cast<uint32_t>(plan.Summary_.Tips)};
  size_t remaining = bytesMost;
  if (!Consume(remaining, 1, kHeaderBytes) || !Consume(remaining, counts.Nodes, kNodeWireBytes) ||
      !Consume(remaining, counts.Hosts, sizeof(uint32_t)) ||
      !Consume(remaining, counts.Leaves, sizeof(uint8_t)) ||
      !Consume(remaining, counts.Unreached, sizeof(uint32_t))) {
    return std::unexpected("crown plan exceeds encoded budget");
  }
  if (!Fits(counts, {.Encoded = bytesMost - remaining, .Decoded = bytesMost})) {
    return std::unexpected("crown plan has no complete stored index");
  }
  BinaryValueWriter out(bytesMost);
  out.Out.Reserve(bytesMost - remaining);
  if (!Header(out, plan.Recipe_, counts, growth)) {
    return std::unexpected("crown header is not writable");
  }
  for (size_t node = 0; node < growth.PositionsM.size(); ++node) {
    if (!out(growth.PositionsM[node], growth.Parents[node])) {
      return std::unexpected("crown nodes are not writable");
    }
  }
  for (const uint32_t host : plan.Hosts_) {
    if (!out(host)) { return std::unexpected("crown hosts are not writable"); }
  }
  if (!out.Out.Put(plan.Variants_)) { return std::unexpected("crown variants are not writable"); }
  for (const uint32_t source : growth.Unreached) {
    if (!out(source)) { return std::unexpected("crown provenance is not writable"); }
  }
  return std::move(out.Out).TakeBytes();
}

std::expected<PlantCrownPlan, std::string_view> DecodePlantCrownPlan(std::span<const uint8_t> bytes,
                                                                     size_t bytesMost) {
  if (bytes.size() > bytesMost) { return std::unexpected("crown plan exceeds encoded budget"); }
  BinaryValueReader in(bytes);
  PlantCrownPlan result;
  Counts counts;
  if (!Header(in, result.Recipe_, counts, result.Growth_) ||
      !Fits(counts, {.Encoded = bytes.size(), .Decoded = bytesMost}) ||
      !PlantCrownPlan::ValidRecipe(result.Recipe_)) {
    return std::unexpected("crown plan has an invalid header or decoded budget");
  }
  if (!ReadArrays(in, result.Growth_, result.Hosts_, result.Variants_, counts) ||
      in.In.Remaining() != 0 ||
      !ValidPlantSkeleton(result.Growth_.PositionsM, result.Growth_.Parents) ||
      !std::ranges::all_of(result.Variants_, [](uint8_t variant) { return variant < 64; }) ||
      !std::ranges::is_sorted(result.Hosts_) ||
      std::ranges::adjacent_find(result.Hosts_) != result.Hosts_.end() ||
      result.Hosts_.front() == 0 || result.Hosts_.back() >= counts.Nodes) {
    return std::unexpected("crown plan has invalid stored arrays or host identities");
  }
  result.Summary_.Nodes = counts.Nodes;
  result.Summary_.Tips = counts.Tips;
  result.Summary_.Shoots = counts.Hosts;
  result.Summary_.Leaves = counts.Leaves;
  result.Summary_.IndexResidentBytes = result.Hosts_.capacity() * sizeof(uint32_t);
  for (const uint8_t variant : result.Variants_) { ++result.Summary_.Variants[variant]; }
  return result;
}
}
