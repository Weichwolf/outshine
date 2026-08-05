/* ONE effect billboard as the picture needs it: where it is, which way it is stretched, how big and
 * how bright. Filled by FBWorld from PUBLISHED unit data (units/FBUnit.h FBUnitSignature) once per
 * frame and handed to FBRenderer — the same one-way street FBUnitDraw travels, and free of every
 * simulation type for the same reason.
 *
 * SIZES ARE METRES, ALWAYS. An effect is drawn at its physical extent and shrinks with range like the
 * airframe beside it; the only concession is the sub-pixel floor in the shader, which keeps ENERGY
 * rather than size (a flare 20 km away is a dim sub-pixel dot, not a marker). That is what stops a
 * plume from telling the human player about a missile his sensors have not earned.
 * Vertrag + Messung: doc/render/units-visual.md. */
#ifndef FBSPRITEDRAW_H
#define FBSPRITEDRAW_H

#include <cstdint>

namespace FlightBox::Render {

/* What the fragment shader makes of the quad. The ordinal is baked into the instance, so adding one
 * costs a shader branch and nothing else. */
enum class FBSpriteKind : uint32_t {
  Flame = 0,   /* a nozzle plume: teardrop along Axis, hot core, additive */
  Flare,       /* a pyrotechnic point source: round, hard core, additive */
  Smoke,       /* a motor trail or a chaff cloud: round-to-stretched, alpha-blended */
};

struct FBSpriteDraw {
  double Ecef[3] = {0, 0, 0};   /* absolute WGS84-ECEF of the sprite's CENTRE */
  float Axis[3] = {0, 0, 1};    /* unit, world: the long axis, pointing from the ROOT toward the TIP */
  float HalfLenM = 1.0f;        /* half extent ALONG Axis; equal to RadiusM makes the quad square */
  float RadiusM = 1.0f;         /* half extent ACROSS it */
  float Color[3] = {1, 1, 1};   /* linear radiance, already premultiplied by Alpha for a Smoke sprite */
  float Alpha = 0.0f;           /* 0 = purely additive (nothing behind it is dimmed) */
  float Param = 0.0f;           /* kind-specific: Flame = plume hardness, Smoke = puff softness */
  uint32_t Kind = 0;            /* FBSpriteKind */
};

} // namespace FlightBox::Render
#endif
