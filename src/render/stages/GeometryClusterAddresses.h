#ifndef OUTSHINE_RENDER_STAGES_GEOMETRYCLUSTERADDRESSES_H
#define OUTSHINE_RENDER_STAGES_GEOMETRYCLUSTERADDRESSES_H
#include "GeometryHierarchy.h"
#include <span>
#include <vector>

namespace outshine::Render {
[[nodiscard]] std::vector<GeometryClusterAddress>
BuildClusterAddresses(std::span<const GeometryRoot> roots,
                      std::span<const GeometryClusterTemplate> templates);
}
#endif
