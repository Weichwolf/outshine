#include "scene/SurfaceSampler.h"
#include "Json.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using Face = std::array<uint32_t, 3>;
constexpr double kHeightMatchM = 1e-10;

struct Fixture {
  std::vector<Vec3> Vertices;
  std::vector<Face> Faces;
  Vec2 Query;
  double Tolerance = 0;
  double Height = 0;
};

std::optional<Fixture> Load() {
  std::ifstream file("lib/tests/data/skinny-shared-ground-contact.json");
  const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  Json json;
  CHECK(!text.empty() && json.Parse(text.data(), text.size()), "the shared Python input loads");
  if (!json.Ok()) { return std::nullopt; }
  const auto root = json.Root();
  Fixture fixture;
  const auto vertices = root["vertices_enu_m"];
  const auto faces = root["triangles"];
  for (size_t at = 0; at < vertices.Size(); ++at) {
    fixture.Vertices.push_back(
        {{vertices[at][size_t{0}].Num(), vertices[at][1].Num(), vertices[at][2].Num()}});
  }
  for (size_t at = 0; at < faces.Size(); ++at) {
    fixture.Faces.push_back({static_cast<uint32_t>(faces[at][size_t{0}].Int()),
                             static_cast<uint32_t>(faces[at][1].Int()),
                             static_cast<uint32_t>(faces[at][2].Int())});
  }
  const auto xy = root["query_xy_m"][size_t{0}];
  fixture.Query = {{xy[size_t{0}].Num(), xy[1].Num()}};
  fixture.Tolerance = root["xy_tolerance_m"].Num();
  fixture.Height = root["expected_height_m"].Num();
  CHECK(fixture.Vertices.size() == 4 && fixture.Faces.size() == 2 && fixture.Tolerance > 0,
        "the fixed original facets and metric tolerance survive");
  return fixture;
}

SurfaceContact Probe(const Fixture &fixture, size_t candidates = 100) {
  auto sampler =
      SurfaceSampler::prepare(fixture.Vertices, fixture.Faces, {.XyToleranceM = fixture.Tolerance});
  CHECK(sampler.has_value(), "the selected finite height surface prepares");
  if (!sampler) { return {}; }
  const std::array xy{fixture.Query};
  auto sampled = sampler->sample(xy, {.MaxCandidates = candidates});
  CHECK(sampled && sampled->Contacts.size() == 1, "the original query is retained");
  return sampled ? sampled->Contacts.front() : SurfaceContact{};
}

void Shared(Fixture fixture) {
  for (size_t variant = 0; variant < 3; ++variant) {
    if (variant == 1) { std::ranges::reverse(fixture.Faces); }
    if (variant == 2) {
      for (auto &face : fixture.Faces) { std::ranges::reverse(face); }
    }
    const auto contact = Probe(fixture);
    CHECK(contact.Status == SurfaceContactStatus::Contact && contact.FaceIndex == 0,
          "the actual thin shared edge chooses the lowest original face in every order");
    CHECK(std::abs(contact.PositionM[2] - fixture.Height) < kHeightMatchM,
          "the actual contact retains its expected affine height");
  }
}

void Ambiguous(const Fixture &fixture) {
  Fixture unshared = fixture;
  unshared.Vertices.clear();
  unshared.Faces = {{0, 1, 2}, {3, 4, 5}};
  for (const auto &face : fixture.Faces) {
    for (const auto id : face) { unshared.Vertices.push_back(fixture.Vertices[id]); }
  }
  CHECK(Probe(unshared).Status == SurfaceContactStatus::Ambiguous,
        "geometrically equal unshared source vertices do not resolve overlap");
  Fixture changed = fixture;
  changed.Vertices[3][2] += 1;
  CHECK(Probe(changed).Status == SurfaceContactStatus::Ambiguous,
        "contradictory affine heights remain unresolved despite shared topology");
  CHECK(Probe(fixture, 1).Status == SurfaceContactStatus::CandidateBudget,
        "the conservative candidate budget remains binding");
  const Fixture extended{
      .Vertices = {{{0, 0, 10}}, {{1, 0, 10}}, {{6, 1e-9, 10}}, {{6, -1e-9, 10}}},
      .Faces = {{0, 1, 2}, {0, 1, 3}},
      .Query = {{5, 0}},
      .Tolerance = 1e-7,
      .Height = 10};
  const auto contact = Probe(extended);
  CHECK(contact.Status == SurfaceContactStatus::Ambiguous && contact.FaceIndex == -1 &&
            std::isnan(contact.PositionM[2]),
        "proximity to an infinite extension is not proximity to the finite shared edge");
}
}

int main() {
  const auto fixture = Load();
  if (fixture) {
    Shared(*fixture);
    Ambiguous(*fixture);
  }
  return Report();
}
