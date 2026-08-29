#ifndef OUTSHINE_CONTENT_GLTF_TANGENTS_H
#define OUTSHINE_CONTENT_GLTF_TANGENTS_H

#include <cstdint>
#include <string>
#include <vector>

namespace outshine::Gltf {

struct TangentSubject {
  const double *PositionsM = nullptr;
  const double *Normals = nullptr;
  const double *Uv = nullptr;
  size_t VertexCount = 0;
  const uint32_t *Indices = nullptr;
  size_t IndexCount = 0;
};

[[nodiscard]] bool GenerateTangents(const TangentSubject &subject, std::vector<double> &out,
                                    std::string &error);

}
#endif
