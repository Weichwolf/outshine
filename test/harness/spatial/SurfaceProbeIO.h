#ifndef OUTSHINE_TEST_HARNESS_SPATIAL_SURFACEPROBEIO_H
#define OUTSHINE_TEST_HARNESS_SPATIAL_SURFACEPROBEIO_H
#include "SurfaceFlightPlan.h"
#include "SurfaceHeightTopology.h"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <istream>
#include <limits>
#include <string>
#include <vector>

namespace outshine::Test::SurfaceProbes {
using namespace outshine::Generators;

inline bool ReadMesh(std::istream &input, PlanarArrangement &mesh) {
  size_t points = 0;
  size_t faces = 0;
  if (!(input >> points >> faces) || points > SurfaceHeightTopologyBudget{}.PlanarVertices ||
      faces > SurfaceHeightTopologyBudget{}.SourceFaces) {
    return false;
  }
  mesh.Mesh.Points.resize(points);
  mesh.Mesh.Faces.resize(faces);
  mesh.FaceDomains.resize(faces);
  for (auto &point : mesh.Mesh.Points) {
    if (!(input >> point[0] >> point[1])) { return false; }
  }
  for (size_t at = 0; at < faces; ++at) {
    auto &face = mesh.Mesh.Faces[at];
    if (!(input >> face[0] >> face[1] >> face[2] >> mesh.FaceDomains[at])) { return false; }
  }
  return true;
}

inline bool ReadRing(std::istream &input, PolygonRing &ring, size_t &points) {
  size_t vertices = 0;
  if (!(input >> vertices) || vertices > SurfaceHeightTopologyBudget{}.PortPoints - points) {
    return false;
  }
  points += vertices;
  ring.resize(vertices);
  for (auto &point : ring) {
    if (!(input >> point[0] >> point[1])) { return false; }
  }
  return true;
}

inline bool ReadPorts(std::istream &input,
                      std::vector<SurfaceHeightPort> &ports,
                      uint32_t &domains,
                      double &distance) {
  size_t count = 0;
  if (!(input >> domains >> count >> distance) || domains > std::numeric_limits<uint64_t>::digits ||
      count > SurfaceHeightTopologyBudget{}.PortPoints) {
    return false;
  }
  ports.resize(count);
  size_t points = 0;
  for (auto &port : ports) {
    size_t owners = 0;
    size_t rings = 0;
    if (!(input >> owners) || owners > domains) { return false; }
    port.Domains.resize(owners);
    for (auto &owner : port.Domains) {
      if (!(input >> owner)) { return false; }
    }
    if (!(input >> rings) || rings > SurfaceHeightTopologyBudget{}.PortPoints) { return false; }
    port.Region.Rings.resize(rings);
    for (auto &ring : port.Region.Rings) {
      if (!ReadRing(input, ring, points)) { return false; }
    }
  }
  return true;
}

inline bool ReadFlights(std::istream &input,
                        size_t domains,
                        std::vector<Generators::SurfaceHeightRecipe> &sources,
                        std::vector<Generators::SurfaceFlightEndpoints> &endpoints) {
  size_t sourceCount = 0;
  size_t flightCount = 0;
  if (!(input >> sourceCount >> flightCount) || sourceCount != domains ||
      flightCount > SurfaceFlightBudget{}.Flights) {
    return false;
  }
  sources.resize(sourceCount);
  for (auto &source : sources) {
    size_t count = 0;
    if (!(input >> source.Steps >> count) || count > SurfaceFlightBudget{}.Vertices) {
      return false;
    }
    source.Axis.resize(count);
    for (auto &point : source.Axis) {
      if (!(input >> point[0] >> point[1])) { return false; }
    }
  }
  endpoints.resize(flightCount);
  for (auto &flight : endpoints) {
    if (!(input >> flight.Source >> flight.Start >> flight.End >> flight.Direction)) {
      return false;
    }
  }
  return true;
}

inline bool
WriteFlights(const char *outputPath, const Generators::SurfaceFlightPlan &plan, size_t pointCount) {
  std::ofstream output(outputPath);
  output.precision(std::numeric_limits<double>::max_digits10);
  output << plan.Flights.size() << ' ' << pointCount << '\n';
  for (size_t at = 0; at < plan.Flights.size(); ++at) {
    const auto &flight = plan.Flights[at];
    const auto &axis = plan.Axes[at];
    output << flight.Source << ' ' << flight.Start << ' ' << flight.End << ' ' << flight.Direction
           << ' ' << flight.CutStart << ' ' << flight.CutEnd << ' ' << axis.Origin[0] << ' '
           << axis.Origin[1] << ' ' << axis.Direction[0] << ' ' << axis.Direction[1] << '\n';
    for (const auto station : flight.Stations) { output << station << ' '; }
    output << '\n';
  }
  output.close();
  return output.good();
}

inline bool WriteHeights(const char *path, const Generators::SurfaceHeightTopology &topology) {
  std::ofstream output(path);
  output.precision(std::numeric_limits<double>::max_digits10);
  output << topology.Points.size() << ' ' << topology.Sources.size() << '\n';
  for (size_t at = 0; at < topology.Points.size(); ++at) {
    output << topology.Points[at][0] << ' ' << topology.Points[at][1] << ' '
           << topology.PrimaryDomains[at] << ' ' << topology.PlanarVertices[at] << '\n';
  }
  for (const auto &part : topology.Sources) {
    output << part.Faces.size() << '\n';
    for (size_t at = 0; at < part.Faces.size(); ++at) {
      const auto face = part.Faces[at];
      output << face[0] << ' ' << face[1] << ' ' << face[2] << ' ' << part.PlanarFaces[at] << '\n';
    }
  }
  output.close();
  return output.good();
}

inline bool WriteMembers(const char *path, const Generators::SurfaceHeightTopology &topology) {
  std::ofstream output(path);
  output << topology.PortVertices.size() << '\n';
  for (const auto &port : topology.PortVertices) {
    output << port.size();
    for (const auto vertex : port) { output << ' ' << vertex; }
    output << '\n';
  }
  output.close();
  return output.good();
}

inline bool WriteArrangement(const char *path, const PlanarArrangement &arrangement) {
  std::ofstream parents(std::string(path) + ".parents");
  for (const auto parent : arrangement.Mesh.FaceParents) { parents << parent << ' '; }
  parents << '\n';
  parents.close();
  if (!parents.good()) { return false; }
  std::ofstream output(path);
  output.precision(std::numeric_limits<double>::max_digits10);
  output << arrangement.Mesh.Points.size() << ' ' << arrangement.Mesh.Faces.size() << '\n';
  for (const auto point : arrangement.Mesh.Points) {
    output << point[0] << ' ' << point[1] << '\n';
  }
  for (size_t at = 0; at < arrangement.Mesh.Faces.size(); ++at) {
    const auto face = arrangement.Mesh.Faces[at];
    output << face[0] << ' ' << face[1] << ' ' << face[2] << ' ' << arrangement.FaceDomains[at]
           << '\n';
  }
  output.close();
  return output.good();
}
}
#endif
