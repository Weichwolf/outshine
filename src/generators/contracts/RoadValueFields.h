#ifndef OUTSHINE_GENERATORS_CONTRACTS_ROADVALUEFIELDS_H
#define OUTSHINE_GENERATORS_CONTRACTS_ROADVALUEFIELDS_H

#include "BinaryValueArchive.h"
#include "BudgetedBinaryValueReader.h"
#include "HeightSolid.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace outshine::Generators::RoadValueFields {
template <class T, class Write>
bool WriteList(BinaryValueWriter &out, std::span<const T> items, Write write) {
  return items.size() <= std::numeric_limits<uint32_t>::max() &&
         out(static_cast<uint32_t>(items.size())) && std::ranges::all_of(items, write);
}

template <class Archive, class T> bool Station(Archive &out, T &station) {
  return out(station.EastM, station.NorthM, station.GradeM, station.Node);
}

template <class Archive, class T> bool Gate(Archive &out, T &gate) {
  return out(gate.EastM,
             gate.NorthM,
             gate.GradeM,
             gate.OutE,
             gate.OutN,
             gate.HalfWidthM,
             gate.ShoulderM,
             gate.ThicknessM);
}

bool Body(BinaryValueWriter &out, const HeightSolid &body);
[[nodiscard]] bool ValidBody(const HeightSolid &body);

template <class Error> bool Body(BudgetedBinaryValueReader<Error> &in, HeightSolid &body) {
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
#endif
