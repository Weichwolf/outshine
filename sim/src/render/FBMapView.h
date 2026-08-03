/* FlightBox — FBMapView: the tactical map's WINDOW ONTO THE WORLD, and its projection.
 *
 * WEB MERCATOR, because the ground under the symbols is a Web-Mercator raster sheet: the OSM bakes
 * fb-tiles serves (/bake/osm/z/x/y) are cut on that grid, so any other projection would put a symbol
 * on the wrong piece of the sheet by a margin that grows with the span. A local-ENU projection (the
 * map's first one) is off by tan(lat)*d/R of the north-south distance from the centre -- 3.5 px at a
 * 46 km span, 10 px at 200 km, at 720p.
 *
 * IT IS A VALUE TYPE and it is VIEW state: a pan, a zoom and a frame size. Nothing here is simulation
 * state and nothing here may be written from a sim tick. */
#ifndef FBMAPVIEW_H
#define FBMAPVIEW_H

namespace FlightBox::Render {

/* 2*pi*a, WGS-84 semi-major axis — the length of the equator, which is what one Mercator unit of x
 * measures. Web Mercator is spherical by definition, so `a` and not the meridian circumference. */
inline constexpr double kMercEquatorM = 40075016.6855785;

struct FBMapView {
  int    Width = 0, Height = 0;
  /* WHERE THE CAMERA'S BORESIGHT LANDS ON THIS FRAME, in pixels, and it is NOT the frame centre: the
   * renderer shifts the scene up by the MFD bank's third of the height (`FBRenderer::ViewShiftNdc`),
   * so the point the camera is over sits at `ViewH/2`. Drawing the map's centre at `Height/2` instead
   * put every symbol 115 px — 4.1 km at a 46 km span — SOUTH of the ground it was measured over.
   * 0 = take the frame centre, which is right only for a frame with no bank. */
  float  CentreXPx = 0.0f, CentreYPx = 0.0f;
  double CentreLatDeg = 0.0, CentreLonDeg = 0.0;
  /* The drawn width in GROUND metres at the centre latitude. Away from the centre Mercator stretches,
   * which is why this is the scale bar's number and not the projection's. */
  double SpanM = 160000.0;

  float CentreX() const { return CentreXPx > 0.0f ? CentreXPx : (float)Width * 0.5f; }
  float CentreY() const { return CentreYPx > 0.0f ? CentreYPx : (float)Height * 0.5f; }

  /* Frame pixels per FULL Mercator unit (the whole world across). At latitude phi one Mercator unit of
   * x is kMercEquatorM*cos(phi) ground metres, and the frame shows SpanM of them over Width pixels. */
  double PxPerMerc() const;
  /* The OSM zoom whose `ts`-texel tile lands closest to its native size on this frame — the level that
   * keeps the sheet sharp. Clamped to what fb-tiles bakes. */
  int SheetZoom(int ts) const;

  void Project(double latDeg, double lonDeg, float &x, float &y) const;
  void Unproject(float x, float y, double &latDeg, double &lonDeg) const;
};

/* The Web-Mercator unit square, both directions. Latitude is clamped to the projection's own
 * +-85.0511 deg limit, where y leaves [0,1]. */
double FBMercX(double lonDeg);
double FBMercY(double latDeg);
double FBMercLon(double x);
double FBMercLat(double y);

/* The span a given OSM zoom draws at 1 texel per pixel — the inverse of SheetZoom. */
double FBMapSpanForZoom(int zoom, int widthPx, int ts, double centreLatDeg);

/* [SET] fb-tiles bakes OSM down to z15 (FBTerrainLoader's provider_terrain_max_zoom); below z3 a
 * 1280 px frame holds more than one world. */
inline constexpr int kMapZoomMin = 3;
inline constexpr int kMapZoomMax = 15;

} // namespace FlightBox::Render
#endif
