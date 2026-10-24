#include "HeightBandMesh.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <stop_token>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
const std::array kPoints{std::array{0., 0., 0.}, std::array{4., 0., 4.}, std::array{0., 4., 0.}};
const std::array<PlanarFace, 1> kFaces{PlanarFace{0, 1, 2}};
const std::array<double, 5> kLevels{0, 1, 2, 3, 4};
const std::array kRanges{HeightLevelRange{.First = 0, .Count = kLevels.size()}};

double ProjectedArea(const HeightBandMesh &mesh) {
  double area = 0;
  for (const auto face : mesh.Faces) {
    const auto &a = mesh.Points[face[0]];
    const auto &b = mesh.Points[face[1]];
    const auto &c = mesh.Points[face[2]];
    area += ((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) / 2;
  }
  return area;
}

void AdjacentBandsShareTheirOriginalEdgeIntersections() {
  const auto mesh = CutHeightBands(
      {.Points = kPoints, .Faces = kFaces, .LevelRanges = kRanges, .Levels = kLevels});
  CHECK(mesh && mesh->Points.size() == 9 && mesh->Faces.size() == 7,
        "three interior levels split two original edges into four indexed bands");
  if (!mesh) { return; }
  CHECK(mesh->Work.NewIntersections == 6 && mesh->Work.SearchComparisons == 0,
        "whole-range triangles reuse six crossings without binary searches");
  CHECK(mesh->FaceParents == std::vector<uint32_t>(7, 0) &&
            mesh->LevelIndices == std::vector<int32_t>({1, 1, 2, 2, 3, 3, 4}),
        "every output triangle retains its input parent and upper tread level");
  CHECK(ProjectedArea(*mesh) == 8, "the bands cover the whole input triangle exactly once");
  const std::array points{std::array{0., 0., 0.},
                          std::array{4., 0., 4.},
                          std::array{4., 4., 4.},
                          std::array{0., 4., 0.}};
  const std::array faces{PlanarFace{0, 1, 2}, PlanarFace{0, 2, 3}};
  const std::array ranges{kRanges[0], kRanges[0]};
  const auto square =
      CutHeightBands({.Points = points, .Faces = faces, .LevelRanges = ranges, .Levels = kLevels});
  CHECK(square && square->Points.size() == 13 && square->Work.NewIntersections == 9,
        "the common diagonal creates each source-edge/level crossing once across both faces");
  CHECK(square && ProjectedArea(*square) == 16,
        "shared cuts preserve the square's complete nonoverlapping projected area");
}

void SelectBandsBeforeClippingAndOwnFlatLevelsOnce() {
  auto points = kPoints;
  points[0][2] = points[2][2] = 2;
  points[1][2] = 3;
  constexpr size_t bands = 64;
  std::array<double, bands + 1> levels{};
  for (size_t at = 0; at < levels.size(); ++at) { levels[at] = static_cast<double>(at); }
  const std::array ranges{HeightLevelRange{.First = 0, .Count = levels.size()}};
  const auto narrow =
      CutHeightBands({.Points = points, .Faces = kFaces, .LevelRanges = ranges, .Levels = levels});
  CHECK(narrow && narrow->Faces.size() == 1 && narrow->Work.BandVisits == 3 &&
            narrow->Work.SkippedBands == 61,
        "a narrow height interval visits only its band and two zero-area boundary contacts");
  for (auto &point : points) { point[2] = 2; }
  const auto flat = CutHeightBands(
      {.Points = points, .Faces = kFaces, .LevelRanges = kRanges, .Levels = kLevels});
  CHECK(flat && flat->Faces.size() == 1 && flat->LevelIndices[0] == 2,
        "a face lying exactly on a level belongs to one tread, never both adjacent bands");
  const std::array continuous{HeightLevelRange{}};
  const auto unchanged =
      CutHeightBands({.Points = kPoints, .Faces = kFaces, .LevelRanges = continuous, .Levels = {}});
  CHECK(unchanged && unchanged->Faces[0] == kFaces[0] && unchanged->LevelIndices[0] == -1 &&
            unchanged->Work.EdgeVisits == 0,
        "unquantized surfaces retain their input triangle without clipping work");
}

void SeparateRecipeRangesDoNotNeedGlobalHeightOrdering() {
  const std::array points{std::array{0., 0., 0.},
                          std::array{4., 0., 2.},
                          std::array{0., 4., 0.},
                          std::array{0., 0., -3.},
                          std::array{4., 0., -1.},
                          std::array{0., 4., -3.}};
  const std::array faces{PlanarFace{0, 1, 2}, PlanarFace{3, 4, 5}};
  const std::array<double, 6> levels{0, 1, 2, -3, -2, -1};
  const std::array ranges{HeightLevelRange{.First = 0, .Count = 3},
                          HeightLevelRange{.First = 3, .Count = 3}};
  const auto mesh =
      CutHeightBands({.Points = points, .Faces = faces, .LevelRanges = ranges, .Levels = levels});
  CHECK(mesh && mesh->Work.LevelChecks == levels.size() && mesh->Work.NewIntersections == 4,
        "prefix validation checks sorted local ranges once and keeps coincident source sheets "
        "separate");
}

void InvalidInputsAndBudgetsNeverExposePartialMeshes() {
  const HeightBandInput input{
      .Points = kPoints, .Faces = kFaces, .LevelRanges = kRanges, .Levels = kLevels};
  for (size_t which = 0; which < 6; ++which) {
    HeightBandBudget budget;
    switch (which) {
      case 0: budget.InputPoints = 1; break;
      case 1: budget.InputFaces = 0; break;
      case 2: budget.Points = 5; break;
      case 3: budget.Faces = 2; break;
      case 4: budget.Levels = 1; break;
      default: budget.EdgeVisits = 1; break;
    }
    const auto refused = CutHeightBands(input, budget);
    CHECK(!refused && refused.error() == HeightBandError::CapacityExceeded,
          "input, output and edge budgets refuse the complete result");
  }
  auto levels = kLevels;
  levels[2] = levels[1];
  auto malformed = input;
  malformed.Levels = levels;
  const auto unordered = CutHeightBands(malformed);
  CHECK(!unordered && unordered.error() == HeightBandError::InvalidInput,
        "a selected level range must be strictly increasing");
  auto points = kPoints;
  points[1][2] = std::numeric_limits<double>::quiet_NaN();
  malformed = input;
  malformed.Points = points;
  const auto invalid = CutHeightBands(malformed);
  CHECK(!invalid && invalid.error() == HeightBandError::InvalidInput,
        "nonfinite input fields are rejected before intersection work");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = CutHeightBands(input, {}, stop.get_token());
  CHECK(!canceled && canceled.error() == HeightBandError::Canceled,
        "pre-cancellation prevents all mesh work");
  const auto empty = CutHeightBands({});
  CHECK(empty && empty->Faces.empty() && empty->Points.empty(), "empty input remains neutral");
}
}

int main() {
  AdjacentBandsShareTheirOriginalEdgeIntersections();
  SelectBandsBeforeClippingAndOwnFlatLevelsOnce();
  SeparateRecipeRangesDoNotNeedGlobalHeightOrdering();
  InvalidInputsAndBudgetsNeverExposePartialMeshes();
  return Report();
}
