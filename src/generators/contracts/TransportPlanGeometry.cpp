#include "TransportPlanFormat.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace outshine::Generators::TransportFormat {
bool Station(BinaryValueWriter &out, const RoadStation &station) {
  return out(station.EastM, station.NorthM, station.GradeM, station.Node);
}

bool Station(Reader &in, RoadStation &station) {
  return in(station.EastM, station.NorthM, station.GradeM, station.Node);
}

bool Gate(BinaryValueWriter &out, const RoadGate &gate) {
  return out(gate.EastM,
             gate.NorthM,
             gate.GradeM,
             gate.OutE,
             gate.OutN,
             gate.HalfWidthM,
             gate.ShoulderM,
             gate.ThicknessM);
}

bool Gate(Reader &in, RoadGate &gate) {
  return in(gate.EastM,
            gate.NorthM,
            gate.GradeM,
            gate.OutE,
            gate.OutN,
            gate.HalfWidthM,
            gate.ShoulderM,
            gate.ThicknessM);
}

bool Body(BinaryValueWriter &out, const HeightSolid &body) {
  return List(out, std::span(body.Points), [&](const auto &point) { return out(point); }) &&
         List(out, std::span(body.Faces), [&](const auto &face) { return out(face); }) &&
         out.Array(std::span(body.WallParents)) &&
         List(out, std::span(body.WallKinds), [&](auto kind) { return out(kind); }) &&
         out(static_cast<uint64_t>(body.SurfaceFaces), body.VolumeM3);
}

bool Body(Reader &in, HeightSolid &body) {
  uint64_t surfaces = 0;
  if (!in.List(body.Points, 3 * sizeof(double), [&](auto &point) { return in(point); }) ||
      !in.List(body.Faces, 3 * sizeof(uint32_t), [&](auto &face) { return in(face); }) ||
      !in.List(body.WallParents, sizeof(uint32_t), [&](auto &parent) { return in(parent); }) ||
      !in.List(body.WallKinds, sizeof(uint32_t), [&](auto &kind) { return in(kind); }) ||
      !in(surfaces, body.VolumeM3) || surfaces > std::numeric_limits<size_t>::max()) {
    return false;
  }
  body.SurfaceFaces = static_cast<size_t>(surfaces);
  return true;
}
}
