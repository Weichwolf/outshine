#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTUREPOSITIONPALETTE_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTUREPOSITIONPALETTE_H

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace outshine {
struct Raised;
}

namespace outshine::Generators::StructureBinary {
class Reader;
class Writer;

struct PositionPalette {
  using Position = std::array<uint32_t, 3>;

  [[nodiscard]] static std::optional<PositionPalette> Make(const Raised &geometry);
  [[nodiscard]] bool Write(Writer &out, const Raised &geometry) const;
  [[nodiscard]] static bool Read(Reader &in, Raised &geometry);

  std::vector<Position> Positions;
  std::vector<uint32_t> References;
};
}

#endif
