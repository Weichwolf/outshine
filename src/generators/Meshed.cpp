#include "Meshed.h"

namespace outshine::Generators {

bool Meshed::Take(std::string named, int material, const float *soup, size_t floats) {
  if (soup == nullptr || floats == 0) {
    Error_ = "a part of no vertices is a refusal, not an empty mesh";
    return false;
  }
  if (floats % kSoupFloatsPerVertex != 0) {
    Error_ = "a soup of " + std::to_string(floats) + " float(s) is not a whole number of " +
             std::to_string(kSoupFloatsPerVertex) + "-float vertices";
    return false;
  }
  const size_t vertices = floats / kSoupFloatsPerVertex;
  if (vertices % 3 != 0) {
    Error_ = "a soup of " + std::to_string(vertices) +
             " vertices is not a whole number of triangles";
    return false;
  }

  Reach reach;
  reach.FirstVertex = PositionsM_.size() / 3;
  reach.VertexCount = vertices;
  reach.FirstIndex = Index_.size();
  reach.IndexCount = vertices;
  reach.Material = material;

  PositionsM_.reserve(PositionsM_.size() + vertices * 3);
  Uv_.reserve(Uv_.size() + vertices * 2);
  NormalM_.reserve(NormalM_.size() + vertices * 3);
  Index_.reserve(Index_.size() + vertices);
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    const float *at = soup + vertex * kSoupFloatsPerVertex;
    PositionsM_.push_back(at[0]);
    PositionsM_.push_back(at[1]);
    PositionsM_.push_back(at[2]);
    Uv_.push_back(at[3]);
    Uv_.push_back(at[4]);
    NormalM_.push_back(at[5]);
    NormalM_.push_back(at[6]);
    NormalM_.push_back(at[7]);
    Index_.push_back((uint32_t)(reach.FirstVertex + vertex));
  }
  Named_.push_back(std::move(named));
  Reaches_.push_back(reach);
  return true;
}

Geometry Meshed::Handed() {
  Geometry out;
  for (size_t at = 0; at < Reaches_.size(); ++at) {
    const Reach &reach = Reaches_[at];
    const int part = out.Part(Named_[at], reach.Material);
    (void)out.Positions(part, std::span<const float>(PositionsM_.data() + reach.FirstVertex * 3,
                                                     reach.VertexCount * 3));
    (void)out.Texture(part, std::span<const float>(Uv_.data() + reach.FirstVertex * 2,
                                                   reach.VertexCount * 2));
    (void)out.Normals(part, std::span<const float>(NormalM_.data() + reach.FirstVertex * 3,
                                                   reach.VertexCount * 3));
    std::vector<uint32_t> run(reach.IndexCount);
    for (size_t step = 0; step < reach.IndexCount; ++step) {
      run[step] = (uint32_t)(Index_[reach.FirstIndex + step] - reach.FirstVertex);
    }
    (void)out.Triangles(part, std::span<const uint32_t>(run.data(), run.size()));
  }
  return out;
}

}
