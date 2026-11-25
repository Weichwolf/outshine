#include "StructurePositionPalette.h"
#include "StructureBinary.h"
#include "StructureMesher.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Generators::StructureBinary {
namespace {
using Position = PositionPalette::Position;
constexpr size_t kShortAddressCount = size_t{1} + std::numeric_limits<uint16_t>::max();

struct Entry {
  Position Words;
  uint32_t Vertex;
};

bool WritePositions(Writer &out, std::span<const Position> positions) {
  for (size_t axis = 0; axis < Position{}.size(); ++axis) {
    for (unsigned byte = 0; byte < sizeof(uint32_t); ++byte) {
      uint32_t previous = 0;
      for (const auto &point : positions) {
        const uint32_t current = point[axis];
        if (!out.Number(static_cast<uint8_t>((previous ^ current) >> (byte * 8u)))) {
          return false;
        }
        previous = current;
      }
    }
  }
  return true;
}

bool ReadPositions(Reader &in, std::span<Position> positions) {
  for (size_t axis = 0; axis < Position{}.size(); ++axis) {
    for (unsigned byte = 0; byte < sizeof(uint32_t); ++byte) {
      for (auto &point : positions) {
        uint8_t word = 0;
        if (!in.Number(word)) { return false; }
        point[axis] |= static_cast<uint32_t>(word) << (byte * 8u);
      }
    }
    uint32_t previous = 0;
    for (auto &point : positions) {
      point[axis] ^= previous;
      previous = point[axis];
      if (!std::isfinite(std::bit_cast<float>(point[axis]))) { return false; }
    }
  }
  return true;
}

template <typename Archive, typename Geometry>
bool Attributes(Archive &archive, Geometry &geometry) {
  for (size_t axis = 0; axis < 2; ++axis) {
    for (auto *vertices : {&geometry.WallCorners, &geometry.RoofCorners}) {
      for (auto &vertex : *vertices) {
        if (!archive.Number(vertex.texture[axis])) { return false; }
      }
    }
  }
  for (auto *vertices : {&geometry.WallCorners, &geometry.RoofCorners}) {
    for (auto &vertex : *vertices) {
      if (!archive.Number(vertex.normWord)) { return false; }
    }
  }
  return true;
}

template <typename Address>
bool WriteReferences(Writer &out, std::span<const uint32_t> references) {
  for (const auto index : references) {
    if (!out.Number(static_cast<Address>(index))) { return false; }
  }
  return true;
}

template <typename Address>
bool ReadReferences(Reader &in, std::span<const Position> positions, Raised &geometry) {
  for (auto *vertices : {&geometry.WallCorners, &geometry.RoofCorners}) {
    for (auto &vertex : *vertices) {
      Address index = 0;
      if (!in.Number(index) || index >= positions.size()) { return false; }
      const Position &point = positions[index];
      for (size_t axis = 0; axis < point.size(); ++axis) {
        vertex.pos[axis] = std::bit_cast<float>(point[axis]);
      }
    }
  }
  return true;
}
}

std::optional<PositionPalette> PositionPalette::Make(const Raised &geometry) {
  const size_t count = geometry.WallCorners.size() + geometry.RoofCorners.size();
  if (count == 0 || count > kStructureArtifactBytesMost / sizeof(StoredVertex)) {
    return std::nullopt;
  }
  std::vector<Entry> entries;
  entries.reserve(count);
  for (const auto *vertices : {&geometry.WallCorners, &geometry.RoofCorners}) {
    for (const auto &vertex : *vertices) {
      if (!std::ranges::all_of(vertex.pos,
                               [](float coordinate) { return std::isfinite(coordinate); })) {
        return std::nullopt;
      }
      entries.push_back({.Words = {std::bit_cast<uint32_t>(vertex.pos[0]),
                                   std::bit_cast<uint32_t>(vertex.pos[1]),
                                   std::bit_cast<uint32_t>(vertex.pos[2])},
                         .Vertex = static_cast<uint32_t>(entries.size())});
    }
  }
  std::ranges::sort(entries, {}, &Entry::Words);
  PositionPalette palette;
  palette.References.resize(count);
  for (const auto &entry : entries) {
    if (palette.Positions.empty() || palette.Positions.back() != entry.Words) {
      palette.Positions.push_back(entry.Words);
    }
    palette.References[entry.Vertex] = static_cast<uint32_t>(palette.Positions.size() - 1u);
  }
  const auto width =
      palette.Positions.size() <= kShortAddressCount ? sizeof(uint16_t) : sizeof(uint32_t);
  const size_t packed =
      sizeof(uint64_t) + palette.Positions.size() * sizeof(Position) + width * count;
  if (packed >= count * sizeof(Position)) { return std::nullopt; }
  return palette;
}

bool PositionPalette::Write(Writer &out, const Raised &geometry) const {
  if (!out.Number(static_cast<uint64_t>(Positions.size())) ||
      !out.Number(static_cast<uint64_t>(geometry.WallCorners.size())) ||
      !out.Number(static_cast<uint64_t>(geometry.RoofCorners.size())) ||
      !WritePositions(out, Positions)) {
    return false;
  }
  const bool written = Positions.size() <= kShortAddressCount
                           ? WriteReferences<uint16_t>(out, References)
                           : WriteReferences<uint32_t>(out, References);
  return written && Attributes(out, geometry);
}

bool PositionPalette::Read(Reader &in, Raised &geometry) {
  uint64_t unique = 0;
  uint64_t walls = 0;
  uint64_t roofs = 0;
  if (!in.Number(unique) || !in.Number(walls) || !in.Number(roofs) || unique == 0 ||
      unique > in.Bytes.size() / sizeof(Position) ||
      unique > in.AllocationLeft / sizeof(Position)) {
    return false;
  }
  const auto positionBytes = static_cast<size_t>(unique) * sizeof(Position);
  const size_t width = unique <= kShortAddressCount ? sizeof(uint16_t) : sizeof(uint32_t);
  const size_t maxVertices =
      std::min((in.AllocationLeft - positionBytes) / sizeof(StoredVertex),
               (in.Bytes.size() - positionBytes) / (width + sizeof(Position)));
  if (walls > maxVertices || roofs > maxVertices - walls || unique > walls + roofs) {
    return false;
  }
  in.AllocationLeft -= static_cast<size_t>(walls + roofs) * sizeof(StoredVertex);
  std::vector<Position> positions(static_cast<size_t>(unique));
  if (!ReadPositions(in, positions)) { return false; }
  geometry.WallCorners.resize(static_cast<size_t>(walls));
  geometry.RoofCorners.resize(static_cast<size_t>(roofs));
  const bool read = unique <= kShortAddressCount
                        ? ReadReferences<uint16_t>(in, positions, geometry)
                        : ReadReferences<uint32_t>(in, positions, geometry);
  return read && Attributes(in, geometry);
}
}
