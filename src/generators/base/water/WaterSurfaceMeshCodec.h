#ifndef OUTSHINE_GENERATORS_BASE_WATER_WATERSURFACEMESHCODEC_H
#define OUTSHINE_GENERATORS_BASE_WATER_WATERSURFACEMESHCODEC_H

#include "WaterSurfaceMesh.h"
#include "BinaryValueArchive.h"

namespace outshine::Generators {
class WaterSurfaceMeshCodec {
public:
  [[nodiscard]] static bool Fields(BinaryValueWriter &out, const WaterSurfaceMesh &mesh);
  [[nodiscard]] static bool Fields(BinaryValueReader &in, WaterSurfaceMesh &mesh);
};
}
#endif
