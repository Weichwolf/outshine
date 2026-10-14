#include "scene/SurfaceSampler.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using Triangle = std::array<uint32_t, 3>;
constexpr std::array<Vec3, 4> Vertices{
    {{{0, 0, 10}}, {{2, 0, 10.2}}, {{2, 2, 10.6}}, {{0, 2, 10.4}}}};
constexpr std::array<Triangle, 2> Faces{{{0, 1, 2}, {0, 2, 3}}};

void AffineContacts() {
  auto sampler = SurfaceSampler::prepare(Vertices, Faces);
  CHECK(sampler.has_value(), "finite affine surface prepares");
  if (!sampler) { return; }
  constexpr std::array<Vec2, 5> xy{{{{.5, .5}}, {{1, 1}}, {{0, 0}}, {{1.8, .2}}, {{.2, 1.8}}}};
  auto samples = sampler->sample(xy);
  CHECK(samples && samples->Contacts.size() == xy.size(), "query retains input order");
  if (!samples) { return; }
  constexpr std::array<int64_t, 5> expectedFaces{0, 0, 0, 0, 1};
  const Vec3 normal{{-.1 / std::sqrt(1.05), -.2 / std::sqrt(1.05), 1 / std::sqrt(1.05)}};
  for (size_t at = 0; at < xy.size(); ++at) {
    const auto &contact = samples->Contacts[at];
    CHECK(contact.Status == SurfaceContactStatus::Contact && contact.FaceIndex == expectedFaces[at],
          "shared topological edge and vertex select lowest face");
    CHECK(std::abs(contact.PositionM[2] - (10 + .1 * xy[at][0] + .2 * xy[at][1])) < 1e-12,
          "contact height matches analytic plane");
    CHECK(std::hypot(contact.Normal[0] - normal[0],
                     contact.Normal[1] - normal[1],
                     contact.Normal[2] - normal[2]) < 1e-12,
          "contact carries upward unit facet normal");
  }
  auto moved = std::move(*sampler);
  CHECK(moved.sample(xy).has_value(), "prepared ownership survives move");
  auto reversed = Faces;
  for (auto &face : reversed) { std::swap(face[0], face[2]); }
  auto shifted = Vertices;
  for (auto &vertex : shifted) { vertex = vertex + Vec3{{1e6, -2e6, 5}}; }
  auto shiftedSampler = SurfaceSampler::prepare(shifted, reversed);
  const std::array<Vec2, 1> shiftedXy{{{{1e6 + 1, -2e6 + 1}}}};
  auto shiftedSamples = shiftedSampler->sample(shiftedXy);
  CHECK(shiftedSamples && shiftedSamples->Contacts[0].FaceIndex == 0 &&
            shiftedSamples->Contacts[0].Normal[2] > 0 &&
            std::abs(shiftedSamples->Contacts[0].PositionM[2] - 15.3) < 1e-12,
        "reversed winding and large local translation preserve contacts");
  auto creasedVertices = Vertices;
  creasedVertices[3][2] += 1;
  auto creased = SurfaceSampler::prepare(creasedVertices, Faces);
  const std::array<Vec2, 2> facets{{{{1, 1}}, {{.1, 1.9}}}};
  auto creasedSamples = creased->sample(facets);
  CHECK(creasedSamples && creasedSamples->Contacts[0].FaceIndex == 0 &&
            creasedSamples->Contacts[1].FaceIndex == 1 &&
            std::abs(creasedSamples->Contacts[0].Normal[0] -
                     creasedSamples->Contacts[1].Normal[0]) > .01,
        "continuous shared height retains different facet normals without claiming C1");
}

void UnresolvedContacts() {
  auto sampler = SurfaceSampler::prepare(Vertices, Faces, {.XyToleranceM = 1e-6});
  constexpr std::array<Vec2, 2> outside{{{{-1e-7, 1}}, {{-1e-4, 1}}}};
  auto samples = sampler->sample(outside);
  CHECK(samples && samples->Contacts[0].Status == SurfaceContactStatus::Contact &&
            std::abs(samples->Contacts[0].PositionM[2] - (10.2 - 1e-8)) < 1e-12,
        "metric tolerance permits bounded affine extrapolation");
  CHECK(samples && samples->Contacts[1].Status == SurfaceContactStatus::Outside &&
            std::isnan(samples->Contacts[1].PositionM[2]) &&
            std::isnan(samples->Contacts[1].Normal[2]) && samples->Contacts[1].FaceIndex == -1,
        "outside surface never supplies a fabricated zero height");
  constexpr std::array<Triangle, 2> stackedFaces{{{0, 1, 2}, {3, 4, 5}}};
  for (double offset : {0.0, 5.0}) {
    const std::array<Vec3, 6> vertices{Vertices[0],
                                       Vertices[1],
                                       Vertices[2],
                                       Vertices[0] + Vec3{{0, 0, offset}},
                                       Vertices[1] + Vec3{{0, 0, offset}},
                                       Vertices[2] + Vec3{{0, 0, offset}}};
    auto stacked = SurfaceSampler::prepare(vertices, stackedFaces);
    constexpr std::array<Vec2, 2> xy{{{{1.5, .5}}, {{1, 1}}}};
    auto result = stacked->sample(xy);
    CHECK(result.has_value(), "stacked surface is valid input");
    for (const auto &contact : result->Contacts) {
      CHECK(contact.Status == SurfaceContactStatus::Ambiguous && contact.FaceIndex == -1 &&
                std::isnan(contact.PositionM[2]),
            "unshared coincident or stacked faces remain unresolved");
    }
  }
}

void LimitsAndInvalidInput() {
  auto sampler = SurfaceSampler::prepare(Vertices, Faces);
  constexpr std::array<Vec2, 3> xy{{{{1.8, .2}}, {{.2, 1.8}}, {{1, 1}}}};
  auto result = sampler->sample(xy, {.MaxCandidates = 2});
  CHECK(result && result->Candidates == 1 && result->Batches == 1 &&
            result->Contacts[0].Status == SurfaceContactStatus::Contact &&
            result->Contacts[1].Status == SurfaceContactStatus::CandidateBudget &&
            result->Contacts[2].Status == SurfaceContactStatus::CandidateBudget,
        "conservative budget preserves earlier contacts and bounds batches");
  CHECK(!sampler->sample(xy, {.MaxQueries = 2}), "query cap rejects oversized request");
  CHECK(!SurfaceSampler::prepare(Vertices, Faces, {.MaxFaces = 1}),
        "face cap precedes index build");
  CHECK(!SurfaceSampler::prepare(Vertices, Faces, {.MaxVertices = 3}), "vertex cap bounds copies");
  const std::array<Vec2, 1> invalid{{{{0, std::numeric_limits<double>::quiet_NaN()}}}};
  CHECK(!sampler->sample(invalid), "nonfinite queries fail explicitly");
  const std::array<Triangle, 2> duplicate{{{0, 1, 2}, {2, 0, 1}}};
  CHECK(!SurfaceSampler::prepare(Vertices, duplicate), "duplicate topological faces are rejected");
  const std::array<Triangle, 1> broken{{{0, 1, 4}}};
  CHECK(!SurfaceSampler::prepare(Vertices, broken), "invalid vertex index cannot enter GEOS");
  const std::array<Triangle, 1> single{{{0, 1, 2}}};
  const std::array invalidMeshes{
      std::array<Vec3, 3>{{{{0, 0, 0}}, {{1, 0, 0}}, {{2, 0, 0}}}},
      std::array<Vec3, 3>{{{{0, 0, 0}}, {{1, 0, 0}}, {{1, 0, 2}}}},
      std::array<Vec3, 3>{{{{0, 0, 0}}, {{1e200, 0, 0}}, {{0, 1e200, 1}}}}};
  for (const auto &vertices : invalidMeshes) {
    CHECK(!SurfaceSampler::prepare(vertices, single),
          "degenerate vertical or overflowing face rejects");
  }
  auto empty = SurfaceSampler::prepare({}, {});
  auto absent = empty->sample(xy);
  CHECK(absent && absent->Candidates == 0 && absent->Batches == 0 &&
            absent->Contacts[0].Status == SurfaceContactStatus::Outside,
        "empty chosen surface supplies no height");
  auto exact = SurfaceSampler::prepare(Vertices, Faces, {.XyToleranceM = 0});
  CHECK(exact->sample(xy)->Contacts[2].FaceIndex == 0, "zero tolerance includes shared edge");
  CHECK(sampler->sample({})->Contacts.empty(), "empty request is valid");
}

void IndexedBatch() {
  constexpr uint32_t side = 64;
  std::vector<Vec3> vertices;
  std::vector<Triangle> faces;
  for (uint32_t y = 0; y <= side; ++y) {
    for (uint32_t x = 0; x <= side; ++x) {
      vertices.push_back({{double(x), double(y), 10 + .1 * x + .2 * y}});
    }
  }
  for (uint32_t y = 0; y < side; ++y) {
    for (uint32_t x = 0; x < side; ++x) {
      const uint32_t a = y * (side + 1) + x;
      faces.push_back({a, a + 1, a + side + 2});
      faces.push_back({a, a + side + 2, a + side + 1});
    }
  }
  auto sampler = SurfaceSampler::prepare(vertices, faces);
  CHECK(sampler.has_value(), "indexed 8192-face fixture prepares");
  if (!sampler) { return; }
  vertices.clear();
  faces.clear();
  std::vector<Vec2> queries;
  for (uint32_t at = 0; at < 22000; ++at) {
    queries.push_back({{double((at * 37) % side) + .75, double((at * 11) % side) + .25}});
  }
  auto samples = sampler->sample(queries);
  CHECK(samples && samples->Candidates == queries.size(),
        "spatial index visits exact local hits across bounded batches");
  if (!samples) { return; }
  for (size_t at = 0; at < queries.size(); ++at) {
    const auto &xy = queries[at];
    const int64_t face = (static_cast<int64_t>(xy[1]) * side + static_cast<int64_t>(xy[0])) * 2;
    const auto &contact = samples->Contacts[at];
    CHECK(contact.Status == SurfaceContactStatus::Contact && contact.FaceIndex == face &&
              std::abs(contact.PositionM[2] - (10 + .1 * xy[0] + .2 * xy[1])) < 1e-12,
          "owned prepared index preserves distant face identity and analytic height");
  }
}
}

int main() {
  AffineContacts();
  UnresolvedContacts();
  LimitsAndInvalidInput();
  IndexedBatch();
  return outshine::Test::Report();
}
