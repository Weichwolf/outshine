#include "WaterSurfaceMeshCodec.h"
#include "WaterAssetFormat.h"
#include "Earth.h"
#include "PlanHierarchy.h"
#include "math/Vec3.h"
#include <cstdint>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
bool Vertices(auto &archive, auto &vertices) {
  return WaterFormat::Records<3 * sizeof(double)>(
      archive, vertices, [](auto &out, auto &point) { return out(point.Axis); });
}

bool Faces(auto &archive, auto &faces) {
  return WaterFormat::Records<4 * sizeof(uint32_t)>(
      archive, faces, [](auto &out, auto &face) { return out(face.Vertices, face.Surface); });
}

bool Nodes(auto &archive, auto &nodes) {
  return WaterFormat::Records<6 * sizeof(double) + 4 * sizeof(uint32_t)>(
      archive, nodes, [](auto &out, auto &node) {
        return out(node.Bounds.Min.Axis,
                   node.Bounds.Max.Axis,
                   node.First,
                   node.Count,
                   node.Left,
                   node.Right);
      });
}
}

bool WaterSurfaceMeshCodec::Fields(BinaryValueWriter &out, const WaterSurfaceMesh &mesh) {
  const auto anchor = mesh.Anchor();
  auto vertices = mesh.VerticesEnuM();
  auto faces = mesh.Faces();
  auto nodes = mesh.Hierarchy().Nodes();
  return out(anchor.LongitudeDeg, anchor.LatitudeDeg) && Vertices(out, vertices) &&
         Faces(out, faces) && Nodes(out, nodes) &&
         out.Array<uint32_t>(mesh.Hierarchy().SourceOrder());
}

bool WaterSurfaceMeshCodec::Fields(BinaryValueReader &in, WaterSurfaceMesh &mesh) {
  LongitudeLatitude anchor;
  std::vector<Vec3> vertices;
  std::vector<WaterSurfaceMesh::Face> faces;
  std::vector<PlanHierarchy::Node> nodes;
  std::vector<uint32_t> order;
  if (!in(anchor.LongitudeDeg, anchor.LatitudeDeg) || !Vertices(in, vertices) ||
      !Faces(in, faces) || !Nodes(in, nodes) || !in.Array(order)) {
    return false;
  }
  auto loaded = WaterSurfaceMesh::Restore(
      anchor, std::move(vertices), std::move(faces), std::move(nodes), std::move(order));
  if (!loaded) { return false; }
  mesh = std::move(*loaded);
  return true;
}
}
