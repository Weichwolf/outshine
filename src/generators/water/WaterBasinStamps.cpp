#include "WaterBasinStamps.h"
#include "WaterSurfaceMesh.h"
#include "math/Vec3.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

namespace outshine::Generators {
void AppendWaterBasinStamps(const WaterAsset &water,
                            const TangentFrame &frame,
                            double depthM,
                            std::vector<EarthworkStamp> &stamps) {
  const auto *prepared = water.PreparedSurfaces();
  if (prepared == nullptr) { return; }
  const auto &mesh = prepared->Mesh;
  const auto source = TangentFrame::At(mesh.Anchor());
  const auto vertices = mesh.VerticesEnuM();
  for (const auto &face : mesh.Faces()) {
    std::array<Vec3, 3> placed;
    for (size_t corner = 0; corner < 3; ++corner) {
      const Vec3 &vertex = vertices[face.Vertices[corner]];
      if (source.Anchor() == frame.Anchor()) {
        placed[corner] = vertex;
      } else {
        const auto point = frame.ToLocalPosition(
            source.ToEcefPosition({.EastM = vertex[0], .NorthM = vertex[1], .UpM = vertex[2]}));
        placed[corner] = {{point.EastM, point.NorthM, point.UpM}};
      }
    }
    const Vec3 normal = Cross(placed[1] - placed[0], placed[2] - placed[0]);
    assert(normal[2] > 0);
    EarthworkStamp made;
    made.LowE = made.HighE = made.AtE = placed[0][0];
    made.LowN = made.HighN = made.AtN = placed[0][1];
    for (const auto &point : placed) {
      made.RingEastNorthM.insert(made.RingEastNorthM.end(), {point[0], point[1]});
      made.LowE = std::min(made.LowE, point[0]);
      made.HighE = std::max(made.HighE, point[0]);
      made.LowN = std::min(made.LowN, point[1]);
      made.HighN = std::max(made.HighN, point[1]);
    }
    made.PlateauM = placed[0][2] - depthM;
    made.SlopeE = -normal[0] / normal[2];
    made.SlopeN = -normal[1] / normal[2];
    made.YieldM = depthM;
    made.Kind = EarthworkKind::Basin;
    made.SeamEastNorthM = made.RingEastNorthM;
    stamps.push_back(std::move(made));
  }
}

}
