/* ONE unit as the picture needs it: where it is, how it is turned, and where its moving parts stand.
 * Filled by FBWorld from the BORROWED registry's PUBLISHED pose (units/FBUnit.h) once per frame and
 * handed to FBRenderer; the renderer never reaches into any unit's FDM, and there is no path back.
 * Deliberately free of every simulation type — a plain record, so render/ carries no dependency on
 * units/ and the boundary is visible in the include list. */
#ifndef FBUNITDRAW_H
#define FBUNITDRAW_H

#include "FBUnitModel.h"

namespace FlightBox::Render {

struct FBUnitDraw {
  double Ecef[3] = {0, 0, 0};   /* absolute WGS84-ECEF of the model origin (the airframe's VRP) */
  /* Body -> ECEF, column-major and in glTF axes: column 0 = +X right, 1 = +Y up, 2 = +Z aft. */
  float Rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  float Art[(int)FBArtChannel::Count] = {};
  char Type[16] = {};   /* the module registry key; empty or unknown = nothing to draw */
};

} // namespace FlightBox::Render
#endif
