#ifndef OUTSHINE_RENDER_IMPOSTOR_IMPOSTOROPTICS_H
#define OUTSHINE_RENDER_IMPOSTOR_IMPOSTOROPTICS_H

#include "ImpostorAtlas.h"
#include "scene/Geometry.h"

namespace outshine::Render {
[[nodiscard]] bool AddOpticalCardImages(Geometry &geometry,
                                        Material &material,
                                        const Content::ImpostorAtlas &atlas,
                                        const Content::ImpostorAtlas::View &source);
}
#endif
