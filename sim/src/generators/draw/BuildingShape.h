/* WHAT AN OUTLINE IS, before anything is drawn on it. One footprint, read once: the ring in metres,
 * the box that fits it best, and from those two the storeys, the bay rhythm and the roof it carries.
 *
 * THE RIDGE DIRECTION IS THE LONG AXIS OF THE MINIMUM-AREA BOX, not a tag. The tile server's
 * buildings layer carries `height` and nothing else — no `building:levels`, no `roof:shape` — so
 * every choice below is a function of the outline's own geometry and of the height the streamer
 * resolved. When a tag ever arrives it replaces the guess at exactly one place: Analyse(). */
#ifndef BUILDINGSHAPE_H
#define BUILDINGSHAPE_H

#include <cstdint>
#include <vector>

#include "Span.h"

namespace outshine::Generators {

struct Plan2 {
  double E = 0.0, N = 0.0;
};

enum class RoofKind : uint8_t { Flat, Gable, Hip, Shed, Mansard, Sawtooth, Dome };

/* WHAT THE BUILDING IS FOR, as far as an outline can say it. Plan area, slenderness and height are
 * the whole evidence; the names are the classes those three separate, not a taxonomy of use. */
enum class BuildingUse : uint8_t { Outbuilding, House, Terrace, Block, Hall, Tower, Spire };

struct BuildingShape {
  std::vector<Plan2> Ring;   /* metres east/north of the first source point, CCW, not closed */
  double AreaM2 = 0.0;
  Plan2 Centre;
  Plan2 AxisU;               /* unit, along the long side of the minimum-area box */
  double HalfUm = 0.0, HalfVm = 0.0;
  double Fill = 0.0;         /* plan area over box area: 1 is a rectangle, pi/4 a circle */

  BuildingUse Use = BuildingUse::House;
  RoofKind Roof = RoofKind::Flat;
  int Storeys = 1;
  double FloorM = 2.9;       /* floor to floor; the storey the facade is banded on */
  double EavesM = 0.0;       /* wall top over the base */
  double RiseM = 0.0;        /* ridge over the eaves */
  double BreakFracV = 0.0;   /* mansard: where the pitch changes, as a fraction of the half-span */
  double BreakRiseM = 0.0;
  double PeriodM = 0.0;      /* sawtooth: one tooth, along the long axis */
  double BayM = 3.0;         /* the window rhythm the facade is divided into */
  double OverhangM = 0.0;    /* eaves and verge, beyond the wall */
  uint32_t Seed = 0;         /* derived from the outline's own place, so it is a fact about there */

  bool Valid() const { return Ring.size() >= 3 && AreaM2 > 1.0; }
  double TopM() const { return EavesM + RiseM; }
  /* The box frame, for the roof: v is the short axis and a gable's fall is along it. */
  Plan2 AxisV() const { return {-AxisU.N, AxisU.E}; }
  void ToBox(const Plan2 &p, double *u, double *v) const;
  Plan2 FromBox(double u, double v) const;
};

/* The outline as the source has it, in lat/lon pairs, plus what the streamer resolved for it. */
BuildingShape Analyse(Span<const double> ringLatLon, double heightM, bool heightMeasured);

}  // namespace outshine::Generators
#endif
