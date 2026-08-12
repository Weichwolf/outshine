/* WHAT AN OUTLINE IS, before anything is drawn on it, and HOW MANY THINGS IT IS. One footprint, read
 * once: the ring in metres, the box that fits it best, and from those two the storeys, the bay
 * rhythm and the roof it carries.
 *
 * A FOOTPRINT IS NOT ONE BOX. An OSM outline is a plot, and what stands on a plot is a main block
 * with a lower wing, a row of plots that share walls, or a mass that steps back over its lower
 * storeys. So the outline is CUT along its own geometry — at a re-entrant corner, or across the long
 * axis at plot width — and each piece is a BuildingShape of its own with its own box, roof, storey
 * height and identity. A piece that shares a wall with its neighbour says so, and that wall carries
 * no openings.
 *
 * THE RIDGE DIRECTION IS THE LONG AXIS OF THE MINIMUM-AREA BOX, not a tag. The tile server's
 * buildings layer carries `height` and nothing else — no `building:levels`, no `roof:shape` — so
 * every choice below is a function of the outline's own geometry and of the height the streamer
 * resolved. When a tag ever arrives it replaces the guess at exactly one place: Finish(). */
#ifndef BUILDINGSHAPE_H
#define BUILDINGSHAPE_H

#include <cstdint>
#include <vector>

#include "Span.h"
#include "StructureMesher.h"

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
  /* Edge i runs Ring[i] -> Ring[i+1]; nonzero means it is shared with the piece next door, so it is
   * a party wall: no openings, no plinth, no pavement. */
  std::vector<uint8_t> Party;
  double AreaM2 = 0.0;
  Plan2 Centre;
  Plan2 AxisU;               /* unit, along the long side of the minimum-area box */
  double HalfUm = 0.0, HalfVm = 0.0;
  double Fill = 0.0;         /* plan area over box area: 1 is a rectangle, pi/4 a circle */

  BuildingUse Use = BuildingUse::House;
  RoofKind Roof = RoofKind::Flat;
  int Storeys = 1;
  double FloorM = 2.9;       /* floor to floor; the storey the facade is banded on */
  double FootM = 0.0;        /* over the structure's base: where this piece's own walls start */
  double EavesM = 0.0;       /* wall top over the FOOT */
  double RiseM = 0.0;        /* ridge over the eaves */
  double BreakFracV = 0.0;   /* mansard: where the pitch changes, as a fraction of the half-span */
  double BreakRiseM = 0.0;
  double PeriodM = 0.0;      /* sawtooth: one tooth, along the long axis */
  double BayM = 3.0;         /* the window rhythm the facade is divided into */
  double OverhangM = 0.0;    /* eaves and verge, beyond the wall */
  uint32_t Seed = 0;         /* derived from the outline's own place, so it is a fact about there */
  int Ident = 0;             /* this piece's own draw of colour and material, 0..kIdentCount-1 */
  int FrontEdge = -1;        /* the ring edge that looks at the street, or none */

  [[nodiscard]] bool Valid() const { return Ring.size() >= 3 && AreaM2 > 1.0; }
  [[nodiscard]] bool OnGround() const { return FootM <= 0.0; }
  double TopM() const { return FootM + EavesM + RiseM; }
  /* The box frame, for the roof: v is the short axis and a gable's fall is along it. */
  Plan2 AxisV() const { return {-AxisU.N, AxisU.E}; }
  void ToBox(const Plan2 &p, double *u, double *v) const;
  Plan2 FromBox(double u, double v) const;
};

/* One footprint's whole answer: the outline the ground works are laid against, and the pieces that
 * stand on it. Empty pieces mean the outline was not buildable. */
struct Massing {
  std::vector<Plan2> Outline;
  std::vector<BuildingShape> Parts;
};

/* The outline as the source has it, in lat/lon pairs, plus what the streamer resolved for it. */
Massing MassOf(Span<const double> ringLatLon, double heightM, bool heightMeasured,
               const Frontage &street);

}  // namespace outshine::Generators
#endif
