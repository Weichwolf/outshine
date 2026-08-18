/* THE FRAMING RULE'S CONSTANTS, one declaration each. A scene that declares no camera gets its
 * viewpoint from these four numbers and from its own bounds, so two cases with the same subject get
 * the same picture and no viewpoint can be tuned into a pass (board:0083).
 *
 * Changing one moves every derived-camera case at once, which is the intended blast radius. */
#ifndef GLTF_FRAMING_H
#define GLTF_FRAMING_H

namespace outshine::Gltf {

/* Off every axis, so no face of a box is edge-on and no silhouette is degenerate. */
constexpr double kFramingAzimuthDeg = 35.0;   /* [SET] */
constexpr double kFramingElevationDeg = 20.0; /* [SET] */
/* THE ORDINARY REFERENCE LENS: 50 mm on a 24 mm frame height, which is 2*atan(12/50) = 39.6 deg. It
 * is written as the arithmetic rather than as 39.6 so the LENS is what is stated and the angle is
 * what falls out. The choice is the engine's and it is [SET]: a normal lens on a full frame puts no
 * perspective signature of its own into a subject that declares no camera. That several tools ship
 * the same lens as their factory default is agreement and not the reason -- nothing outside this
 * header decides these four numbers. */
constexpr double kFramingSensorHalfHeightMm = 12.0; /* [SET] */
constexpr double kFramingFocalLengthMm = 50.0;      /* [SET] */
/* The subject's bounding sphere spans 60 % of the frame's vertical extent. */
constexpr double kFramingFill = 0.6; /* [SET] */
/* The near plane cannot follow the subject all the way in: a camera inside its own bounding sphere
 * would put znear at or behind the eye. */
constexpr double kFramingNearFloorFraction = 0.001; /* [SET] */

} // namespace outshine::Gltf
#endif
