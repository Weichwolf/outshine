#ifndef OUTSHINE_GENERATORS_ROAD_MESHING_ROADRIBBONMESH_H
#define OUTSHINE_GENERATORS_ROAD_MESHING_ROADRIBBONMESH_H

#include "RoadDisplaySampler.h"
#include "RoadMesher.h"
#include "Ribbon.h"

namespace outshine::Generators {
[[nodiscard]] RoadDisplayContacts RoadRibbonContacts(const RoadSweep &surface);
// The ribbon is woven; appends source-frame vertices and binds complete native contacts.
void AppendRoadRibbon(const Ribbon &ribbon, const RoadSweep &surface, RoadMeshBuffers &mesh);
}

#endif
