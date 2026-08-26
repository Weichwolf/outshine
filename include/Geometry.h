#ifndef OUTSHINE_GEOMETRY_H
#define OUTSHINE_GEOMETRY_H

#include <cstdint>
#include <span>
#include <string_view>

namespace outshine {

struct Part {
  std::string_view Named;
  int Material = -1;
  std::span<const float> PositionsM;
  std::span<const float> Normals;
  std::span<const float> Uv;
  std::span<const float> Uv1;
  std::span<const float> Tangents;
  std::span<const float> Colours;
  std::span<const uint32_t> Indices;
};

struct Geometry {
  std::span<const Part> Parts;
};

}

#endif
