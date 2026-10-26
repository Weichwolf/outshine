#ifndef OUTSHINE_RENDER_IMPOSTOR_IMPOSTOROPTICS_H
#define OUTSHINE_RENDER_IMPOSTOR_IMPOSTOROPTICS_H

#include "ImpostorAtlas.h"
#include "Geometry.h"

namespace outshine::Render {
[[nodiscard]] bool AddOpticalCardImages(Geometry &geometry,
                                        Material &material,
                                        const ImpostorAtlas &atlas,
                                        const ImpostorAtlas::View &source);
}
#endif
