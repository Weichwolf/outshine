#include "Check.h"
#include "Corridors.h"
#include "Json.h"
#include "ProfiledRoadMesher.h"
#include "StreetGraphBuilder.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

constexpr double kPathGradient = .06;
constexpr double kStairGradient = .5;
constexpr double kGradientRoundoff = 1e-6;
constexpr double kCycleHalfWidthM = 1;
constexpr auto kWorkDeadline = std::chrono::seconds(10);

struct Sample {
  EastNorth At;
  double UpM;
};

struct Inputs {
  LongitudeLatitude Origin;
  std::vector<Osm::OsmField::Declared> Ways;
  std::vector<std::vector<Sample>> Ground;

  bool Load() {
    std::ifstream file("test/outshine/data/transport/wien-junction-sources.json");
    const std::string text(std::istreambuf_iterator<char>{file}, {});
    Json fixture;
    if (!fixture.Parse(text.data(), text.size())) { return false; }
    const auto root = fixture.Root();
    Origin = {.LongitudeDeg = root["reference_lat_lon"][1].Num(),
              .LatitudeDeg = root["reference_lat_lon"][size_t{0}].Num()};
    const auto frame = TangentFrame::At(Origin);
    const auto rows = root["ways"];
    for (size_t i = 0; i < rows.Size(); ++i) {
      const auto row = rows[i];
      const auto properties = row["source"]["feature"]["properties"];
      auto &way = Ways.emplace_back();
      way.Layer = "streets";
      way.Key = "kind";
      way.Value = properties["subclass"].Str();
      way.WidthM = row["width_m"].Num();
      way.Bridge = row["bridge"].Bool();
      way.Level = properties["layer"].Int();
      auto &ground = Ground.emplace_back();
      const auto points = row["points_lat_lon"];
      for (size_t j = 0; j < points.Size(); ++j) {
        const double lat = points[j][size_t{0}].Num();
        const double lon = points[j][1].Num();
        way.LatLon.insert(way.LatLon.end(), {lat, lon});
        ground.push_back(
            {.At = frame.ToLocalGroundPosition({.LongitudeDeg = lon, .LatitudeDeg = lat}),
             .UpM = row["source_ground_height_m"][j].Num()});
      }
    }
    return Ways.size() == 4;
  }

  [[nodiscard]] double HeightAt(EastNorth at) const {
    double nearest = std::numeric_limits<double>::infinity();
    double heightM = 0;
    for (const auto &along : Ground) {
      for (size_t i = 1; i < along.size(); ++i) {
        const auto &a = along[i - 1];
        const auto &b = along[i];
        const double e = b.At.EastM - a.At.EastM;
        const double n = b.At.NorthM - a.At.NorthM;
        const double t = std::clamp(((at.EastM - a.At.EastM) * e + (at.NorthM - a.At.NorthM) * n) /
                                        (e * e + n * n),
                                    0.0,
                                    1.0);
        const double gap =
            std::hypot(at.EastM - a.At.EastM - t * e, at.NorthM - a.At.NorthM - t * n);
        if (gap < nearest) {
          nearest = gap;
          heightM = std::lerp(a.UpM, b.UpM, t);
        }
      }
    }
    return heightM;
  }
};

struct RecordingMesher final : RoadMesher {
  ProfiledRoadMesher Native;
  mutable size_t Sweeps = 0;
  mutable bool LegalGrades = true;

  RoadMeshingStats
  Sweep(std::span<const RoadStation> along, RoadSweep how, RoadMeshBuffers &into) const override {
    ++Sweeps;
    const double maximum = how.HalfWidthM >= kCycleHalfWidthM ? kPathGradient : kStairGradient;
    for (size_t i = 1; i < along.size(); ++i) {
      const double runM =
          std::hypot(along[i].EastM - along[i - 1].EastM, along[i].NorthM - along[i - 1].NorthM);
      LegalGrades &=
          std::abs(along[i].GradeM - along[i - 1].GradeM) <= maximum * runM + kGradientRoundoff;
    }
    return Native.Sweep(along, how, into);
  }

  void Junction(std::span<const RoadGate> gates,
                RoadPlane plane,
                const Vec3f &wears,
                RoadMeshBuffers &into) const override {
    LegalGrades &= std::hypot(plane.SlopeE, plane.SlopeN) <= kPathGradient + kGradientRoundoff;
    Native.Junction(gates, plane, wears, into);
  }
};

Geometry Build(const Inputs &input, bool incremental) {
  Geometry geometry;
  const auto frame = TangentFrame::At(input.Origin);
  outshine::Ground::GroundMaterials materials;
  outshine::Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "the genuine case uses production path, stair and bridge recipes");
  const std::array<std::string, 1> layers{"streets"};
  Osm::OsmField vectors(14, layers);
  CHECK(vectors.Declare(input.Ways, input.Origin).has_value(),
        "all four genuine axes are retained");
  Osm::StreetField ways;
  CHECK(ways.Ingest(vectors, vegetation) == input.Ways.size(), "all source axes enter planning");
  const auto sourcePoints = std::vector(vectors.Points().begin(), vectors.Points().end());
  const auto network = Osm::StreetGraphBuilder::BuildOneShot(
      ways, vectors.Points(), [&](LongitudeLatitude at) -> std::optional<double> {
        return input.HeightAt(frame.ToLocalGroundPosition(at));
      });
  CHECK(network.Graph, "the production graph includes both neighbouring bridge junctions");
  if (!network.Graph) { return geometry; }
  const auto empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty, .Field = [&](EastNorth at) { return input.HeightAt(at); }};
  const std::shared_ptr<const ClassStructure> classes;
  const RecordingMesher mesher;
  const Corridors corridors(mesher);
  const Corridors::Site site{.Vectors = &vectors,
                             .Ways = ways,
                             .Materials = materials,
                             .Vegetation = vegetation,
                             .Ground = nullptr,
                             .Network = network.Graph.get(),
                             .Standing = frame,
                             .Draped = drape,
                             .Classes = classes,
                             .WaterUpM = {},
                             .CensusAt = {},
                             .EyeLatDeg = input.Origin.LatitudeDeg,
                             .EyeLonDeg = input.Origin.LongitudeDeg,
                             .Projection = {.FocalPx = 800}};
  std::vector<EarthworkStamp> contacts;
  std::vector<DiagnosticSample> notes;
  bool ready = false;
  if (incremental) {
    auto job = Corridors::Begin(site);
    const auto deadline = std::chrono::steady_clock::now() + kWorkDeadline;
    while (std::chrono::steady_clock::now() < deadline) {
      const auto step = corridors.Advance(*job, site, 1, 1, geometry, contacts, notes);
      if (!step) { break; }
      if (*step) {
        ready = true;
        break;
      }
    }
  } else {
    ready = corridors.Lay(site, geometry, contacts, notes);
  }
  CHECK(ready && geometry.parts() > 0 && mesher.Sweeps > 0 && !contacts.empty(),
        "a locally impossible cycle retains usable road geometry and terrain contacts");
  CHECK(mesher.LegalGrades, "the retained network preserves its declared grade limits");
  const auto omitted = std::ranges::count_if(notes, [](const auto &note) {
    return note.Name.starts_with("streets: unused source ") && note.Unit == "ways" &&
           note.Value == 1;
  });
  CHECK(omitted > 0 && static_cast<size_t>(omitted) < input.Ways.size(),
        "only a local subset is omitted and every omission has a named reason");
  CHECK(std::ranges::equal(sourcePoints, vectors.Points()) &&
            std::ranges::all_of(ways.Ways(), [](const auto &way) { return way.PointCount >= 2; }),
        "the original source field and its four axes are unchanged by local planning");
  return geometry;
}

}

int main() {
  Inputs input;
  CHECK(input.Load(), "the full four-axis source counterexample is available");
  if (input.Ways.size() != 4) { return Report(); }
  const auto sync = Build(input, false);
  const auto async = Build(input, true);
  CHECK(sync.parts() == async.parts(), "bulk and incremental planning retain the same parts");
  for (int part = 0; part < sync.parts() && part < async.parts(); ++part) {
    CHECK(std::ranges::equal(sync.positionsOf(part), async.positionsOf(part)) &&
              std::ranges::equal(sync.trianglesOf(part), async.trianglesOf(part)),
          "bulk and incremental planning publish exactly the same native geometry");
  }
  return Report();
}
