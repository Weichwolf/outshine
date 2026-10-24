#include "Check.h"
#include "SurfaceProfile.h"
#include <array>
#include <cstdint>
#include <limits>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Generators;
using Point = std::array<double, 3>;
constexpr SurfaceProfilePlane kPlane{.Center = {.5, 0}, .Tangent = {1, 0}, .HalfSpanM = 2};

std::array<SurfaceProfileTriangle, 2> Faces() {
  return {{{{{0, -1, 4}, {2, -1, 8}, {2, 1, 14}}}, {{{0, -1, 4}, {2, 1, 14}, {0, 1, 10}}}}};
}

void WholeProfile() {
  const auto triangles = Faces();
  const auto profile = CutSurfaceProfile(triangles, kPlane);
  CHECK(profile.has_value(), "native cap triangles supply a complete transverse profile");
  if (!profile) { return; }
  const std::vector<Point> expected{{.5, -1, 5}, {.5, -.5, 6.5}, {.5, 0, 8}, {.5, 1, 11}};
  const std::vector<uint32_t> parents{0, 1, 1};
  CHECK(profile->Points == expected && profile->AxisVertex == 2,
        "the actual plane z=2*x+3*y+7 survives across the diagonal and source axis");
  CHECK(profile->Faces == parents,
        "each profile interval retains its original native face, including the inserted axis");
  CHECK(profile->Work.TriangleVisits == triangles.size() &&
            profile->Work.EdgeTests == 3 * triangles.size(),
        "the bounded input face and edge work is explicit");
  auto permuted = triangles;
  std::swap(permuted[0], permuted[1]);
  for (auto &triangle : permuted) { std::swap(triangle[0], triangle[2]); }
  const auto reordered = CutSurfaceProfile(permuted, kPlane);
  CHECK(reordered && reordered->Points == profile->Points,
        "triangle order and winding cannot change the physical profile");
  CHECK(reordered && reordered->Faces == std::vector<uint32_t>({1, 0, 0}),
        "face provenance follows the supplied order instead of a hidden sorted-face numbering");
  auto backwards = kPlane;
  backwards.Tangent[0] = -1;
  const auto reversed = CutSurfaceProfile(triangles, backwards);
  const std::vector<Point> opposite(expected.rbegin(), expected.rend());
  CHECK(reversed && reversed->Points == opposite && reversed->AxisVertex == 1,
        "reversing the qualified source frame reverses its physical profile consistently");
  auto clipped = kPlane;
  clipped.HalfSpanM = 1;
  const auto narrow = CutSurfaceProfile(triangles, clipped);
  CHECK(!narrow && narrow.error() == SurfaceProfileError::ClippedProfile,
        "a span ending at the surface edge is not a proof of a complete outside band");
  const auto bounded = CutSurfaceProfile(triangles, kPlane, {.Points = 3});
  CHECK(!bounded && bounded.error() == SurfaceProfileError::CapacityExceeded,
        "axis insertion cannot silently exceed the complete profile's vertex budget");
}

void InvalidChains() {
  const auto original = Faces();
  auto discontinuous = original;
  for (auto &point : discontinuous[1]) { point[2] += 1; }
  const auto gap = CutSurfaceProfile(discontinuous, kPlane);
  CHECK(!gap && gap.error() == SurfaceProfileError::DisconnectedProfile,
        "equal transverse offsets with different native heights are not welded");
  std::vector<SurfaceProfileTriangle> stacked(original.begin(), original.end());
  for (auto triangle : original) {
    for (auto &point : triangle) { point[2] += 1; }
    stacked.push_back(triangle);
  }
  const auto overlap = CutSurfaceProfile(stacked, kPlane);
  CHECK(!overlap && overlap.error() == SurfaceProfileError::OverlappingProfile,
        "two native layers cannot be replaced with an invented highest or average profile");
  auto bad = original;
  bad[0][1] = bad[0][0];
  const auto degenerate = CutSurfaceProfile(bad, kPlane);
  CHECK(!degenerate && degenerate.error() == SurfaceProfileError::InvalidInput,
        "a degenerate cap cannot enter the height cut");
  bad = original;
  bad[0][1][2] = std::numeric_limits<double>::infinity();
  const auto infinite = CutSurfaceProfile(bad, kPlane);
  CHECK(!infinite && infinite.error() == SurfaceProfileError::InvalidInput,
        "nonfinite source geometry is refused before classification");
  const auto capacity = CutSurfaceProfile(original, kPlane, {.Triangles = 1});
  CHECK(!capacity && capacity.error() == SurfaceProfileError::CapacityExceeded,
        "face work is bounded before allocation or traversal");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = CutSurfaceProfile(original, kPlane, {}, stop.get_token());
  CHECK(!canceled && canceled.error() == SurfaceProfileError::Canceled,
        "cancellation publishes no partial cut");
  auto outside = kPlane;
  outside.Center[0] = 3;
  const auto missing = CutSurfaceProfile(original, outside);
  CHECK(!missing && missing.error() == SurfaceProfileError::MissingSupport,
        "a cut outside the supplied faces cannot become a fabricated endpoint");
}

void EdgeAndFilteredFaces() {
  const auto original = Faces();
  auto plane = kPlane;
  plane.Center[0] = 2;
  plane.Tangent[0] = 2;
  const auto edge = CutSurfaceProfile(original, plane);
  const std::vector<Point> expected{{2, -1, 8}, {2, 0, 11}, {2, 1, 14}};
  CHECK(edge && edge->Points == expected && edge->AxisVertex == 1,
        "an existing boundary edge is cut completely, including its physical axis height");
  CHECK(edge && edge->Faces == std::vector<uint32_t>({0, 0}),
        "a face touching only one vertex does not invent another support interval");
  auto remote = original[0];
  for (auto &point : remote) { point[0] += 4; }
  const std::array<SurfaceProfileTriangle, 3> candidates{remote, original[0], original[1]};
  const auto filtered = CutSurfaceProfile(candidates, kPlane);
  CHECK(filtered && filtered->Faces == std::vector<uint32_t>({1, 2, 2}),
        "noncrossing candidates do not renumber retained original face provenance");
  auto missingBand = original;
  constexpr double kGapM = .125;
  for (auto &point : missingBand[1]) { point[1] += kGapM; }
  const auto gap = CutSurfaceProfile(missingBand, kPlane);
  CHECK(!gap && gap.error() == SurfaceProfileError::DisconnectedProfile,
        "a transverse coverage gap is rejected instead of bridged or resampled");
}
}

int main() {
  WholeProfile();
  InvalidChains();
  EdgeAndFilteredFaces();
  return outshine::Test::Report();
}
