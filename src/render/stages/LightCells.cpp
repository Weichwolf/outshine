#include "LightCells.h"
#include "scene/PunctualLight.h"
#include "math/Vec3.h"
#include "math/Vec4.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace outshine::Render {
namespace {
using Cell = std::array<int32_t, 3>;
constexpr int32_t kPositionCellLimit = LightCellFrame::kPositionCellLimit;
constexpr std::array<uint32_t, 3> kHashMixers{0x9e3779b1u, 0x85ebca77u, 0xc2b2ae3du};

struct CellSlot {
  Cell Key{};
  uint32_t Count = 0;
  uint32_t First = 0;
};

static_assert(sizeof(CellSlot) == LightCells::kCellWords * sizeof(uint32_t) &&
              alignof(CellSlot) == 4);
static_assert(sizeof(Cell) == 12 && alignof(Cell) == 4);

struct PackedLayout {
  size_t Capacity = 0, Globals = 0;
  float MaximumRange = 0;
};

struct Bounds {
  Cell First{}, Last{};
  uint64_t Tests = 0;
};

bool Global(const SubjectLightUniform &light) {
  return light.Tint[3] == 0 || std::fpclassify(light.Place[3]) != FP_NORMAL;
}

bool Finite(const auto &values) {
  return std::ranges::all_of(values, [](auto value) { return std::isfinite(value); });
}

bool Valid(const SubjectLight &source) {
  const auto &light = source.Light;
  const bool kind = light.Kind == LightKind::Directional || light.Kind == LightKind::Point ||
                    light.Kind == LightKind::Spot;
  return kind && Finite(source.PositionEcefM) && Finite(light.Colour) && Finite(light.Direction) &&
         std::isfinite(light.Intensity) && light.Intensity >= 0 &&
         std::ranges::all_of(light.Colour, [](float channel) { return channel >= 0; }) &&
         std::isfinite(light.RangeM) && light.RangeM >= 0 && std::isfinite(light.InnerConeRad) &&
         std::isfinite(light.OuterConeRad);
}

bool Encode(const SubjectLight &source, SubjectLightUniform &packed, Cell &cell) {
  if (!Valid(source)) { return false; }
  const auto &light = source.Light;
  for (size_t axis = 0; axis < 3; ++axis) {
    const double coordinate = std::floor(source.PositionEcefM[axis] / LightCells::kPositionWidthM);
    if (coordinate < -kPositionCellLimit || coordinate >= kPositionCellLimit) { return false; }
    cell[axis] = static_cast<int32_t>(coordinate);
    packed.Tint[axis] = light.Colour[axis] * light.Intensity;
    if (!std::isfinite(packed.Tint[axis])) { return false; }
    packed.Place[axis] =
        static_cast<float>(source.PositionEcefM[axis] - coordinate * LightCells::kPositionWidthM);
    packed.Beam[axis] = light.Direction[axis];
  }
  packed.Tint[3] = static_cast<float>(light.Kind);
  packed.Place[3] = light.RangeM > 0 ? 1.0f / light.RangeM : 0.0f;
  const float outer = std::cos(light.OuterConeRad);
  const float inner = std::cos(light.InnerConeRad);
  packed.Cone = {{outer, inner > outer ? 1.0f / (inner - outer) : 0.0f, 0, 0}};
  return true;
}

bool FootprintBounds(const Vec3 &centre, double radius, Bounds &bounds, uint64_t remaining) {
  uint64_t tests = 1;
  for (size_t axis = 0; axis < 3; ++axis) {
    const double low =
        std::floor(std::nextafter(centre[axis] - radius, -std::numeric_limits<double>::infinity()) /
                   LightCells::kLookupWidthM);
    const double high =
        std::floor(std::nextafter(centre[axis] + radius, std::numeric_limits<double>::infinity()) /
                   LightCells::kLookupWidthM);
    if (low < std::numeric_limits<int32_t>::min() || high > std::numeric_limits<int32_t>::max()) {
      return false;
    }
    const auto extent = static_cast<uint64_t>(high - low + 1);
    if (extent == 0 || tests > remaining / extent) { return false; }
    bounds.First[axis] = static_cast<int32_t>(low);
    bounds.Last[axis] = static_cast<int32_t>(high);
    tests *= extent;
  }
  bounds.Tests = tests;
  return true;
}

template <class Visit>
bool VisitFootprint(
    const Bounds &bounds, double radius, const Vec3 &centre, uint64_t &tests, Visit visit) {
  for (int64_t x = bounds.First[0]; x <= bounds.Last[0]; ++x) {
    for (int64_t y = bounds.First[1]; y <= bounds.Last[1]; ++y) {
      for (int64_t z = bounds.First[2]; z <= bounds.Last[2]; ++z) {
        const Cell cell{static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z)};
        double distance = 0;
        for (size_t axis = 0; axis < 3; ++axis) {
          const double low = static_cast<double>(cell[axis]) * LightCells::kLookupWidthM;
          const double high = (static_cast<double>(cell[axis]) + 1) * LightCells::kLookupWidthM;
          const double delta = std::max({low - centre[axis], centre[axis] - high, 0.0});
          distance += delta * delta;
        }
        if (distance <= radius * radius && !visit(cell)) {
          const auto rows = static_cast<uint64_t>(int64_t{bounds.Last[1]} - bounds.First[1] + 1);
          const auto columns = static_cast<uint64_t>(int64_t{bounds.Last[2]} - bounds.First[2] + 1);
          tests += (static_cast<uint64_t>(x - bounds.First[0]) * rows +
                    static_cast<uint64_t>(y - bounds.First[1])) *
                       columns +
                   static_cast<uint64_t>(z - bounds.First[2]) + 1;
          return false;
        }
      }
    }
  }
  tests += bounds.Tests;
  return true;
}

uint32_t Key(int32_t coordinate) {
  return std::bit_cast<uint32_t>(coordinate) ^ 0x80000000u;
}

uint32_t Hash(const Cell &cell) {
  return Key(cell[0]) * kHashMixers[0] ^ Key(cell[1]) * kHashMixers[1] ^
         Key(cell[2]) * kHashMixers[2];
}

size_t FindCell(std::span<const CellSlot> slots, const Cell &key) {
  size_t at = Hash(key) & (slots.size() - 1);
  while (slots[at].Count != 0 && slots[at].Key != key) { at = (at + 1) & (slots.size() - 1); }
  return at;
}

struct CountedCells {
  static constexpr size_t kLiveBytesPerSlot =
      sizeof(CellSlot) + sizeof(Cell) / 2 + sizeof(uint32_t);
  std::vector<CellSlot> Slots;
  size_t Count = 0, MaximumCapacity = 0, PeakBytes = 0;
  uint64_t Assignments = 0;
  size_t Allocations = 0, CopyBytes = 0;

  bool Grow() {
    const size_t capacity = Slots.empty() ? 2 : Slots.size() * 2;
    if (capacity > MaximumCapacity) { return false; }
    std::vector<CellSlot> next(capacity);
    ++Allocations;
    CopyBytes += Count * sizeof(CellSlot);
    PeakBytes = std::max(PeakBytes, (Slots.capacity() + next.capacity()) * sizeof(CellSlot));
    for (const auto &slot : Slots) {
      if (slot.Count != 0) { next[FindCell(next, slot.Key)] = slot; }
    }
    Slots.swap(next);
    return true;
  }

  bool Add(const Cell &key) {
    if (Slots.empty() && !Grow()) { return false; }
    size_t at = FindCell(Slots, key);
    if (Slots[at].Count == 0) {
      if (Count == Slots.size() / 2) {
        if (!Grow()) { return false; }
        at = FindCell(Slots, key);
      }
      Slots[at].Key = key;
      ++Count;
    }
    ++Slots[at].Count;
    ++Assignments;
    return true;
  }
};

void PackCellHeaders(const CountedCells &table,
                     std::span<const Cell> ordered,
                     size_t capacity,
                     std::vector<uint32_t> &words) {
  size_t next = LightCells::kHeaderWords + LightCells::kCellWords * capacity;
  for (const auto &cell : ordered) {
    const auto count = table.Slots[FindCell(table.Slots, cell)].Count;
    size_t slot = Hash(cell) & (capacity - 1);
    while (words[LightCells::kHeaderWords + LightCells::kCellWords * slot + 4] != 0) {
      slot = (slot + 1) & (capacity - 1);
    }
    const size_t at = LightCells::kHeaderWords + LightCells::kCellWords * slot;
    for (size_t axis = 0; axis < 3; ++axis) { words[at + axis] = Key(cell[axis]); }
    words[at + 3] = static_cast<uint32_t>(next);
    words[at + 4] = count;
    next += count;
  }
}

void PackWords(LightCells &result,
               std::span<const SubjectLight> sources,
               const CountedCells &table,
               std::span<const Cell> ordered,
               const PackedLayout &layout) {
  const size_t globalAt =
      LightCells::kHeaderWords + LightCells::kCellWords * layout.Capacity + table.Assignments;
  const size_t cellAt = globalAt + layout.Globals;
  result.Words.assign(cellAt + 3 * sources.size(), 0);
  auto &words = result.Words;
  words[0] = static_cast<uint32_t>(globalAt);
  words[1] = static_cast<uint32_t>(layout.Globals);
  words[2] = LightCells::kHeaderWords;
  words[3] = static_cast<uint32_t>(layout.Capacity - 1);
  words[4] = static_cast<uint32_t>(cellAt);
  words[5] = std::bit_cast<uint32_t>(layout.MaximumRange);
  words[6] = static_cast<uint32_t>(sources.size());
  words[7] = LightCells::kFormatVersion;
  PackCellHeaders(table, ordered, layout.Capacity, words);
  size_t global = globalAt;
  for (size_t id = 0; id < sources.size(); ++id) {
    if (result.FullEnumeration || Global(result.Emitters[id])) {
      words[global++] = static_cast<uint32_t>(id);
    }
    for (size_t axis = 0; axis < 3; ++axis) {
      const auto cell = static_cast<int32_t>(
          std::floor(sources[id].PositionEcefM[axis] / LightCells::kPositionWidthM));
      words[cellAt + 3 * id + axis] = std::bit_cast<uint32_t>(cell);
    }
  }
}

struct PlannedLights {
  uint64_t Tests = 0;
  size_t Globals = 0;
  float MaximumRange = 0;
};

std::expected<PlannedLights, std::string> PlanLights(std::span<const SubjectLight> lights,
                                                     LightCells &result,
                                                     const LightCellLimits &limits) {
  PlannedLights plan;

  for (size_t id = 0; id < lights.size(); ++id) {
    auto &packed = result.Emitters[id];
    Cell cell;
    if (!Encode(lights[id], packed, cell)) {
      return std::unexpected("light input has invalid photometry or an unrepresentable position");
    }
    if (Global(packed)) {
      ++plan.Globals;
      continue;
    }
    const double radius = 1.0 / packed.Place[3];
    plan.MaximumRange = std::max(
        plan.MaximumRange,
        std::nextafter(static_cast<float>(radius), std::numeric_limits<float>::infinity()));
    if (result.FullEnumeration) { continue; }
    Bounds bounds;
    if (!FootprintBounds(
            lights[id].PositionEcefM, radius, bounds, limits.SphereBoxTests / 2 - plan.Tests)) {
      result.FullEnumeration = true;
      continue;
    }
    plan.Tests += bounds.Tests;
  }
  return plan;
}

template <class Visit>
bool ForLightFootprints(std::span<const SubjectLight> lights,
                        std::span<const SubjectLightUniform> emitters,
                        uint64_t boundLimit,
                        uint64_t &tests,
                        Visit visit) {
  for (size_t id = 0; id < lights.size(); ++id) {
    if (Global(emitters[id])) { continue; }
    Bounds bounds;
    const double radius = 1.0 / emitters[id].Place[3];
    if (!FootprintBounds(lights[id].PositionEcefM, radius, bounds, boundLimit)) { return false; }
    const auto one = [&](const Cell &cell) { return visit(cell, static_cast<uint32_t>(id)); };
    if (!VisitFootprint(bounds, radius, lights[id].PositionEcefM, tests, one)) { return false; }
  }
  return true;
}

bool ScatterLights(LightCells &result,
                   std::span<const SubjectLight> lights,
                   uint64_t boundLimit,
                   std::span<uint32_t> cursors) {
  const size_t capacity = cursors.size();
  const auto scatter = [&](const Cell &cell, uint32_t id) {
    size_t slot = Hash(cell) & (capacity - 1);
    for (size_t probe = 0; probe < capacity; ++probe) {
      const size_t at = LightCells::kHeaderWords + LightCells::kCellWords * slot;
      if (result.Words[at + 4] == 0) { return false; }
      if (result.Words[at] == Key(cell[0]) && result.Words[at + 1] == Key(cell[1]) &&
          result.Words[at + 2] == Key(cell[2])) {
        if (cursors[slot] >= result.Words[at + 4]) { return false; }
        result.Words[result.Words[at + 3] + cursors[slot]++] = id;
        return true;
      }
      slot = (slot + 1) & (capacity - 1);
    }
    return false;
  };
  return ForLightFootprints(
      lights, result.Emitters, boundLimit, result.Costs.SphereBoxTests, scatter);
}

}

std::expected<LightCells, std::string> BuildLightCells(std::span<const SubjectLight> lights,
                                                       const LightCellLimits &limits) {
  constexpr size_t kMinimumWords = LightCells::kHeaderWords + 2 * LightCells::kCellWords;
  if (lights.size() > (std::numeric_limits<uint32_t>::max() - kMinimumWords) / 4 ||
      lights.size() > limits.EmitterBytes / sizeof(SubjectLightUniform) ||
      limits.IndexBytes / sizeof(uint32_t) < kMinimumWords ||
      lights.size() > (limits.IndexBytes / sizeof(uint32_t) - kMinimumWords) / 4) {
    return std::unexpected("light data exceeds the declared storage budget");
  }
  LightCells result;
  result.Emitters.resize(lights.size());
  const auto plan = PlanLights(lights, result, limits);
  if (!plan) { return std::unexpected(plan.error()); }
  size_t globals = plan->Globals;
  const float maxRange = plan->MaximumRange;
  CountedCells table;
  table.MaximumCapacity = std::bit_floor(limits.ScratchBytes / CountedCells::kLiveBytesPerSlot);
  if (!result.FullEnumeration) {
    result.FullEnumeration =
        !ForLightFootprints(lights,
                            result.Emitters,
                            plan->Tests,
                            result.Costs.SphereBoxTests,
                            [&](const Cell &key, uint32_t) { return table.Add(key); });
  }
  const size_t capacity = std::bit_ceil(std::max(size_t{2}, 2 * table.Count));
  const size_t words = kMinimumWords + 3 * lights.size() + globals + table.Assignments +
                       LightCells::kCellWords * (capacity - 2);
  if (result.FullEnumeration || words > limits.IndexBytes / sizeof(uint32_t) ||
      words > std::numeric_limits<uint32_t>::max()) {
    result.FullEnumeration = true;
    globals = lights.size();
    const CountedCells empty;
    PackWords(
        result, lights, empty, {}, {.Capacity = 2, .Globals = globals, .MaximumRange = maxRange});
    result.Costs.ScratchBytes =
        std::max(table.PeakBytes, table.Slots.capacity() * sizeof(CellSlot));
    result.Costs.Allocations = table.Allocations + 1 + (lights.empty() ? 0u : 1u);
    result.Costs.ScratchCopyBytes = table.CopyBytes;
    result.Costs.PackedBytes = result.Words.size() * sizeof(uint32_t) +
                               result.Emitters.size() * sizeof(SubjectLightUniform);
    return result;
  }
  std::vector<Cell> ordered;
  ordered.reserve(table.Count);
  for (const auto &slot : table.Slots) {
    if (slot.Count != 0) { ordered.push_back(slot.Key); }
  }
  std::ranges::sort(ordered);
  PackWords(result,
            lights,
            table,
            ordered,
            {.Capacity = capacity, .Globals = globals, .MaximumRange = maxRange});
  std::vector<uint32_t> cursors(ordered.empty() ? 0 : capacity, 0);
  if (!ScatterLights(result, lights, plan->Tests, cursors)) {
    return std::unexpected("native light scatter failed");
  }
  result.Costs.Assignments = table.Assignments;
  result.Costs.Allocations =
      table.Allocations + 1 + (lights.empty() ? 0u : 1u) + (ordered.empty() ? 0u : 2u);
  result.Costs.ScratchCopyBytes = table.CopyBytes;
  result.Costs.ScratchBytes =
      std::max(table.PeakBytes,
               table.Slots.capacity() * sizeof(CellSlot) + ordered.capacity() * sizeof(Cell) +
                   cursors.capacity() * sizeof(uint32_t));
  result.Costs.PackedBytes =
      result.Words.size() * sizeof(uint32_t) + result.Emitters.size() * sizeof(SubjectLightUniform);
  return result;
}

}
