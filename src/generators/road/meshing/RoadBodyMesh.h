#ifndef OUTSHINE_GENERATORS_ROAD_MESHING_ROADBODYMESH_H
#define OUTSHINE_GENERATORS_ROAD_MESHING_ROADBODYMESH_H

#include "HeightSolid.h"
#include "RoadMesher.h"

namespace outshine::Generators {
[[nodiscard]] bool
AppendRoadBodyMesh(const HeightSolid &body, const Vec3f &colour, RoadMeshBuffers &mesh);
}
#endif
