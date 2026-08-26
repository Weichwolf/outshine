#ifndef OUTSHINE_BASE_GEOMETRYHELD_H
#define OUTSHINE_BASE_GEOMETRYHELD_H

#include <string>
#include <vector>

#include <Geometry.h>

namespace outshine {

struct Geometry::Held {
  struct Piece {
    std::string Named;
    int Material;
    std::vector<float> PositionsM;
    std::vector<float> Normals;
    std::vector<float> Uv;
    std::vector<float> Uv1;
    std::vector<float> Tangents;
    std::vector<float> Colours;
    std::vector<uint32_t> Indices;
  };
  std::vector<Piece> Parts;
};

}

#endif
