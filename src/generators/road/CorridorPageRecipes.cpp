#include "Corridors.h"
#include "RoadLaneColour.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string_view>
#include <vector>
#include <utility>

namespace outshine::Generators {

RoadSweep Corridors::SurfaceOf(const Osm::StreetField::Way &lane,
                               const RegionSurfaceRecipes &vegetation,
                               const Edge &edge,
                               bool unbanked) {
  return {.HalfWidthM = static_cast<double>(lane.HalfWidthM),
          .Profile = ProfileOf(lane, vegetation),
          .WearsLinear = RoadLaneColour(lane.CoverRow, vegetation),
          .Crossfall = lane.Bridge && !unbanked ? std::atan(kCrossfall) : 0.0,
          .Form = lane.Bridge ? RibbonForm::ClosedShell : RibbonForm::Surface,
          .EndPorts = edge.EndPorts};
}

std::expected<bool, std::string_view>
Corridors::AdvancePlan(Job &job, const Site &site, size_t lanesMost, size_t nodesMost) const {
  if (!MatchesFunctionalInput(job, site) || job.Retirement != Job::RetireStage::Active ||
      (site.Vectors != nullptr ? site.Vectors->Generation() : 0) != job.VectorGeneration ||
      site.Ways.Ways().size() != job.WayCount) {
    return std::unexpected("road page plan inputs are stale or retired");
  }
  const auto &ways = job.RetainedWays ? *job.RetainedWays : site.Ways;
  if (site.Vectors != nullptr && !ways.SourceDigest(*site.Vectors, 0)) {
    return std::unexpected("road page plan source context differs");
  }
  if (job.Phase == Job::Stage::Pave) { return true; }
  if (job.Phase == Job::Stage::Done && job.Work.Edges.empty() && job.Work.Junctions.empty()) {
    return true;
  }
  if (job.Phase > Job::Stage::Pave) {
    return std::unexpected("road page planning must precede meshing");
  }
  Geometry unused;
  std::vector<EarthworkStamp> earthworks;
  std::vector<DiagnosticSample> notes;
  const auto advanced = Advance(job, site, lanesMost, nodesMost, unused, earthworks, notes);
  if (!advanced) { return std::unexpected(advanced.error()); }
  return *advanced || job.Phase == Job::Stage::Pave;
}

std::expected<std::vector<EarthworkStamp>, std::string_view>
Corridors::TakeTerrainContacts(Job &job) {
  const bool ready =
      job.Phase == Job::Stage::Pave ||
      (job.Phase == Job::Stage::Done && job.Work.Edges.empty() && job.Work.Junctions.empty());
  if (job.Retirement != Job::RetireStage::Active || !ready) {
    return std::unexpected("terrain contacts require a complete unmeshed road plan");
  }
  return std::exchange(job.Corridor, {});
}

std::expected<std::vector<RoadPagePart>, std::string_view> Corridors::PlannedParts(const Job &job) {
  if (job.Retirement != Job::RetireStage::Active || job.Phase < Job::Stage::Pave) {
    return std::unexpected("road page plan is not available");
  }
  const auto &work = job.Work;
  if (work.Edges.size() > std::numeric_limits<uint32_t>::max() ||
      work.Junctions.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected("road page plan exceeds its part identity range");
  }
  std::vector<RoadPagePart> parts;
  parts.reserve(work.Edges.size() + work.Junctions.size());
  for (size_t at = 0; at < work.Edges.size(); ++at) {
    if (work.Edges[at].PlannedCount < 2) { continue; }
    parts.push_back({.Kind = RoadPagePartKind::Interval, .Index = static_cast<uint32_t>(at)});
  }
  for (size_t at = 0; at < work.Junctions.size(); ++at) {
    parts.push_back({.Kind = RoadPagePartKind::Junction, .Index = static_cast<uint32_t>(at)});
  }
  return parts;
}

std::span<const DiagnosticSample> Corridors::PlanMeasures(const Job &job) noexcept {
  return job.Work.Notes;
}

std::expected<RoadPageRecipe, std::string_view>
Corridors::PageRecipe(const Job &job, const Site &site, std::span<const RoadPagePart> parts) {
  if (!MatchesFunctionalInput(job, site) || job.Retirement != Job::RetireStage::Active ||
      job.Phase < Job::Stage::Pave ||
      (site.Vectors != nullptr ? site.Vectors->Generation() : 0) != job.VectorGeneration ||
      site.Ways.Ways().size() != job.WayCount) {
    return std::unexpected("road page plan inputs are stale or unavailable");
  }
  const auto &ways = job.RetainedWays ? *job.RetainedWays : site.Ways;
  if (site.Vectors != nullptr && !ways.SourceDigest(*site.Vectors, 0)) {
    return std::unexpected("road page plan source context differs");
  }
  std::vector<RoadPagePart> ordered(parts.begin(), parts.end());
  std::ranges::sort(ordered, [](const auto &left, const auto &right) {
    return left.Kind != right.Kind ? left.Kind < right.Kind : left.Index < right.Index;
  });
  if (std::ranges::adjacent_find(ordered) != ordered.end()) {
    return std::unexpected("duplicate road page part request");
  }
  RoadPageRecipe recipe;
  const auto &work = job.Work;
  for (const auto part : ordered) {
    if (part.Kind == RoadPagePartKind::Junction && part.Index < work.Junctions.size()) {
      const auto &one = work.Junctions[part.Index];
      recipe.Junctions.push_back({.Index = part.Index,
                                  .Node = one.Node,
                                  .Gates = one.Gates,
                                  .Plane = {.SlopeE = one.SlopeE, .SlopeN = one.SlopeN},
                                  .WearsLinear = one.WearsLinear});
    } else if (part.Kind == RoadPagePartKind::Interval && part.Index < work.Edges.size()) {
      const auto &edge = work.Edges[part.Index];
      if (edge.PlannedCount < 2 || edge.PlannedFirst > work.Planned.size() ||
          edge.PlannedCount > work.Planned.size() - edge.PlannedFirst || site.Vectors == nullptr ||
          !ways.SourceOf(*site.Vectors, edge.Lane)) {
        return std::unexpected("road page interval lacks its complete original plan");
      }
      const auto from = work.Planned.begin() + edge.PlannedFirst;
      recipe.Intervals.push_back({.Edge = part.Index,
                                  .Lane = edge.Lane,
                                  .Stations = {from, from + edge.PlannedCount},
                                  .Surface = SurfaceOf(ways.Ways()[edge.Lane],
                                                       site.Vegetation,
                                                       edge,
                                                       work.UnbankedWays.contains(edge.Lane)),
                                  .StepBody = edge.StepBody});
    } else {
      return std::unexpected("unknown road page part request");
    }
  }
  return recipe;
}
}
