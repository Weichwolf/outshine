#include "Check.h"
#include "Json.h"
#include "RoadStepBody.h"
#include "RoadBodyMesh.h"
#include "SurfaceEarthworks.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr uint32_t kExpectedRisers = 21;
constexpr double kHeightToleranceM = 1e-10;

RoadPort Port(Json::Ref row) {
  const auto gate = row["gate"];
  return {
      .Gate = {.EastM = gate[size_t{0}].Num(),
               .NorthM = gate[size_t{1}].Num(),
               .GradeM = gate[size_t{2}].Num(),
               .OutE = gate[size_t{3}].Num(),
               .OutN = gate[size_t{4}].Num(),
               .HalfWidthM = gate[size_t{5}].Num(),
               .ShoulderM = gate[size_t{6}].Num(),
               .ThicknessM = gate[size_t{7}].Num()},
      .Plane = {.SlopeE = row["plane"][size_t{0}].Num(), .SlopeN = row["plane"][size_t{1}].Num()}};
}

void Profiles(const SurfaceBody &body, Json::Ref fixture) {
  const auto indices = fixture["reference_boundary_vertex_indices"];
  const auto positions = fixture["reference_positions_east_up_south_m"];
  for (size_t end = 0; end < indices.Size(); ++end) {
    for (size_t at = 0; at < indices[end].Size(); ++at) {
      const size_t vertex = static_cast<size_t>(indices[end][at].Int());
      const std::array expected{static_cast<float>(positions[vertex * 3].Num()),
                                static_cast<float>(-positions[vertex * 3 + 2].Num()),
                                static_cast<float>(positions[vertex * 3 + 1].Num())};
      CHECK(std::ranges::any_of(body.Geometry.Points,
                                [&](const auto &point) {
                                  return static_cast<float>(point[0]) == expected[0] &&
                                         static_cast<float>(point[1]) == expected[1] &&
                                         static_cast<float>(point[2]) == expected[2];
                                }),
            "each whole sloped exterior profile keeps its actual emitted Float32 words");
    }
  }
}

void Contacts(const SurfaceBody &body) {
  constexpr uint64_t key = 41;
  const SurfaceGroundSource ground{.Effect = SurfaceGroundEffect::GroundContact, .ContactKey = key};
  const std::array sources{ground, ground, ground};
  const auto contacts = BuildSurfaceEarthworks(body, sources);
  CHECK(contacts && contacts->Stamps.size() == body.ReferenceFaces.size(),
        "one body supplies both fixed-profile transitions and every terrain contact");
  if (!contacts) { return; }
  for (size_t at = 0; at < contacts->Stamps.size(); ++at) {
    const auto &stamp = contacts->Stamps[at];
    CHECK(stamp.CorridorKey == key && stamp.Fills, "one source recipe owns these ground contacts");
    for (const auto vertex : body.Geometry.Faces[body.Geometry.SurfaceFaces + at]) {
      const auto point = body.Geometry.Points[vertex];
      CHECK_NEAR(stamp.PlateauM + stamp.SlopeE * (point[0] - stamp.AtE) +
                     stamp.SlopeN * (point[1] - stamp.AtN),
                 point[2],
                 kHeightToleranceM,
                 "m",
                 "terrain is supported by the actual body underside");
    }
  }
}
}

int main() {
  std::ifstream input("test/experiments/transport/data/world-stair-ports.json");
  const std::string text{std::istreambuf_iterator<char>(input), {}};
  Json fixture;
  CHECK(fixture.Parse(text.data(), text.size()), "the real world input is available");
  if (!fixture.Ok()) { return Report(); }
  const auto root = fixture.Root();
  const auto rows = root["stations_east_north_grade_node"];
  std::vector<RoadStation> stations;
  stations.reserve(rows.Size());
  for (size_t at = 0; at < rows.Size(); ++at) {
    stations.push_back({.EastM = rows[at][size_t{0}].Num(),
                        .NorthM = rows[at][size_t{1}].Num(),
                        .GradeM = rows[at][size_t{2}].Num()});
  }
  RoadSweep surface{.HalfWidthM = root["half_width_m"].Num(),
                    .Profile = RoadProfile::Simple,
                    .WearsLinear = {{.5f, .5f, .5f}},
                    .EndPorts = {Port(root["ports"][size_t{0}]), Port(root["ports"][size_t{1}])}};
  const auto body = BuildRoadStepBody(stations, surface);
  CHECK(body.has_value(), "the actual world interval becomes a native closed stair body");
  if (!body) { return Report(); }
  CHECK(body->Stairs.size() == 1 && body->Stairs[0].Risers == kExpectedRisers,
        "the real endpoint heights require twenty-one bounded rises");
  CHECK(body->Geometry.VolumeM3 > 0, "the body has positive volume");
  Profiles(*body, root);
  Contacts(*body);
  RoadMeshBuffers mesh;
  CHECK(AppendRoadBodyMesh(body->Geometry, surface.WearsLinear, mesh),
        "the real render consumer accepts it");
  CHECK(mesh.Index.size() == body->Geometry.Faces.size() * 3 &&
            std::ranges::all_of(mesh.NormalM, [](float value) { return std::isfinite(value); }),
        "flat treads and risers retain separate finite geometric normals");
  std::ranges::reverse(stations);
  std::swap(surface.EndPorts[0], surface.EndPorts[1]);
  const auto reversed = BuildRoadStepBody(stations, surface);
  CHECK(reversed && reversed->Geometry.Points == body->Geometry.Points &&
            reversed->Geometry.Faces == body->Geometry.Faces,
        "source direction changes no physical product or exterior profile");
  stations[1].EastM += 1;
  CHECK(!BuildRoadStepBody(stations, surface), "an unplanned curved flight is refused locally");
  return Report();
}
