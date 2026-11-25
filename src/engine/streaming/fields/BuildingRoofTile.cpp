#include "BuildingRoofTile.h"
#include "AssetCache.h"
#include "BuildingGeometrySource.h"
#include "BuildingRoofAssets.h"
#include "LevelOfDetail.h"
#include "Sha256.h"
#include "SourceCoverage.h"
#include "SourceProductSelection.h"
#include "ViewPath.h"
#include <algorithm>
#include <array>
#include <iterator>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
namespace {
using Generators::BuildingGeometrySource;
using Generators::BuildingRoofAssets;
using Generators::PreparedBuildingAssets;

double DiameterLimit(const Box &bounds,
                     const ViewPath &path,
                     const Vec2 &viewport,
                     ProjectedErrorBudget projection) {
  constexpr double calibratedSupportPx = 16;
  const double support = std::min(calibratedSupportPx, 8 * projection.AllowedErrorPx);
  return SourceProductSelection::ProjectedSupportLimitM(
      bounds, path, viewport, projection.FocalPx, support);
}

struct Split {
  std::vector<uint32_t> Roofs;
  BuildingGeometrySource::Recipe Remainder;
  uint64_t Faces = 0;
  double ShellErrorM = 0;
};

constexpr uint64_t CellBit(uint32_t cell) {
  return uint64_t{1} << (cell - 1u);
}

std::optional<uint32_t> AdmittedCell(std::span<const PreparedBuildingAssets::SourceMember> members,
                                     std::span<const Generators::BuildingRoofProxy> roofs,
                                     double shellErrorM) {
  std::vector<uint8_t> analytic(members.size());
  for (const auto &shape : roofs) {
    if (shape.Source >= analytic.size()) { return std::nullopt; }
    analytic[shape.Source] = 1;
  }
  uint64_t unknown = 0;
  using Availability = PreparedBuildingAssets::SourceMember::State;
  for (size_t source = 0; source < members.size(); ++source) {
    const auto &member = members[source];
    if (member.Cell == 0 || member.Cell > 64) { return std::nullopt; }
    const bool verifiedShell = member.Availability == Availability::Surface && member.ShellErrorM &&
                               *member.ShellErrorM <= shellErrorM;
    if (analytic[source] == 0 && member.Availability != Availability::Empty && !verifiedShell) {
      unknown |= CellBit(member.Cell);
    }
  }
  constexpr size_t cellCount = std::numeric_limits<uint64_t>::digits;
  std::array<uint32_t, cellCount> counts{};
  for (const auto &shape : roofs) {
    const auto cell = members[shape.Source].Cell;
    if ((unknown & CellBit(cell)) == 0) { ++counts[cell - 1u]; }
  }
  auto *const best = std::ranges::max_element(counts);
  if (*best == 0) { return std::nullopt; }
  return static_cast<uint32_t>(std::distance(counts.begin(), best)) + 1u;
}

std::optional<Split> SplitSources(BuildingRoofTile &product,
                                  const PreparedBuildingAssets::SourceGroups &groups,
                                  const std::string &key,
                                  double shellErrorM) {
  if (groups.Members.size() > std::numeric_limits<uint32_t>::max()) { return std::nullopt; }
  Split result{.Roofs = {},
               .Remainder = {.BaseKey = key,
                             .SourceCount = groups.Members.size(),
                             .Sources = {},
                             .Detail = LevelOfDetail::Shell}};
  const auto chosen = AdmittedCell(groups.Members, product.Roofs, shellErrorM);
  if (!chosen) { return std::nullopt; }
  product.Cells = CellBit(*chosen);
  std::erase_if(product.Roofs,
                [&](const auto &shape) { return groups.Members[shape.Source].Cell != *chosen; });
  result.Roofs.reserve(product.Roofs.size());
  for (const auto &shape : product.Roofs) { result.Roofs.push_back(shape.Source); }
  std::ranges::sort(result.Roofs);
  if (result.Roofs.empty() || std::ranges::adjacent_find(result.Roofs) != result.Roofs.end()) {
    return std::nullopt;
  }
  std::vector<uint32_t> domain;
  auto next = result.Roofs.begin();
  for (uint32_t source = 0; source < groups.Members.size(); ++source) {
    const auto &member = groups.Members[source];
    if ((product.Cells & CellBit(member.Cell)) == 0) { continue; }
    domain.push_back(source);
    if (next != result.Roofs.end() && *next == source) {
      ++next;
    } else {
      result.Remainder.Sources.push_back(source);
      result.Faces += member.Faces;
      result.ShellErrorM = std::max(result.ShellErrorM, member.ShellErrorM.value_or(0));
    }
  }
  auto coverage = SourceCoverage::FromSources(key, groups.Members.size(), domain);
  if (!coverage) { return std::nullopt; }
  product.Coverage = std::move(*coverage);
  return result;
}

bool LoadRemainder(BuildingRoofTile &product,
                   AssetCache &cache,
                   PreparedBuildingAssets &assets,
                   const Split &split) {
  if (split.Remainder.Sources.empty()) { return true; }
  if (split.Faces == 0) { return true; }
  constexpr uint64_t attemptsPerFace = 16;
  constexpr size_t bytesPerAttempt = size_t{3} * 64;
  constexpr size_t headerBytes = 4096;
  if (split.Faces >
      (std::numeric_limits<size_t>::max() - headerBytes) / (attemptsPerFace * bytesPerAttempt)) {
    return false;
  }
  const uint64_t attempts = split.Faces * attemptsPerFace;
  auto remainder = BuildingGeometrySource::Resolve(
      cache,
      &assets,
      split.Remainder,
      {.EncodedBytesMost = headerBytes + static_cast<size_t>(attempts) * bytesPerAttempt,
       .TriangleAttempts = attempts,
       .KeepWhenCacheBusy = true});
  if (!remainder) { return false; }
  const auto roofs = SourceCoverage::FromSources(
      split.Remainder.BaseKey, split.Remainder.SourceCount, split.Roofs);
  if (!product.Coverage || !roofs || !remainder->Layout.Coverage) { return false; }
  const std::array<const SourceCoverage *, 2> coverage{&*roofs, &*remainder->Layout.Coverage};
  if (!SourceCoverage::Partition(*product.Coverage, coverage)) { return false; }
  product.Remainder = std::move(*remainder);
  return true;
}
}

bool BuildingRoofTile::Support::Allows(const Vec3 &eye,
                                       const Vec2 &viewport,
                                       ProjectedErrorBudget projection) const {
  const ViewPath point{.Start = eye, .End = eye};
  if (!(DiameterM > 0) || DiameterM > DiameterLimit(Bounds, point, viewport, projection)) {
    return false;
  }
  return ShellErrorM <=
         SourceProductSelection::GeometryErrorLimitM(Bounds, point, viewport, projection);
}

std::optional<BuildingRoofTile>
BuildingRoofTile::Load(AssetCache &cache,
                       PreparedBuildingAssets &assets,
                       const std::string &key,
                       const Box &bounds,
                       const PreparedBuildingAssets::SourceGroups &groups,
                       const ViewPath &path,
                       const Vec2 &viewport,
                       ProjectedErrorBudget projection) {
  const double broadLimit = DiameterLimit(bounds, path, viewport, projection);
  if (!(broadLimit > 0) || groups.Members.empty()) { return std::nullopt; }
  const auto directory = BuildingRoofAssets::Resolve(cache, &assets, key);
  if (!directory || directory->SourceCount() != groups.Members.size()) { return std::nullopt; }
  const size_t prefix = directory->Prefix(broadLimit);
  if (prefix == 0) { return std::nullopt; }
  BuildingRoofTile product{
      .Projected = {.Bounds = bounds,
                    .DiameterM = directory->Blocks()[prefix - 1].MaximumDiameterM},
      .Key = Sha256Hex("building-hybrid-roof-1/" + key + "/" + std::to_string(prefix)),
      .Anchor = directory->AnchorEcef(),
      .Roofs = {},
      .Coverage = std::nullopt,
      .Remainder = std::nullopt};
  for (size_t at = 0; at < prefix; ++at) {
    const auto block = directory->LoadBlock(cache, at);
    if (!block || !*block) { return std::nullopt; }
    product.Roofs.insert(product.Roofs.end(), (**block).begin(), (**block).end());
  }
  const double shellErrorM =
      SourceProductSelection::GeometryErrorLimitM(bounds, path, viewport, projection);
  const auto split = SplitSources(product, groups, key, shellErrorM);
  if (!split || !LoadRemainder(product, cache, assets, *split)) { return std::nullopt; }
  product.Projected.ShellErrorM = split->ShellErrorM;
  product.Key = Sha256Hex(product.Key + "/" + std::to_string(product.Cells));
  return product;
}
}
