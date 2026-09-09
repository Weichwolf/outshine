#ifndef OUTSHINE_IMPORT_SURFACE_SHAPED_H
#define OUTSHINE_IMPORT_SURFACE_SHAPED_H

#include <expected>
#include "Shape.h"
#include "Subject.h"

namespace outshine::Gltf {

[[nodiscard]] std::expected<Render::Shape, ClusterError> Shaped(const Subject &from,
                                                                Render::ShapeStore &into);

[[nodiscard]] std::expected<Render::Shape, ClusterError>
Shaped(const Subject &from, const outshine::Geometry &also, Render::ShapeStore &into);

}
#endif
