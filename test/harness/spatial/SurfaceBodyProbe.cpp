#include "SurfaceBody.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <fstream>
#include <iostream>
#include <limits>
#include <ratio>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;

struct Input {
  SurfaceHeightTopology Topology;
  std::vector<SurfaceHeightRecipe> Sources;
  std::vector<SurfaceHeightJunction> Ports;
  std::vector<SurfaceHeightFlight> Flights;
  std::vector<double> Heights;
};

bool Read(std::istream &stream, Input &input) {
  size_t points = 0;
  size_t sources = 0;
  size_t ports = 0;
  size_t flights = 0;
  const SurfaceBodyBudget budget;
  if (!(stream >> points >> sources >> ports >> flights) || points > budget.Points ||
      sources > 64 || ports > 128 || flights > budget.Flights) {
    return false;
  }
  input.Topology.Points.resize(points);
  input.Heights.resize(points);
  for (size_t at = 0; at < points; ++at) {
    auto &xy = input.Topology.Points[at];
    if (!(stream >> xy[0] >> xy[1] >> input.Heights[at])) { return false; }
  }
  input.Sources.resize(sources);
  input.Topology.Sources.resize(sources);
  size_t faceTotal = 0;
  for (size_t at = 0; at < sources; ++at) {
    size_t axes = 0;
    size_t faces = 0;
    auto &source = input.Sources[at];
    if (!(stream >> source.Steps >> axes >> faces) || axes > budget.Points ||
        faces > budget.Faces - faceTotal) {
      return false;
    }
    faceTotal += faces;
    source.Axis.resize(axes);
    for (auto &xy : source.Axis) {
      if (!(stream >> xy[0] >> xy[1])) { return false; }
    }
    auto &mesh = input.Topology.Sources[at];
    mesh.Faces.resize(faces);
    for (auto &face : mesh.Faces) {
      if (!(stream >> face[0] >> face[1] >> face[2])) { return false; }
    }
  }
  input.Ports.resize(ports);
  for (auto &port : input.Ports) {
    if (!(stream >> port.Origin[0] >> port.Origin[1])) { return false; }
  }
  input.Flights.resize(flights);
  for (auto &flight : input.Flights) {
    if (!(stream >> flight.Source >> flight.Start >> flight.End >> flight.Direction >>
          flight.CutStart >> flight.CutEnd)) {
      return false;
    }
  }
  return true;
}

bool Write(const char *path, const SurfaceBody &body) {
  std::ofstream output(path);
  output.precision(std::numeric_limits<double>::max_digits10);
  const auto &solid = body.Geometry;
  output << body.ReferencePoints.size() << ' ' << body.ReferenceFaces.size() << ' '
         << body.Levels.size() << ' ' << body.Stairs.size() << ' ' << solid.Points.size() << ' '
         << solid.Faces.size() << ' ' << solid.SurfaceFaces << ' ' << solid.WallParents.size()
         << ' ' << solid.VolumeM3 << '\n';
  for (const auto &point : body.ReferencePoints) {
    output << point[0] << ' ' << point[1] << ' ' << point[2] << '\n';
  }
  for (size_t at = 0; at < body.ReferenceFaces.size(); ++at) {
    const auto face = body.ReferenceFaces[at];
    output << face[0] << ' ' << face[1] << ' ' << face[2] << ' ' << body.FaceSources[at] << ' '
           << body.FaceLevels[at] << '\n';
  }
  for (const auto level : body.Levels) { output << level << '\n'; }
  for (const auto recipe : body.Stairs) {
    output << recipe.Source << ' ' << recipe.FirstLevel << ' ' << recipe.Risers << ' '
           << recipe.LowM << ' ' << recipe.HighM << ' ' << recipe.RiseM << '\n';
  }
  for (const auto &point : solid.Points) {
    output << point[0] << ' ' << point[1] << ' ' << point[2] << '\n';
  }
  for (const auto face : solid.Faces) {
    output << face[0] << ' ' << face[1] << ' ' << face[2] << '\n';
  }
  for (size_t at = 0; at < solid.WallParents.size(); ++at) {
    output << solid.WallParents[at] << ' '
           << static_cast<unsigned>(std::to_underlying(solid.WallKinds[at])) << '\n';
  }
  output.close();
  return output.good();
}
}

int main(int argc, char *const *argv) {
  if (argc != 3) { return 1; }
  Input input;
  std::ifstream stream(argv[1]);
  if (!Read(stream, input)) { return 2; }
  const SurfaceHeightRequest request{.Topology = input.Topology,
                                     .Sources = input.Sources,
                                     .Ports = input.Ports,
                                     .Flights = input.Flights,
                                     .Contacts = {},
                                     .Orders = {},
                                     .MaximumAdjustmentM = 0,
                                     .MaximumCrossfall = 0};
  const auto began = std::chrono::steady_clock::now();
  const auto body = BuildSurfaceBody(request, input.Heights);
  const double elapsed =
      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - began).count();
  if (!body) {
    std::visit(
        [](const auto error) {
          std::cerr << std::format("surface body error={}\n", std::to_underlying(error));
        },
        body.error());
    std::cerr << std::format("surface body stage={}\n", body.error().index());
    return 3;
  }
  if (!Write(argv[2], *body)) { return 4; }
  const auto &work = body->Work;
  std::cout << std::format("{{\"elapsed_us\":{},\"visits\":{},\"nearest_tests\":{},"
                           "\"plane_tests\":{},\"maximum_plane_error_m\":{},"
                           "\"envelope_faces\":{},\"band_faces\":{},\"band_visits\":{},"
                           "\"solid_index_bytes\":{}}}\n",
                           elapsed,
                           work.Visits,
                           work.NearestTests,
                           work.PlaneTests,
                           work.MaximumPlaneErrorM,
                           work.Envelope.OutputFaces,
                           work.Bands.OutputFaces,
                           work.HeightCuts.BandVisits,
                           body->Geometry.Work.IndexCapacityBytes);
  return 0;
}
