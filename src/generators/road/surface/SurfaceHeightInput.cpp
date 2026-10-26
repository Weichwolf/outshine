#include "SurfaceHeightProgramInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <vector>
#include <utility>

namespace outshine::Generators::HeightAssembly {
namespace {
template <size_t Size> bool Finite(const std::array<double, Size> &point) {
  return std::ranges::all_of(point, [](double value) { return std::isfinite(value); });
}

template <size_t Size, size_t Extent>
bool LineValid(std::span<const std::array<double, Size>, Extent> points) {
  if (points.empty()) { return false; }
  for (size_t at = 0; at < points.size(); ++at) {
    if (!Finite(points[at])) { return false; }
    if (at != 0 && points[at][0] == points[at - 1][0] && points[at][1] == points[at - 1][1]) {
      return false;
    }
  }
  return true;
}

bool RecipeValid(const SurfaceHeightRecipe &recipe, size_t &coordinates, System &system) {
  if (!std::isfinite(recipe.WidthM) || recipe.WidthM <= 0 || !std::isfinite(recipe.MaximumGrade) ||
      recipe.MaximumGrade <= 0 || recipe.Axis.size() < 2 || recipe.Prior.size() < 2 ||
      recipe.Domain.Rings.empty()) {
    return system.Fail(Error::InvalidInput);
  }
  if (!system.Count(coordinates, system.Budget.Coordinates, recipe.Axis.size()) ||
      !system.Count(coordinates, system.Budget.Coordinates, recipe.Prior.size())) {
    return false;
  }
  if (!LineValid(std::span(recipe.Axis)) || !LineValid(std::span(recipe.Prior))) {
    return system.Fail(Error::InvalidInput);
  }
  for (const auto &ring : recipe.Domain.Rings) {
    if (!system.Count(coordinates, system.Budget.Coordinates, ring.size())) { return false; }
    if (ring.size() < 3 || !LineValid(std::span(ring))) { return system.Fail(Error::InvalidInput); }
  }
  for (const auto &section : recipe.Sections) {
    if (!system.Count(coordinates, system.Budget.Coordinates, section.size())) { return false; }
    if (!LineValid(std::span(section))) { return system.Fail(Error::InvalidInput); }
  }
  return true;
}

bool SourceFaces(const Input &input, Source &source, std::vector<bool> &used, System &system) {
  if (source.Faces.empty() || source.Faces.size() != source.FaceIds.size()) {
    return system.Fail(Error::InvalidInput);
  }
  for (const auto face : source.Faces) {
    for (const auto vertex : face) {
      if (vertex >= input.Points.size()) { return system.Fail(Error::InvalidInput); }
      source.Vertices.push_back(vertex);
      used[vertex] = true;
    }
    for (const auto &axis : Gradients(input.Points, face, false)) {
      if (!Finite(axis)) { return system.Fail(Error::InvalidInput); }
    }
    if (!source.Steps) {
      for (const auto &axis : Gradients(input.Points, face, true)) {
        if (!Finite(axis)) { return system.Fail(Error::InvalidInput); }
      }
    }
  }
  std::ranges::sort(source.Vertices);
  source.Vertices.erase(std::ranges::unique(source.Vertices).begin(), source.Vertices.end());
  return system.Check();
}

bool PortsValid(const Input &input, size_t &coordinates, System &system) {
  for (const auto &port : input.Ports) {
    if (!system.Count(coordinates, system.Budget.Coordinates, port.Vertices.size())) {
      return false;
    }
    if (!Finite(port.Origin) || port.Vertices.empty()) { return system.Fail(Error::InvalidInput); }
    uint32_t previous = 0;
    for (size_t at = 0; at < port.Vertices.size(); ++at) {
      const auto vertex = port.Vertices[at];
      if (vertex >= input.Points.size() || (at != 0 && vertex <= previous)) {
        return system.Fail(Error::InvalidInput);
      }
      previous = vertex;
    }
  }
  return true;
}

bool FlightsValid(const Input &input, size_t &coordinates, System &system) {
  std::vector<bool> owned(input.Sources.size());
  for (const auto &flight : input.Flights) {
    if (!system.Count(coordinates, system.Budget.Coordinates, flight.Stations.size())) {
      return false;
    }
    if (flight.Source >= input.Sources.size() || flight.Start >= input.Ports.size() ||
        flight.End >= input.Ports.size() || flight.Start == flight.End ||
        !std::isfinite(flight.CutStart) || !std::isfinite(flight.CutEnd) ||
        !(flight.CutStart < flight.CutEnd) || flight.Stations.size() != input.Points.size() ||
        (flight.Direction != -1 && flight.Direction != 1)) {
      return system.Fail(Error::InvalidInput);
    }
    if (!input.Sources[flight.Source].Steps || owned[flight.Source]) {
      return system.Fail(Error::ConflictingOwners);
    }
    owned[flight.Source] = true;
    if (!std::ranges::all_of(flight.Stations, [](double value) { return std::isfinite(value); })) {
      return system.Fail(Error::InvalidInput);
    }
  }
  for (size_t at = 0; at < owned.size(); ++at) {
    if (input.Sources[at].Steps != owned[at]) { return system.Fail(Error::InvalidInput); }
  }
  return true;
}

bool ContactsValid(const Input &input, size_t &coordinates, System &system) {
  for (const auto &contact : input.Contacts) {
    if (!system.Count(coordinates, system.Budget.Coordinates, contact.Points.size())) {
      return false;
    }
    if (contact.Source >= input.Sources.size() || !LineValid(std::span(contact.Points))) {
      return system.Fail(Error::InvalidInput);
    }
  }
  return true;
}
}

std::expected<Input, Error> PrepareInput(const SurfaceHeightRequest &request, System &system) {
  const auto &topology = request.Topology;
  if (request.Sources.size() > std::numeric_limits<uint64_t>::digits ||
      request.Sources.size() > system.Budget.Sources ||
      request.Ports.size() > system.Budget.Ports ||
      request.Flights.size() > system.Budget.Flights ||
      request.Contacts.size() > system.Budget.Coordinates ||
      request.Orders.size() > system.Budget.Coordinates) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (topology.Points.empty() || request.Sources.empty() ||
      request.Sources.size() != topology.Sources.size() ||
      !std::isfinite(request.MaximumAdjustmentM) || request.MaximumAdjustmentM < 0 ||
      !std::isfinite(request.MaximumCrossfall) || request.MaximumCrossfall <= 0 ||
      !std::ranges::all_of(topology.Points, Finite<2>)) {
    return std::unexpected(Error::InvalidInput);
  }
  Input input{.Points = topology.Points,
              .Sources = {},
              .Ports = request.Ports,
              .Flights = request.Flights,
              .Contacts = request.Contacts,
              .Orders = {},
              .Adjustment = request.MaximumAdjustmentM,
              .Crossfall = request.MaximumCrossfall};
  size_t coordinates = topology.Points.size();
  size_t faces = 0;
  std::vector<bool> used(topology.Points.size());
  for (size_t at = 0; at < request.Sources.size(); ++at) {
    const auto &recipe = request.Sources[at];
    const auto &part = topology.Sources[at];
    if (!system.Count(faces, system.Budget.Faces, part.Faces.size()) ||
        !RecipeValid(recipe, coordinates, system)) {
      return std::unexpected(system.Failure.value_or(Error::InvalidInput));
    }
    Source source{.Width = recipe.WidthM,
                  .Grade = recipe.MaximumGrade,
                  .Steps = recipe.Steps,
                  .Axis = recipe.Axis,
                  .Prior = recipe.Prior,
                  .Faces = part.Faces,
                  .FaceIds = part.PlanarFaces,
                  .Vertices = {},
                  .Sections = recipe.Sections,
                  .Domain = &recipe.Domain};
    if (!SourceFaces(input, source, used, system)) {
      return std::unexpected(system.Failure.value_or(Error::InvalidInput));
    }
    input.Sources.push_back(std::move(source));
  }
  if (!std::ranges::all_of(used, [](bool value) { return value; })) {
    return std::unexpected(Error::InvalidInput);
  }
  input.Orders.assign(input.Sources.size() * input.Sources.size(), -1);
  for (const auto order : request.Orders) {
    if (order.Lower >= input.Sources.size() || order.Upper >= input.Sources.size() ||
        order.Lower == order.Upper) {
      return std::unexpected(Error::InvalidInput);
    }
    auto &lower = input.Orders[order.Lower * input.Sources.size() + order.Upper];
    auto &upper = input.Orders[order.Upper * input.Sources.size() + order.Lower];
    if (lower == 0) { return std::unexpected(Error::ConflictingOwners); }
    lower = 1;
    upper = 0;
  }
  if (!PortsValid(input, coordinates, system) || !FlightsValid(input, coordinates, system) ||
      !ContactsValid(input, coordinates, system)) {
    return std::unexpected(system.Failure.value_or(Error::InvalidInput));
  }
  return input;
}
}
