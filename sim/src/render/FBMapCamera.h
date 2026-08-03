/* FlightBox — FBMapCamera: where the commander is LOOKING, and nothing else.
 *
 * Zoom and pan are ANSICHTS-Zustand. They are held here, in the client's view layer, and there is no
 * path from this class to the simulation: it takes the node's position as an argument and returns an
 * FBMapView. Nothing it holds is ever read by a tick.
 *
 * THE ZOOM LADDER IS THE OSM LADDER. A step is one OSM zoom level and the span it produces is the one
 * that draws that level's tiles at exactly one texel per pixel (FBMapSpanForZoom) — anything between
 * two levels resamples the sheet and softens it. A free span (the CLI's --map-span-km) is still
 * allowed; it simply lands between rungs, and the sheet picks the nearer level. */
#ifndef FBMAPCAMERA_H
#define FBMAPCAMERA_H

#include "FBMapView.h"

namespace FlightBox::Render {

class FBMapCamera {
public:
  /* The sheet's own tile edge in texels — the ladder is defined against it. */
  explicit FBMapCamera(int sheetTs) : Ts(sheetTs) {}

  void SetFrame(int widthPx, int heightPx, float centreXPx, float centreYPx);
  /* The free span the CLI hands in; the ladder snaps to it on the next zoom step. */
  void SetSpanM(double spanM);

  /* The node's live position, every frame. It moves the map only while FOLLOWING — a panned map is
   * the commander's own frame of reference and does not get dragged around by the aircraft. */
  void Track(double latDeg, double lonDeg);
  /* Back onto the node (the Home key). */
  void Recentre();
  bool Following() const { return Follow; }

  void ZoomIn() { SetZoom(Zoom() + 1); }
  void ZoomOut() { SetZoom(Zoom() - 1); }
  /* Pins the span to that rung: one sheet texel per frame pixel, clamped to what fb-tiles bakes. */
  void SetZoom(int zoom);
  int  Zoom() const;

  /* A drag or an arrow key, in FRAME pixels. Panning leaves follow mode by construction. */
  void PanPixels(double dxPx, double dyPx);

  FBMapView View() const;
  double SpanM() const { return Span; }

private:
  int    Ts;
  int    W = 0, H = 0;
  float  CxPx = 0.0f, CyPx = 0.0f;
  double Span = 46000.0;   /* [SET] the map's boot span, unchanged from the fixed one it replaces */
  double LatDeg = 0.0, LonDeg = 0.0;
  bool   Follow = true;
  bool   Placed = false;   /* no node seen yet: Track must seed the centre even while panned */
};

} // namespace FlightBox::Render
#endif
