#ifndef OUTSHINE_RENDER_IMPOSTOR_SCENEMATERIALFIELDVIEW_H
#define OUTSHINE_RENDER_IMPOSTOR_SCENEMATERIALFIELDVIEW_H
#include "Extent.h"
#include "Mat4.h"
#include "RadianceImage.h"
#include "Viewing.h"

namespace outshine::Render {
struct SceneMaterialFieldView {
  Viewpoint Capture;
  Mat4 SceneFromField;
  Extent Target;
  RadianceEncoding Encoding = RadianceEncoding::MipmappedTexels;
};
}
#endif
