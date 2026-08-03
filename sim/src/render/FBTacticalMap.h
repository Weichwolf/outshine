/* FlightBox — FBTacticalMap: the tactical map's SYMBOLOGY, and nothing else.
 *
 * It draws a core/FBForcePicture into a systems/FBHudGeometry — the same stroke-and-glyph buffer the
 * HUD and the MFD bank already fill, so the map costs the frame EXACTLY ZERO extra render passes. The
 * pass-count-per-frame contract is what makes that the right vehicle and not merely a convenient one
 * (doc/render/renderer.md, the pass topology).
 *
 * IT KNOWS NOTHING. Every symbol it draws comes out of one FBForceSymbol, which came out of one
 * published block. There is no path from here to the unit registry, to another unit's state or to the
 * world; a symbol this class cannot find in the picture it was handed is a symbol it does not draw.
 *
 * THE SYMBOL SET IS APP-6 / MIL-STD-2525, reduced to what this tree can honestly assert:
 *   FRIEND (air)   the standard's arc — a semicircle open at the bottom
 *   UNKNOWN (air)  the standard's quatrefoil, drawn as its four lobes
 *   a BEARING      a dashed ray from the observer with NO symbol at the end, because an RWR measured
 *                  no range and a point would be a claim
 * HOSTILE and SUSPECT have frames in the standard and none here: nothing in this tree may assert them
 * (doc/player-layer.md §9.3). If a symbol of that shape ever appears on this map, something upstream
 * started deriving allegiance from a sensor. */
#ifndef FBTACTICALMAP_H
#define FBTACTICALMAP_H

#include "FBForcePicture.h"
#include "FBHudGeometry.h"

namespace FlightBox::Render {

class FBTacticalMap {
public:
  /* THE WINDOW ONTO THE WORLD, in the same shape the renderer's own camera takes: a centre and a span.
   * SpanM is the width of the drawn rect in metres, so the scale is Width / SpanM px per metre. */
  struct FBMapView {
    int    Width = 0, Height = 0;
    /* WHERE THE CAMERA'S BORESIGHT LANDS ON THIS FRAME, in pixels, and it is NOT the frame centre: the
     * renderer shifts the scene up by the MFD bank's third of the height (`FBRenderer::ViewShiftNdc`),
     * so the point the camera is over sits at `ViewH/2`. Drawing the map's centre at `Height/2` instead
     * put every symbol 115 px — 4.1 km at a 46 km span — SOUTH of the ground it was measured over.
     * 0 = take the frame centre, which is right only for a frame with no bank. */
    float  CentreXPx = 0.0f, CentreYPx = 0.0f;
    double CentreLatDeg = 0.0, CentreLonDeg = 0.0;
    double SpanM = 160000.0;

    float CentreX() const { return CentreXPx > 0.0f ? CentreXPx : (float)Width * 0.5f; }
    float CentreY() const { return CentreYPx > 0.0f ? CentreYPx : (float)Height * 0.5f; }
  };

  /* [SET] Beyond this the datum is old enough that the net could not have refreshed it — three PPLI
   * cycles at 1 Hz (sensors/FBDatalinkSystem::kDropAfterCycles x kNetPeriodS). The symbol keeps its
   * shape and loses its fill: it is still the best thing anybody has. */
  static constexpr float kStaleS = 3.0f;
  /* [SET] ...and beyond THIS it is a datum with an age rather than a position: the radar's own coast
   * limit, measured in this tree's own logs as `coastS=12`. Drawn dashed, with the age printed. */
  static constexpr float kColdS = 12.0f;

  /* Which own unit the commander has selected. Empty = none; a callsign that is not in the picture
   * simply highlights nothing, because the map may not draw what the force does not hold. */
  void SetSelected(const char *callsign);
  const char *Selected() const { return Selected_; }
  /* One line of the commander's own state at the bottom of the frame — the last order and its answer,
   * handed in by whoever posted it. This class computes no order state of its own. */
  void SetStatus(const char *text);

  void Build(const FBForcePicture &pic, const FBMapView &v, Systems::FBHudGeometry &out) const;

  /* The map's own projection, published so a client can turn a cursor into a place with the identical
   * arithmetic the symbols were drawn with. */
  static void Project(const FBMapView &v, double latDeg, double lonDeg, float &x, float &y);
  static void Unproject(const FBMapView &v, float x, float y, double &latDeg, double &lonDeg);

private:
  void DrawFriendAir(Systems::FBHudGeometry &out, float x, float y, float r, float cr, float cg,
                     float cb) const;
  void DrawUnknownAir(Systems::FBHudGeometry &out, float x, float y, float r, float cr, float cg,
                      float cb) const;
  void DrawBearing(Systems::FBHudGeometry &out, const FBMapView &v, const FBForceSymbol &s) const;
  void DrawGrid(Systems::FBHudGeometry &out, const FBMapView &v) const;
  void DrawLegend(Systems::FBHudGeometry &out, const FBForcePicture &pic, const FBMapView &v) const;

  char Selected_[kForceLabelLen] = {};
  char Status_[96] = {};
};

} // namespace FlightBox::Render
#endif
