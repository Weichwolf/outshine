#ifndef OUTSHINE_RENDER_STAGES_MESHINPUT_H
#define OUTSHINE_RENDER_STAGES_MESHINPUT_H

#include <cstdint>
#include <span>

#include "Mat4.h"
#include "Vec3.h"
#include "DrawList.h"
#include "ClusterDag.h"

namespace outshine::Render {

// CPU sources live through the call; GPU work uses owned staging.
struct VertexSource {
  const float *From = nullptr;
  void (*Writes)(const void *carrying, float *into, uint32_t floats) = nullptr;
  const void *Carrying = nullptr;

  [[nodiscard]] bool HasSource() const { return From != nullptr || Writes != nullptr; }
};

struct VertexInput {
  VertexSource Verts;

  std::span<const float> Positions;
  VertexSource Uv;

  VertexSource Uv1;
  VertexSource Normals;

  VertexSource Tangents;

  VertexSource Colours;
  VertexSource Emitted;

  VertexSource PrevVerts;
  uint32_t VertexCount = 0;
  Vec3 Anchor = {{0, 0, 0}};
  Mat4 Model = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
};

struct MeshInput : VertexInput {
  const uint32_t *Indices = nullptr;
  uint32_t IndexCount = 0;
  const DrawList *Draws = nullptr;

  std::span<const DagCluster> Clusters;

  std::span<const float> ClusterSpheres;
};

}

#endif
