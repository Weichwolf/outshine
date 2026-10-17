#ifndef OUTSHINE_EXPERIMENT_LOCAL_LIGHT_WORLD_BUILD_H
#define OUTSHINE_EXPERIMENT_LOCAL_LIGHT_WORLD_BUILD_H

#include "local_light_build.h"
#include <array>
#include <cstdint>
#include <expected>
#include <map>
#include <memory_resource>
#include <span>
#include <vector>

namespace LightWorld {
using LightBuild::Light;
using Cell = std::array<int32_t, 3>;
static_assert(sizeof(Cell) == 12);
using Vector = LightBuild::Vector;
enum class Error : uint8_t { Input, Domain, Size };
enum class Lookup : uint8_t { Hash, Sorted };

struct Builder {
  LightBuild::Memory Heap;
  std::pmr::vector<Light> Source{&Heap}, Local{&Heap};
  std::pmr::vector<Cell> Coordinates{&Heap}, Before{&Heap}, After{&Heap};
  std::pmr::map<Cell, std::pmr::vector<uint32_t>> Cells{&Heap};
  std::pmr::vector<uint32_t> Globals{&Heap}, Words{&Heap};
  Vector Translation{};
  uint32_t Width = 32;
  size_t IndexBytes = size_t{16} * 1024 * 1024;
  uint64_t Tests = 0, MembershipChanges = 0, Assignments = 0;
  bool Overflow = false;

  [[nodiscard]] std::expected<void, Error>
  Build(std::span<const Light> lights, uint32_t width, size_t budget, Vector translation);
  [[nodiscard]] std::expected<void, Error> Update(uint32_t id, const Light &light);
  [[nodiscard]] std::expected<void, Error> Pack(Lookup method);

private:
  [[nodiscard]] std::expected<Cell, Error> Position(const Light &light) const;
  [[nodiscard]] bool Footprint(const Light &light, std::pmr::vector<Cell> &result);
  void Encode(uint32_t id);
  void Fallback();
  void Insert(const Cell &cell, uint32_t id);
  void Remove(const Cell &cell, uint32_t id);
};
}
#endif
