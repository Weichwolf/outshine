#ifndef OUTSHINE_ENGINE_STREAMING_STRUCTURECELLASSEMBLY_H
#define OUTSHINE_ENGINE_STREAMING_STRUCTURECELLASSEMBLY_H

#include "StructureBake.h"
#include <cstddef>
#include <cstdint>

namespace outshine {
class StructureCellAssembly {
public:
  explicit StructureCellAssembly(uint64_t cells) : Cells_(cells) {}

  void Collect(const Generators::BakedTile &child);

  [[nodiscard]] uint64_t Cells() const noexcept { return Cells_; }

  [[nodiscard]] uint64_t Remaining() const noexcept { return Cells_ & ~Collected_; }

  [[nodiscard]] const Generators::BakedTile &Metadata() const noexcept { return Metadata_; }

  [[nodiscard]] size_t Triangles() const noexcept { return Triangles_; }

  [[nodiscard]] size_t GeometryBytes() const noexcept { return GeometryBytes_; }

private:
  uint64_t Cells_ = 0;
  uint64_t Collected_ = 0;
  Generators::BakedTile Metadata_;
  size_t Triangles_ = 0;
  size_t GeometryBytes_ = 0;
};
}
#endif
