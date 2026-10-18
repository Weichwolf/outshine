#include "PreparedPlant.h"
#include "BinaryValueArchive.h"
#include "content/GeometryAsset.h"
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
constexpr uint64_t kFormat = 0x000131544e4c50ULL;
constexpr size_t kAxisBytes = sizeof(uint64_t) + 2 * sizeof(uint32_t);
constexpr size_t kLeafBytes = 2 * sizeof(uint64_t) + 16 * sizeof(double);

bool WriteLeaves(BinaryValueWriter &out, const PreparedPlantLeaves &group) {
  if (group.Ids.size() > std::numeric_limits<uint32_t>::max() ||
      !out(group.Variant, static_cast<uint32_t>(group.Ids.size()))) {
    return false;
  }
  for (size_t at = 0; at < group.Ids.size(); ++at) {
    if (!out(group.Ids[at], group.Axes[at], group.Transforms[at].Column)) { return false; }
  }
  return true;
}

bool ReadLeaves(BinaryValueReader &in, PreparedPlantLeaves &group) {
  uint32_t count = 0;
  if (!in(group.Variant, count) || count == 0 || count > in.In.Remaining() / kLeafBytes) {
    return false;
  }
  group.Ids.resize(count);
  group.Axes.resize(count);
  group.Transforms.resize(count);
  for (size_t at = 0; at < count; ++at) {
    if (!in(group.Ids[at], group.Axes[at], group.Transforms[at].Column)) { return false; }
  }
  return true;
}
}

std::expected<std::vector<uint8_t>, std::string_view>
EncodePreparedPlant(const PreparedPlant &plant, size_t bytesMost) {
  if (!PreparedPlantIsValid(plant) || plant.Axes.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected("prepared plant requires complete prototypes and owned identities");
  }
  const auto geometry = EncodeGeometryAsset(plant.Prototypes, bytesMost);
  if (!geometry) { return std::unexpected("prepared plant geometry exceeds encoded budget"); }
  BinaryValueWriter out(bytesMost);
  if (!out(kFormat,
           plant.PlanIdentity,
           plant.Root,
           static_cast<uint64_t>(geometry->size()),
           static_cast<uint32_t>(plant.Axes.size()),
           static_cast<uint32_t>(plant.Leaves.size())) ||
      !out.Out.Put(*geometry)) {
    return std::unexpected("prepared plant exceeds encoded budget");
  }
  for (const auto &axis : plant.Axes) {
    if (!out(axis.Id, axis.FirstIndex, axis.IndexCount)) {
      return std::unexpected("prepared plant exceeds encoded budget");
    }
  }
  for (const auto &group : plant.Leaves) {
    if (!WriteLeaves(out, group)) {
      return std::unexpected("prepared plant exceeds encoded budget");
    }
  }
  return std::move(out.Out).TakeBytes();
}

std::expected<PreparedPlant, std::string_view> DecodePreparedPlant(std::span<const uint8_t> bytes,
                                                                   size_t bytesMost) {
  if (bytes.size() > bytesMost) { return std::unexpected("prepared plant exceeds encoded budget"); }
  BinaryValueReader in(bytes);
  PreparedPlant result;
  uint64_t format = 0;
  uint64_t geometryBytes = 0;
  uint32_t axes = 0;
  uint32_t groups = 0;
  if (!in(format, result.PlanIdentity, result.Root, geometryBytes, axes, groups) ||
      format != kFormat || geometryBytes > in.In.Remaining() || axes == 0 ||
      groups > size_t{1} + std::numeric_limits<uint8_t>::max()) {
    return std::unexpected("prepared plant has an invalid header");
  }
  const auto geometry = in.In.Take(static_cast<size_t>(geometryBytes));
  if (!geometry || axes > in.In.Remaining() / kAxisBytes) {
    return std::unexpected("prepared plant has incomplete axis ranges");
  }
  auto decoded = DecodeGeometryAsset(*geometry, bytesMost);
  if (!decoded) { return std::unexpected("prepared plant has invalid native geometry"); }
  result.Prototypes = std::move(*decoded);
  result.Axes.resize(axes);
  for (auto &axis : result.Axes) {
    if (!in(axis.Id, axis.FirstIndex, axis.IndexCount)) {
      return std::unexpected("prepared plant has incomplete axis ranges");
    }
  }
  result.Leaves.resize(groups);
  for (auto &group : result.Leaves) {
    if (!ReadLeaves(in, group)) {
      return std::unexpected("prepared plant has incomplete leaf groups");
    }
  }
  if (in.In.Remaining() != 0 || !PreparedPlantIsValid(result)) {
    return std::unexpected("prepared plant requires complete prototypes and owned identities");
  }
  return result;
}
}
