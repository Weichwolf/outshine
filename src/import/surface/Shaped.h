#ifndef OUTSHINE_IMPORT_SURFACE_SHAPED_H
#define OUTSHINE_IMPORT_SURFACE_SHAPED_H

#include "Shape.h"
#include "Subject.h"

namespace outshine::Gltf {

// A DOCUMENT'S GEOMETRY AS THE RENDERER'S OWN SHAPE. The importer knows the engine and the engine
// does not know the importer, so the translation stands on this side of the door -- and it is the
// same handover a generator makes, which is what makes them one path rather than two.
[[nodiscard]] Render::Shape Shaped(const Subject &from, Render::ShapeStore &into);

// AND FROM THE DOOR'S OWN GEOMETRY, which is where a CLIENT's subject arrives. This stood
// file-local in `Live.cpp` and had no second caller until a conformance runner needed the same
// layout over the same door type -- two copies of it is how two readings of one file start to
// disagree, so it stands beside the form it is the twin of.
[[nodiscard]] Render::Shape Shaped(const outshine::Geometry &from, Render::ShapeStore &into);

// AND BOTH AT ONCE, the subject's parts first. A driven world holds a subject and the ground it
// stands on, and they are one shape or the subject is not in the picture.
[[nodiscard]] Render::Shape Shaped(const Subject &from, const outshine::Geometry &also,
                                   Render::ShapeStore &into);

}
#endif
