#ifndef OUTSHINE_RENDER_VIEWING_H
#define OUTSHINE_RENDER_VIEWING_H

#include <cstdint>

namespace outshine::Render {

// WHERE THE PICTURE IS SEEN FROM, and it belongs to the renderer rather than to a file format.
// The importer's own viewpoint type stood here: the render tier held a type from the glTF reader
// and used it as a
// plain data carrier -- no method of it was ever called from `src/render/`. A camera is not a glTF
// concept, it is the thing every renderer has; Filament spells it `Camera` at its door and Unreal's
// FSceneView is its own type with the importer nowhere near it.
//
// THE IMPORTER KEEPS ITS OWN, and that is not a duplicate. A glTF camera NODE is a real glTF thing
// and the importer's viewpoint reads it -- `Subject::Frame` and `DeclaredPlacement` both produce
// one. What
// changes is that the conversion happens ONCE, where a file's camera enters, instead of the whole
// render tier speaking the file's dialect.
enum class CameraKind : uint8_t { Perspective, Orthographic };

struct Viewpoint {
  double EyeM[3] = {0, 0, 0};
  double Forward[3] = {0, 0, -1};
  double Right[3] = {1, 0, 0};
  double Up[3] = {0, 1, 0};

  CameraKind Kind = CameraKind::Perspective;
  double YfovRad = 0;
  double XMagM = 0;
  double YMagM = 0;
  double ZNearM = 0;
  double ZFarM = 0;

  // AIMED FROM A PLACE AT A PLACE. Fifteen lines of cross products that belong to a camera, and the
  // engine called them through the importer's copy for want of its own.
  [[nodiscard]] static bool LookAt(const double eyeM[3], const double aimM[3], double rollRad,
                                   Viewpoint &out);
};

}
#endif
