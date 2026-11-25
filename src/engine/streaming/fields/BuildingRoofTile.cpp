#include "BuildingRoofTile.h"
#include "AssetCache.h"
#include "BuildingGeometrySource.h"
#include "BuildingRoofAssets.h"
#include "LevelOfDetail.h"
#include "Log.h"
#include "Sha256.h"
#include "SourceCoverage.h"
#include "SourceProductSelection.h"
#include "ViewPath.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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
using Cohort = BuildingRoofAssets::Cohort;
using Availability = PreparedBuildingAssets::SourceMember::State;

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

std::vector<Cohort> DescribeCohorts(std::span<const PreparedBuildingAssets::SourceMember> members,
                                    std::span<const Generators::BuildingRoofProxy> roofs) {
  std::vector<uint8_t> analytic(members.size());
  constexpr size_t cellCount = Generators::kStructureCellsPerTile;
  std::array<Cohort, cellCount> cells{};
  for (const auto &shape : roofs) {
    if (shape.Source >= analytic.size() || analytic[shape.Source] != 0) { return {}; }
    analytic[shape.Source] = 1;
  }
  for (size_t source = 0; source < members.size(); ++source) {
    const auto &member = members[source];
    if (member.Cell == 0 || member.Cell > cells.size()) { return {}; }
    auto &cell = cells[member.Cell - 1u];
    cell.Cell = member.Cell;
    if (analytic[source] != 0) {
      ++cell.Rows;
    } else if (member.Availability != Availability::Empty) {
      const double error = member.Availability == Availability::Surface && member.ShellErrorM
                               ? *member.ShellErrorM
                               : std::numeric_limits<double>::infinity();
      cell.ShellErrorM = std::max(cell.ShellErrorM, error);
    }
  }
  std::vector<Cohort> result;
  for (const auto &cell : cells) {
    if (cell.Rows != 0 && std::isfinite(cell.ShellErrorM)) { result.push_back(cell); }
  }
  return result;
}

std::optional<Cohort> AdmittedCell(std::span<const Cohort> directory, double shellErrorM) {
  Cohort best;
  for (const auto &entry : directory) {
    if (entry.ShellErrorM <= shellErrorM && entry.Rows > best.Rows) { best = entry; }
  }
  return best.Rows != 0 ? std::optional(best) : std::nullopt;
}

struct RoofCut {
  std::vector<Cohort> Directory;
  Cohort Selected;
  std::vector<Generators::BuildingRoofProxy> Rows;
};

constexpr size_t kCohortReads = 2;

std::optional<RoofCut> ReadCut(const AssetCache &cache,
                               const BuildingRoofAssets &directory,
                               size_t prefix,
                               std::span<const PreparedBuildingAssets::SourceMember> members,
                               double shellErrorM,
                               BuildingRoofTile::ReadCost &cost) {
  if (prefix > kCohortReads) {
    const auto cached = directory.ReadCohorts(cache, prefix);
    if (cached) {
      const auto selected = AdmittedCell(*cached, shellErrorM);
      if (!selected) { return std::nullopt; }
      auto rows = directory.ReadCohort(cache, prefix, *selected);
      if (rows) {
        ++cost.Cohorts;
        cost.DecodedRows += rows->size();
        return RoofCut{.Directory = {}, .Selected = *selected, .Rows = std::move(*rows)};
      }
    }
  }
  std::vector<Generators::BuildingRoofProxy> rows;
  for (size_t at = 0; at < prefix; ++at) {
    const auto block = directory.LoadBlock(cache, at);
    if (!block || !*block) { return std::nullopt; }
    ++cost.Blocks;
    cost.DecodedRows += (**block).size();
    rows.insert(rows.end(), (**block).begin(), (**block).end());
  }
  auto cohorts = DescribeCohorts(members, rows);
  const auto selected = AdmittedCell(cohorts, shellErrorM);
  if (!selected) { return std::nullopt; }
  std::erase_if(rows,
                [&](const auto &shape) { return members[shape.Source].Cell != selected->Cell; });
  return RoofCut{.Directory = std::move(cohorts), .Selected = *selected, .Rows = std::move(rows)};
}

std::optional<Split> SplitSources(BuildingRoofTile &product,
                                  const PreparedBuildingAssets::SourceGroups &groups,
                                  const std::string &key,
                                  const Cohort &selected,
                                  double shellErrorM) {
  if (groups.Members.size() > std::numeric_limits<uint32_t>::max()) { return std::nullopt; }
  Split result{.Roofs = {},
               .Remainder = {.BaseKey = key,
                             .SourceCount = groups.Members.size(),
                             .Sources = {},
                             .Detail = LevelOfDetail::Shell}};
  product.Cells = CellBit(selected.Cell);
  result.Roofs.reserve(product.Roofs.size());
  for (const auto &shape : product.Roofs) {
    if (shape.Source >= groups.Members.size() ||
        groups.Members[shape.Source].Cell != selected.Cell) {
      return std::nullopt;
    }
    result.Roofs.push_back(shape.Source);
  }
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
      const bool shell = member.Availability == Availability::Surface && member.ShellErrorM &&
                         *member.ShellErrorM <= shellErrorM;
      if (member.Availability != Availability::Empty && !shell) { return std::nullopt; }
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
                       ProjectedErrorBudget projection,
                       ReadCost &cost) {
  const double broadLimit = DiameterLimit(bounds, path, viewport, projection);
  if (!(broadLimit > 0) || groups.Members.empty()) { return std::nullopt; }
  const auto directory = BuildingRoofAssets::Resolve(cache, &assets, key);
  if (!directory || directory->SourceCount() != groups.Members.size()) { return std::nullopt; }
  const size_t prefix = directory->Prefix(broadLimit);
  if (prefix == 0) { return std::nullopt; }
  const double shellErrorM =
      SourceProductSelection::GeometryErrorLimitM(bounds, path, viewport, projection);
  auto cut = ReadCut(cache, *directory, prefix, groups.Members, shellErrorM, cost);
  if (!cut) { return std::nullopt; }
  BuildingRoofTile product{
      .Projected = {.Bounds = bounds,
                    .DiameterM = directory->Blocks()[prefix - 1].MaximumDiameterM},
      .Key = Sha256Hex("building-hybrid-roof-1/" + key + "/" + std::to_string(prefix)),
      .Anchor = directory->AnchorEcef(),
      .Roofs = std::move(cut->Rows),
      .Coverage = std::nullopt,
      .Remainder = std::nullopt,
      .Caster = std::nullopt};
  const auto split = SplitSources(product, groups, key, cut->Selected, shellErrorM);
  if (!split || !LoadRemainder(product, cache, assets, *split)) { return std::nullopt; }
  if (prefix > kCohortReads && !cut->Directory.empty()) {
    const auto stored =
        directory->PublishCohort(cache, prefix, cut->Directory, cut->Selected, product.Roofs);
    if (!stored && stored.error() != AssetCacheError::Busy) {
      Log::Debug(LogTag::World, "building_roof_cohort_deferred", {{"key", key}});
    }
  }
  product.Projected.ShellErrorM = split->ShellErrorM;
  product.Key = Sha256Hex(product.Key + "/" + std::to_string(product.Cells));
  const auto cell = static_cast<uint32_t>(std::countr_zero(product.Cells)) + 1u;
  auto caster = assets.LoadShadowCell(key, cell, cost.Caster);
  if (caster && caster->ErrorM <= SourceProductSelection::GeometryErrorLimitM(
                                      caster->Bounds, path, viewport, projection)) {
    product.Caster = std::move(caster);
  }
  return product;
}
}
