#include "TransportPlanFormat.h"
#include <cstdint>
#include <limits>
#include <span>
#include <variant>

namespace outshine::Generators::TransportFormat {
namespace {
bool End(BinaryValueWriter &out, const TransportPathEnd &end) {
  if (const auto *boundary = std::get_if<TransportBoundaryPortReference>(&end)) {
    return out(uint32_t{1}, boundary->Port);
  }
  if (const auto *junction = std::get_if<TransportJunctionGateReference>(&end)) {
    return out(uint32_t{2}, junction->Junction, junction->Gate);
  }
  return out(uint32_t{0});
}

bool End(Reader &in, TransportPathEnd &end) {
  uint32_t kind = 0;
  if (!in(kind)) { return false; }
  switch (kind) {
    case 0: end = std::monostate{}; return true;
    case 1: {
      TransportBoundaryPortReference boundary;
      if (!in(boundary.Port)) { return false; }
      end = boundary;
      return true;
    }
    case 2: {
      TransportJunctionGateReference junction;
      if (!in(junction.Junction, junction.Gate)) { return false; }
      end = junction;
      return true;
    }
    default: return false;
  }
}

template <class Archive, class Path> bool PathScalars(Archive &archive, Path &path) {
  return archive(path.FirstStation,
                 path.StationCount,
                 path.HalfWidthM,
                 path.CrossfallRad,
                 path.MaximumGradient,
                 path.SpeedMps,
                 path.Profile,
                 path.Use,
                 path.Layer,
                 path.Lanes,
                 path.Priority,
                 path.Oneway,
                 path.Bridge);
}
}

bool Input(BinaryValueWriter &out, const TransportInputReference &input) {
  return out.Text(input.DatasetId) && out.Text(input.Version) && out.Text(input.Revision) &&
         out.Text(input.ItemId) && out(input.Content);
}

bool Input(Reader &in, TransportInputReference &input) {
  return in.Text(input.DatasetId) && in.Text(input.Version) && in.Text(input.Revision) &&
         in.Text(input.ItemId) && in(input.Content);
}

bool Path(BinaryValueWriter &out, const TransportPath &path) {
  return out.Text(path.PartId) && out.Text(path.SourceId) && out.Text(path.Material) &&
         PathScalars(out, path) && End(out, path.Ends[0]) && End(out, path.Ends[1]) &&
         out(path.Body.has_value()) && (!path.Body || out(*path.Body));
}

bool Path(Reader &in, TransportPath &path) {
  bool body = false;
  if (!in.Text(path.PartId) || !in.Text(path.SourceId) || !in.Text(path.Material) ||
      !PathScalars(in, path) || !End(in, path.Ends[0]) || !End(in, path.Ends[1]) || !in(body)) {
    return false;
  }
  if (body) {
    path.Body.emplace();
    return in(*path.Body);
  }
  return true;
}

bool Port(BinaryValueWriter &out, const TransportPort &port) {
  return out(static_cast<uint32_t>(port.Boundary.Level),
             port.Boundary.Axis,
             port.Boundary.Line,
             port.Boundary.Segment) &&
         out.Text(port.SourceId) &&
         out(port.Inputs, port.Anchor.LongitudeDeg, port.Anchor.LatitudeDeg) &&
         Gate(out, port.Contact.Gate) &&
         out(port.Contact.Plane.SlopeE,
             port.Contact.Plane.SlopeN,
             port.Profile,
             port.CrossfallRad,
             port.Layer,
             port.Use);
}

bool Port(Reader &in, TransportPort &port) {
  uint32_t level = 0;
  if (!in(level, port.Boundary.Axis, port.Boundary.Line, port.Boundary.Segment) ||
      level > static_cast<uint32_t>(std::numeric_limits<int>::max()) || !in.Text(port.SourceId) ||
      !in(port.Inputs, port.Anchor.LongitudeDeg, port.Anchor.LatitudeDeg) ||
      !Gate(in, port.Contact.Gate) ||
      !in(port.Contact.Plane.SlopeE,
          port.Contact.Plane.SlopeN,
          port.Profile,
          port.CrossfallRad,
          port.Layer,
          port.Use)) {
    return false;
  }
  port.Boundary.Level = static_cast<int>(level);
  return true;
}

bool Junction(BinaryValueWriter &out, const TransportJunction &junction) {
  return out(junction.Node) &&
         List(out,
              std::span(junction.Sources),
              [&](const auto &source) { return out.Text(source); }) &&
         out.Text(junction.Material) &&
         List(out, std::span(junction.Gates), [&](const auto &gate) { return Gate(out, gate); }) &&
         out(junction.Plane.SlopeE, junction.Plane.SlopeN);
}

bool Junction(Reader &in, TransportJunction &junction) {
  return in(junction.Node) &&
         in.List(
             junction.Sources, sizeof(uint32_t), [&](auto &source) { return in.Text(source); }) &&
         in.Text(junction.Material) &&
         in.List(junction.Gates, 8 * sizeof(double), [&](auto &gate) { return Gate(in, gate); }) &&
         in(junction.Plane.SlopeE, junction.Plane.SlopeN);
}

bool Omission(BinaryValueWriter &out, const TransportOmission &omission) {
  return out.Text(omission.SourceId) && out(omission.Reason);
}

bool Omission(Reader &in, TransportOmission &omission) {
  return in.Text(omission.SourceId) && in(omission.Reason);
}
}
