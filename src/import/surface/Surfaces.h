#ifndef OUTSHINE_IMPORT_SURFACE_SURFACES_H
#define OUTSHINE_IMPORT_SURFACE_SURFACES_H

#include <string>

#include "Document.h"
#include "Subject.h"
#include "Surfacing.h"

namespace outshine::Gltf {

void ResolveSurfaceTable(const Document &file, const Subject &geometry, bool carriesTransmission,
                         bool ownMaterials, Render::SurfaceTable &out);

[[nodiscard]] bool ResolveFileSurface(const Document &file, const Subject &geometry,
                                      Render::ColourFrom channel, Render::ColourCarrier carrier,
                                      Render::SurfaceTable &table, std::string &error);

}
#endif
