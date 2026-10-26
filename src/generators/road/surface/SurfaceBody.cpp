#include "SurfaceBodyInternal.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators::SurfaceBodies {
namespace {
Checked<void> CutLevels(State &state) {
  const auto ranges = AssignLevels(state);
  if (!ranges) { return std::unexpected(ranges.error()); }
  const auto &output = state.Output;
  auto cut = CutHeightBands({.Points = output.ReferencePoints,
                             .Faces = output.ReferenceFaces,
                             .LevelRanges = *ranges,
                             .Levels = output.Levels},
                            state.Budget.Bands,
                            state.Stop);
  if (!cut) { return std::unexpected(cut.error()); }
  std::vector<uint64_t> owners;
  owners.reserve(cut->Faces.size());
  for (const auto parent : cut->FaceParents) { owners.push_back(output.FaceSources[parent]); }
  state.Output.ReferencePoints = std::move(cut->Points);
  state.Output.ReferenceFaces = std::move(cut->Faces);
  state.Output.FaceSources = std::move(owners);
  state.Output.FaceLevels = std::move(cut->LevelIndices);
  state.Output.Work.HeightCuts = cut->Work;
  return {};
}

Checked<void> Close(State &state) {
  const auto &output = state.Output;
  std::vector<PlanarPoint> xy;
  std::vector<double> bottoms;
  xy.reserve(output.ReferencePoints.size());
  bottoms.reserve(output.ReferencePoints.size());
  for (const auto &point : output.ReferencePoints) {
    xy.push_back({point[0], point[1]});
    bottoms.push_back(point[2] - state.Budget.ThicknessM);
  }
  std::vector<std::array<double, 3>> tops;
  tops.reserve(output.ReferenceFaces.size());
  for (size_t at = 0; at < output.ReferenceFaces.size(); ++at) {
    const auto face = output.ReferenceFaces[at];
    const auto level = output.FaceLevels[at];
    std::array<double, 3> heights{};
    for (size_t corner = 0; corner < face.size(); ++corner) {
      heights[corner] = level < 0 ? output.ReferencePoints[face[corner]][2]
                                  : output.Levels[static_cast<size_t>(level)];
    }
    tops.push_back(heights);
  }
  auto geometry = BuildHeightSolid(
      {.Points = xy, .Faces = output.ReferenceFaces, .TopHeights = tops, .BottomHeights = bottoms},
      state.Budget.Solid,
      state.Stop);
  if (!geometry) { return std::unexpected(geometry.error()); }
  state.Output.Geometry = std::move(*geometry);
  return {};
}
}
}

namespace outshine::Generators {
std::expected<SurfaceBody, SurfaceBodyError> BuildSurfaceBody(const SurfaceHeightRequest &request,
                                                              std::span<const double> heights,
                                                              SurfaceBodyBudget budget,
                                                              const std::stop_token &stop) {
  using namespace SurfaceBodies;
  State state{.Request = request,
              .Heights = heights,
              .Budget = budget,
              .Stop = stop,
              .Output = {},
              .Flights = {},
              .SourceFlights = {}};
  if (auto prepared = Prepare(state); !prepared) { return std::unexpected(prepared.error()); }
  if (auto envelope = Remesh(state, Pass::Envelope); !envelope) {
    return std::unexpected(envelope.error());
  }
  if (auto recipes = RefreshRecipes(state); !recipes) { return std::unexpected(recipes.error()); }
  if (auto cuts = CutLevels(state); !cuts) { return std::unexpected(cuts.error()); }
  if (auto bands = Remesh(state, Pass::Bands); !bands) { return std::unexpected(bands.error()); }
  Compact(state);
  if (auto body = Close(state); !body) { return std::unexpected(body.error()); }
  if (auto valid = state.Visit(0); !valid) { return std::unexpected(valid.error()); }
  return std::move(state.Output);
}
}
