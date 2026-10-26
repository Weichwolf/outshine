#ifndef OUTSHINE_RENDER_IMPOSTOR_IMPOSTORBAKER_H
#define OUTSHINE_RENDER_IMPOSTOR_IMPOSTORBAKER_H

#include "ImpostorAtlas.h"
#include "ImpostorAtlasShape.h"
#include "Mat4.h"
#include "Vec3.h"
#include "Geometry.h"

#include <optional>
#include <string>
#include <vector>

namespace outshine::Render {

struct ImpostorCaptureSource {
  Geometry Mesh;
  std::vector<Mat4> Instances;
};

struct ImpostorCapture {
  std::vector<ImpostorCaptureSource> Sources;
  Vec3 LeastM;
  Vec3 MostM;
};

class ImpostorBaker {
public:
  [[nodiscard]] static std::optional<ImpostorAtlas>
  Bake(ImpostorCapture capture, ImpostorAtlasShape shape, std::string &error);
};

}
#endif
