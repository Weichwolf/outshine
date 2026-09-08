#ifndef OUTSHINE_IMPORT_SURFACE_SHAPED_H
#define OUTSHINE_IMPORT_SURFACE_SHAPED_H

#include "Shape.h"
#include "Subject.h"

namespace outshine::Gltf {

[[nodiscard]] Render::Shape Shaped(const Subject &from, Render::ShapeStore &into);

[[nodiscard]] Render::Shape Shaped(const outshine::Geometry &from, Render::ShapeStore &into);

[[nodiscard]] Render::Shape
Shaped(const Subject &from, const outshine::Geometry &also, Render::ShapeStore &into);

}
#endif
