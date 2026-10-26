#include "SurfaceHeightProgramInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Generators::HeightAssembly {
namespace {
constexpr double kFlightAreaThreshold = 1e-10;
constexpr double kSectionDistanceM = 1e-7;
}

bool Priors(const Input &input, System &system) {
  std::vector<double> heights;
  for (const auto &source : input.Sources) {
    for (const auto p : source.Prior) { heights.push_back(p[2]); }
  }
  if (heights.empty()) { return false; }
  std::ranges::sort(heights);
  const size_t middle = heights.size() / 2;
  system.Datum =
      heights.size() % 2 == 1 ? heights[middle] : (heights[middle - 1] + heights[middle]) / 2;
  for (const auto &source : input.Sources) {
    for (const auto vertex : source.Vertices) {
      const double height = HeightAt(source.Prior, input.Points[vertex], system);
      system.HeightBound = std::max(system.HeightBound, std::abs(height));
      const std::array<uint32_t, 2> columns{vertex, system.Program.AdjustmentVariable};
      const std::array positive{1., -1.};
      const std::array negative{-1., -1.};
      system.Add(columns, positive, height - system.Datum);
      system.Add(columns, negative, system.Datum - height);
    }
  }
  system.HeightBound += input.Adjustment;
  system.Program.Variables[system.Program.AdjustmentVariable] = {.Minimum = 0,
                                                                 .Maximum = input.Adjustment};
  return system.Check();
}

bool Planes(const Input &input, System &system) {
  for (size_t at = 0; at < input.Ports.size(); ++at) {
    const auto &port = input.Ports[at];
    const auto height = static_cast<uint32_t>(input.Points.size() + 3 * at);
    system.Program.Variables[height + 1] = {.Minimum = 0, .Maximum = 0};
    system.Program.Variables[height + 2] = {.Minimum = 0, .Maximum = 0};
    for (const auto vertex : port.Vertices) {
      if (vertex >= input.Points.size()) { return false; }
      const auto delta = Difference(input.Points[vertex], port.Origin);
      const std::array columns{vertex, height, height + 1, height + 2};
      const std::array values{1., -1., -delta[0], -delta[1]};
      system.Equal(columns, values, 0);
    }
  }
  return system.Check();
}

namespace {
bool FlatLanding(PlanarFace face, const SurfaceHeightFlight &flight) {
  return std::ranges::all_of(
             face, [&](uint32_t vertex) { return flight.Stations[vertex] <= flight.CutStart; }) ||
         std::ranges::all_of(
             face, [&](uint32_t vertex) { return flight.Stations[vertex] >= flight.CutEnd; });
}

bool FlightSections(const Input &input, const Source &source, System &system) {
  for (const auto &section : source.Sections) {
    std::optional<uint32_t> first;
    for (const auto vertex : source.Vertices) {
      if (!system.Count(system.Work.SectionTests, system.Budget.SectionTests)) { return false; }
      if (SegmentDistance(input.Points[vertex], section[0], section[1]) <= kSectionDistanceM) {
        if (!first) {
          first = vertex;
        } else {
          const std::array columns{*first, vertex};
          const std::array values{1., -1.};
          system.Equal(columns, values, 0);
        }
      }
    }
  }
  return system.Check();
}

bool MonotonicFacets(const Input &input,
                     Workspace &workspace,
                     const Source &source,
                     const SurfaceHeightFlight &flight,
                     System &system) {
  std::vector<uint32_t> candidates;
  for (size_t segment = 1; segment < source.Axis.size(); ++segment) {
    auto tangent = Difference(source.Axis[segment], source.Axis[segment - 1]);
    const double length = std::hypot(tangent[0], tangent[1]);
    for (auto &v : tangent) { v *= -static_cast<double>(flight.Direction) / length; }
    if (!Influence(input,
                   workspace,
                   flight.Source,
                   source.Axis[segment - 1],
                   source.Axis[segment],
                   kFlightAreaThreshold,
                   candidates,
                   system)) {
      return false;
    }
    for (const auto at : candidates) {
      const auto face = source.Faces[at];
      if (FlatLanding(face, flight)) { continue; }
      system.Add(face, Along(Gradients(input.Points, face, false), tangent), 0);
      if (!system.Check()) { return false; }
    }
  }
  return system.Check();
}
}

bool Flights(const Input &input, Workspace &workspace, System &system) {
  for (const auto &flight : input.Flights) {
    const auto &source = input.Sources[flight.Source];
    const auto start =
        static_cast<uint32_t>(input.Points.size() + 3 * static_cast<size_t>(flight.Start));
    const auto end =
        static_cast<uint32_t>(input.Points.size() + 3 * static_cast<size_t>(flight.End));
    const uint32_t low = flight.Direction == 1 ? start : end;
    const uint32_t high = flight.Direction == 1 ? end : start;
    for (const auto vertex : source.Vertices) {
      const std::array a{low, vertex};
      const std::array b{vertex, high};
      const std::array values{1., -1.};
      system.Add(a, values, 0);
      system.Add(b, values, 0);
    }
    if (!MonotonicFacets(input, workspace, source, flight, system) ||
        !FlightSections(input, source, system)) {
      return false;
    }
  }
  return system.Check();
}

}
