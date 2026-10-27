#ifndef OUTSHINE_RENDER_GEOMETRY_GEOMETRYUPLOADTICKET_H
#define OUTSHINE_RENDER_GEOMETRY_GEOMETRYUPLOADTICKET_H

#include <cstdint>

namespace outshine::Render {

struct GeometryUploadTicket {
  uint64_t Generation = 0;
  bool NeedsFinish = false;
};

}
#endif
