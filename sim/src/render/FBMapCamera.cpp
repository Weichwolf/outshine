#include "FBMapCamera.h"

#include <cmath>

namespace FlightBox::Render {

void FBMapCamera::SetFrame(int widthPx, int heightPx, float centreXPx, float centreYPx) {
  W = widthPx;
  H = heightPx;
  CxPx = centreXPx;
  CyPx = centreYPx;
}

void FBMapCamera::SetSpanM(double spanM) {
  if (spanM > 1.0) Span = spanM;
}

void FBMapCamera::Track(double latDeg, double lonDeg) {
  if (!Follow && Placed) return;
  LatDeg = latDeg;
  LonDeg = lonDeg;
  Placed = true;
}

void FBMapCamera::Recentre() { Follow = true; }

int FBMapCamera::Zoom() const { return View().SheetZoom(Ts); }

void FBMapCamera::SetZoom(int zoom) {
  if (zoom < kMapZoomMin) zoom = kMapZoomMin;
  if (zoom > kMapZoomMax) zoom = kMapZoomMax;
  Span = FBMapSpanForZoom(zoom, W > 0 ? W : 1280, Ts, LatDeg);
}

void FBMapCamera::PanPixels(double dxPx, double dyPx) {
  FBMapView v = View();
  double p = v.PxPerMerc();
  if (p < 1e-9) return;
  Follow = false;
  Placed = true;
  double mx = FBMercX(LonDeg) + dxPx / p;
  double my = FBMercY(LatDeg) + dyPx / p;
  if (my < 0.0) my = 0.0;   /* the poles are the sheet's edge, not a wrap */
  if (my > 1.0) my = 1.0;
  mx -= std::floor(mx);     /* longitude wraps; latitude does not */
  LonDeg = FBMercLon(mx);
  LatDeg = FBMercLat(my);
}

FBMapView FBMapCamera::View() const {
  FBMapView v;
  v.Width = W;
  v.Height = H;
  v.CentreXPx = CxPx;
  v.CentreYPx = CyPx;
  v.CentreLatDeg = LatDeg;
  v.CentreLonDeg = LonDeg;
  v.SpanM = Span;
  return v;
}

} // namespace FlightBox::Render
