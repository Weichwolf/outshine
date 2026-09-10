#ifndef OUTSHINE_GENERATORS_BUILDING_MESHED_H
#define OUTSHINE_GENERATORS_BUILDING_MESHED_H

#include <string>
#include <string_view>
#include <span>
#include "StoredVertex.h"

#include <scene/Geometry.h>

namespace outshine::Generators {

class Meshed {
public:
  [[nodiscard]] bool
  Take(std::string_view named, MaterialInstance material, std::span<const StoredVertex> soup);

  [[nodiscard]] size_t Parts() const { return static_cast<size_t>(Held_.parts()); }

  [[nodiscard]] Geometry Handed() { return std::move(Held_); }

  [[nodiscard]] const std::string &Error() const { return Error_; }

private:
  Geometry Held_;
  std::string Error_;
};

}
#endif
