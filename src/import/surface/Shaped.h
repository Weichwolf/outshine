#ifndef OUTSHINE_IMPORT_SURFACE_SHAPED_H
#define OUTSHINE_IMPORT_SURFACE_SHAPED_H

#include "Shape.h"
#include "Subject.h"

namespace outshine::Gltf {

// A DOCUMENT'S GEOMETRY AS THE RENDERER'S OWN SHAPE. The importer knows the engine and the engine
// does not know the importer, so the translation stands on this side of the door -- and it is the
// same handover a generator makes, which is what makes them one path rather than two.
[[nodiscard]] Render::Shape Shaped(const Subject &from, Render::ShapeStore &into);

}
#endif
