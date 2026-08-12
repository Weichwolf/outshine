#include "BuildingMesh.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "BuildingShape.h"
#include "Geodesy.h"
#include "RoofSurface.h"

namespace outshine::Generators {

namespace {

/* [SET] the wall is put 0.30 m into the ground so a plan standing on a terrain facet cannot show
 * daylight under itself where the facet falls away from the corner the base was sampled at. */
constexpr double kSinkM = 0.30;
/* [SET] rafter, boarding and covering: 0.20 m is what a German pantile roof measures from the
 * underside of the soffit to the top of the tile, and it is what makes the verge a board rather
 * than a knife edge. */
constexpr double kSlabM = 0.20;
constexpr double kParapetThickM = 0.32;
constexpr double kCorniceM = 0.16;
constexpr double kChimneyWideM = 0.55;
constexpr double kChimneyOverRidgeM = 0.85;
/* [SET] the base course. A German Sockel runs 0.40-0.70 m and stands 6-10 cm proud of the render;
 * it exists because splash off the pavement destroys lime render, and it is what stops a wall from
 * meeting the ground at a razor edge. Measured from the HIGHEST ground on the ring, not from the
 * base: the base is the lowest corner, and a plot with half a metre of fall across it buried the
 * whole course except at that one corner. */
constexpr double kPlinthM = 0.50;
constexpr double kPlinthProudM = 0.09;
/* [SET] a kerb: 0.12 m upstand and a 0.16 m coping. The upstand is also what lifts the pavement off
 * the terrain mesh, so the two surfaces cannot fight for the same depth. */
constexpr double kKerbUpM = 0.12;
constexpr double kKerbTopM = 0.16;
constexpr double kKerbSkirtM = 0.10;
/* A pavement narrower than this is the wall's own shadow gap and neither is a footway. The kerb line
 * is a CHORD of the way, so a corner of a long frontage on a bend reads much further back than the
 * middle does; rejecting on the far corner threw away most of a town's frontages, and clamping the
 * band instead keeps the geometry a footway rather than a forecourt. */
constexpr double kPavementLeastM = 0.6;
constexpr double kPavementMostM = 24.0;
constexpr double kFootwayMostM = 5.0;

struct Vtx {
  Plan2 P;
  double Z = 0.0;   /* metres over the structure's base */
  float U = 0.0f, V = 0.0f;
};

FacadeStyle StyleOf(BuildingUse use) {
  switch (use) {
    case BuildingUse::Outbuilding: return FacadeStyle::Outbuilding;
    case BuildingUse::Terrace:     return FacadeStyle::Terrace;
    case BuildingUse::Block:       return FacadeStyle::Block;
    case BuildingUse::Hall:        return FacadeStyle::Hall;
    case BuildingUse::Tower:       return FacadeStyle::Tower;
    case BuildingUse::Spire:       return FacadeStyle::Spire;
    case BuildingUse::House:       break;
  }
  return FacadeStyle::House;
}

double EavesZ(const BuildingShape &s) { return s.FootM + s.EavesM; }

Vtx Wall(const BuildingShape &s, const Plan2 &p, double z, double bays, Standing stand) {
  return {p, z, FacadeUvX(StyleOf(s.Use), stand, (float)bays),
          FacadeUvY(s.Ident, (float)((z - s.FootM) / s.FloorM))};
}

Vtx Face(const BuildingShape &s, const Plan2 &p, double z, Facade kind) {
  return {p, z, FaceUvX(kind, s.Ident), (float)z};
}

/* WHERE THE OUTLINE IS, once: the tangent frame at the ring's first corner. A building spans at most
 * a couple of hundred metres, over which the sphere departs from this plane by d^2/2R — 0.8 mm at
 * 100 m — so one frame per structure is exact to well inside the vertex format. */
class Site {
public:
  Site(const StructurePlan &plan, std::vector<float> &soup) : Soup_(soup) {
    const double lat = plan.RingLatLon[0], lon = plan.RingLatLon[1];
    double origin[3];
    GeoToEcef(lat, lon, plan.BaseAslM, origin);
    EnuAxesEcef(lat, lon, East_, North_, Up_);
    for (int c = 0; c < 3; c++) Origin_[c] = origin[c] - plan.AnchorEcef[c];
  }

  void Tri(const Vtx &a, const Vtx &b, const Vtx &c) {
    const double e1 = b.P.E - a.P.E, n1 = b.P.N - a.P.N, z1 = b.Z - a.Z;
    const double e2 = c.P.E - a.P.E, n2 = c.P.N - a.P.N, z2 = c.Z - a.Z;
    double nrm[3] = {n1 * z2 - z1 * n2, z1 * e2 - e1 * z2, e1 * n2 - n1 * e2};
    const double len = std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (len < 1.0e-9) return;
    for (int c2 = 0; c2 < 3; c2++) nrm[c2] /= len;
    Push(a, nrm);
    Push(b, nrm);
    Push(c, nrm);
  }

  void Quad(const Vtx &a, const Vtx &b, const Vtx &c, const Vtx &d) {
    Tri(a, b, c);
    Tri(a, c, d);
  }

private:
  void Push(const Vtx &v, const double nrm[3]) {
    for (int c = 0; c < 3; c++)
      Soup_.push_back((float)(Origin_[c] + v.P.E * East_[c] + v.P.N * North_[c] + v.Z * Up_[c]));
    Soup_.push_back(v.U);
    Soup_.push_back(v.V);
    for (int c = 0; c < 3; c++)
      Soup_.push_back((float)(nrm[0] * East_[c] + nrm[1] * North_[c] + nrm[2] * Up_[c]));
  }

  std::vector<float> &Soup_;
  double Origin_[3], East_[3], North_[3], Up_[3];
};

/* THE TERRAIN UNDER THE PLOT, as a plane through the corners the streamer sampled. A pavement four
 * metres wide has to follow the ground and a single base height cannot express that; a plane can, it
 * costs one 3x3 solve per footprint, and over a plot it is what the DEM's own posting resolves
 * anyway. Metres over the structure's base, which is the LOWEST corner. */
class Site2Ground {
public:
  Site2Ground(Span<const double> ringLatLon, Span<const double> cornerAslM, double baseAslM) {
    const size_t n = std::min(cornerAslM.Size(), ringLatLon.Size() / 2);
    if (n < 3) return;
    double m[3][4] = {};
    for (size_t k = 0; k < n; k++) {
      double e = 0.0, nn = 0.0;
      EnuOffsetM(ringLatLon[0], ringLatLon[1], ringLatLon[k * 2], ringLatLon[k * 2 + 1], e, nn);
      const double z = cornerAslM[k] - baseAslM;
      const double b[3] = {1.0, e, nn};
      for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) m[r][c] += b[r] * b[c];
        m[r][3] += b[r] * z;
      }
    }
    for (int c = 0; c < 3; c++) {
      int piv = c;
      for (int r = c + 1; r < 3; r++)
        if (std::fabs(m[r][c]) > std::fabs(m[piv][c])) piv = r;
      if (std::fabs(m[piv][c]) < 1.0e-6) return;
      for (int k = 0; k < 4; k++) std::swap(m[c][k], m[piv][k]);
      for (int r = 0; r < 3; r++) {
        if (r == c) continue;
        const double f = m[r][c] / m[c][c];
        for (int k = c; k < 4; k++) m[r][k] -= f * m[c][k];
      }
    }
    Const_ = m[0][3] / m[0][0];
    SlopeE_ = m[1][3] / m[1][1];
    SlopeN_ = m[2][3] / m[2][2];
  }

  double At(const Plan2 &p) const { return Const_ + SlopeE_ * p.E + SlopeN_ * p.N; }

private:
  double Const_ = 0.0, SlopeE_ = 0.0, SlopeN_ = 0.0;
};

double EdgeLength(const Plan2 &p, const Plan2 &q) { return std::hypot(q.E - p.E, q.N - p.N); }

Plan2 Along(const Plan2 &p, const Plan2 &q, double t) {
  return {p.E + (q.E - p.E) * t, p.N + (q.N - p.N) * t};
}

/* Whole bays, so a pier lands on both corners and a window is never cut by one. A face too narrow
 * for an opening gets no rhythm at all, which the facade reads as a blind wall. */
double BaysOn(double lengthM, double bayM) {
  if (lengthM < 1.9) return 0.0;
  return std::max(1.0, std::round(lengthM / bayM));
}

void WallPanel(const BuildingShape &s, const Plan2 &p, const Plan2 &q, double bay0, double bay1,
               double lowZ, double highZ, Standing stand, Site &site) {
  site.Quad(Wall(s, p, lowZ, bay0, stand), Wall(s, q, lowZ, bay1, stand),
            Wall(s, q, highZ, bay1, stand), Wall(s, p, highZ, bay0, stand));
}

/* THE DOOR IS ONE BAY OF THE FRONT, cut out of the face as its own panel. A hash over the whole face
 * would put the entrance on whichever wall the rhythm happened to land on; splitting the face is
 * what makes "one door, on the street" a statement of geometry rather than of luck. */
void FrontWall(const BuildingShape &s, const Plan2 &p, const Plan2 &q, double bays, double lowZ,
               double highZ, Site &site) {
  const double door = std::floor(0.5 * bays);
  const double t0 = door / bays, t1 = (door + 1.0) / bays;
  const Plan2 a = Along(p, q, t0), b = Along(p, q, t1);
  if (door > 0.0) WallPanel(s, p, a, 0.0, door, lowZ, highZ, Standing::Front, site);
  WallPanel(s, a, b, door, door + 1.0, lowZ, highZ, Standing::Entrance, site);
  if (door + 1.0 < bays)
    WallPanel(s, b, q, door + 1.0, bays, lowZ, highZ, Standing::Front, site);
}

void Walls(const BuildingShape &s, double lowZ, Site &site) {
  const size_t n = s.Ring.size();
  const double topZ = EavesZ(s);
  for (size_t i = 0; i < n; i++) {
    const Plan2 &p = s.Ring[i], &q = s.Ring[(i + 1) % n];
    const double len = EdgeLength(p, q);
    if (len < 0.05) continue;
    const double bays = s.Party[i] ? 0.0 : BaysOn(len, s.BayM);
    if ((int)i == s.FrontEdge && bays >= 2.0) {
      FrontWall(s, p, q, bays, lowZ, topZ, site);
      continue;
    }
    WallPanel(s, p, q, 0.0, bays, lowZ, topZ,
              (int)i == s.FrontEdge ? Standing::Entrance : Standing::Back, site);
  }
}

/* THE BASE COURSE, as relief. A change of colour alone leaves the wall meeting the ground at one
 * plane; nine centimetres of stone standing proud puts a cast shadow line along the foot of every
 * facade, and that line is what the eye reads as contact. A party wall gets none — there is no
 * outside of it to weather. */
void Plinth(const BuildingShape &s, double topZ, Site &site) {
  const std::vector<Plan2> out = RoofSurface::Widened(s.Ring, kPlinthProudM);
  if (out.size() != s.Ring.size()) return;
  const size_t n = s.Ring.size();
  const double lowZ = -kSinkM;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    if (s.Party[i]) continue;
    site.Quad(Face(s, out[i], lowZ, Facade::Plinth), Face(s, out[j], lowZ, Facade::Plinth),
              Face(s, out[j], topZ, Facade::Plinth), Face(s, out[i], topZ, Facade::Plinth));
    site.Quad(Face(s, out[i], topZ, Facade::Ledge), Face(s, out[j], topZ, Facade::Ledge),
              Face(s, s.Ring[j], topZ, Facade::Ledge), Face(s, s.Ring[i], topZ, Facade::Ledge));
  }
}

/* The wall that closes a roof against the sky, wherever the roof stands over the outline: a gable,
 * a shed's high side and a sawtooth's glazing are the same statement made at different corners. */
void Gables(const BuildingShape &s, const RoofSurface &roof, Site &site) {
  const size_t n = s.Ring.size();
  const double eaves = EavesZ(s);
  for (size_t i = 0; i < n; i++) {
    const Plan2 &p = s.Ring[i], &q = s.Ring[(i + 1) % n];
    const double hp = std::max(roof.HeightAt(p), 0.0), hq = std::max(roof.HeightAt(q), 0.0);
    if (hp < 0.03 && hq < 0.03) continue;
    const double len = EdgeLength(p, q);
    if (len < 0.05) continue;
    const double bays = s.Party[i] ? 0.0 : BaysOn(len, s.BayM);
    site.Quad(Wall(s, p, eaves, 0.0, Standing::Back), Wall(s, q, eaves, bays, Standing::Back),
              Wall(s, q, eaves + hq, bays, Standing::Back),
              Wall(s, p, eaves + hp, 0.0, Standing::Back));
  }
}

void Covering(const BuildingShape &s, const RoofSurface &roof, const std::vector<Plan2> &plan,
              double deckZ, Site &site) {
  std::vector<Plan2> tris;
  roof.Cover(plan, tris);
  const Facade kind = s.Roof == RoofKind::Flat ? Facade::RoofFlat : Facade::RoofPitch;
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    Vtx v[3];
    for (int k = 0; k < 3; k++)
      v[k] = Face(s, tris[i + (size_t)k], deckZ + roof.HeightAt(tris[i + (size_t)k]) + kSlabM, kind);
    site.Tri(v[0], v[1], v[2]);
  }
}

/* The underside of the overhang and the board that closes it. This is the one detail that turns an
 * extrusion into a building at a hundred metres: it puts a hard shadow line along the whole eaves,
 * and the shadow is cast, not drawn. */
void Eaves(const BuildingShape &s, const RoofSurface &roof, const std::vector<Plan2> &wide,
           Site &site) {
  const size_t n = s.Ring.size();
  if (wide.size() != n) return;
  const double eaves = EavesZ(s);
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    const double zi = eaves + roof.HeightAt(wide[i]), zj = eaves + roof.HeightAt(wide[j]);
    const double ri = eaves + roof.HeightAt(s.Ring[i]), rj = eaves + roof.HeightAt(s.Ring[j]);
    site.Quad(Face(s, s.Ring[i], ri, Facade::Soffit), Face(s, s.Ring[j], rj, Facade::Soffit),
              Face(s, wide[j], zj, Facade::Soffit), Face(s, wide[i], zi, Facade::Soffit));
    site.Quad(Face(s, wide[i], zi, Facade::Trim), Face(s, wide[j], zj, Facade::Trim),
              Face(s, wide[j], zj + kSlabM, Facade::Trim), Face(s, wide[i], zi + kSlabM,
                                                                Facade::Trim));
  }
}

/* A CORNICE AND THE PARAPET OVER IT. The cornice is what a flat roof has instead of an eaves
 * overhang: a course standing 0.16 m proud of the wall, which puts the same hard cast shadow line
 * along the top of the facade that a verge puts under a pitched roof. The parapet then stands back
 * on it, so its own foot is in that shadow — and its top is the structure's top, which is why the
 * shape took the parapet out of the height instead of adding it on. */
void Crown(const BuildingShape &s, const std::vector<Plan2> &inner, const std::vector<Plan2> &out,
           Site &site) {
  const size_t n = s.Ring.size();
  const double eaves = EavesZ(s);
  const double band = eaves - 0.34, lo = eaves, hi = eaves + s.RiseM;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    site.Quad(Face(s, out[i], band, Facade::Ledge), Face(s, out[j], band, Facade::Ledge),
              Face(s, out[j], lo, Facade::Ledge), Face(s, out[i], lo, Facade::Ledge));
    site.Quad(Face(s, s.Ring[i], band, Facade::Soffit), Face(s, out[i], band, Facade::Soffit),
              Face(s, out[j], band, Facade::Soffit), Face(s, s.Ring[j], band, Facade::Soffit));
    site.Quad(Face(s, out[i], lo, Facade::Ledge), Face(s, out[j], lo, Facade::Ledge),
              Face(s, s.Ring[j], lo, Facade::Ledge), Face(s, s.Ring[i], lo, Facade::Ledge));

    site.Quad(Face(s, s.Ring[i], lo, Facade::Parapet), Face(s, s.Ring[j], lo, Facade::Parapet),
              Face(s, s.Ring[j], hi, Facade::Parapet), Face(s, s.Ring[i], hi, Facade::Parapet));
    site.Quad(Face(s, inner[j], lo, Facade::Parapet), Face(s, inner[i], lo, Facade::Parapet),
              Face(s, inner[i], hi, Facade::Parapet), Face(s, inner[j], hi, Facade::Parapet));
    site.Quad(Face(s, inner[i], hi, Facade::Ledge), Face(s, inner[j], hi, Facade::Ledge),
              Face(s, s.Ring[j], hi, Facade::Ledge), Face(s, s.Ring[i], hi, Facade::Ledge));
  }
}

void Box(Site &site, const BuildingShape &s, const Plan2 &centre, double halfU, double halfV,
         double lowZ, double highZ, Facade side) {
  Plan2 c[4];
  const Plan2 u{s.AxisU.E * halfU, s.AxisU.N * halfU};
  const Plan2 v{-s.AxisU.N * halfV, s.AxisU.E * halfV};
  c[0] = {centre.E - u.E - v.E, centre.N - u.N - v.N};
  c[1] = {centre.E + u.E - v.E, centre.N + u.N - v.N};
  c[2] = {centre.E + u.E + v.E, centre.N + u.N + v.N};
  c[3] = {centre.E - u.E + v.E, centre.N - u.N + v.N};
  for (int i = 0; i < 4; i++) {
    const int j = (i + 1) % 4;
    site.Quad(Face(s, c[i], lowZ, side), Face(s, c[j], lowZ, side), Face(s, c[j], highZ, side),
              Face(s, c[i], highZ, side));
  }
  site.Quad(Face(s, c[0], highZ, Facade::Ledge), Face(s, c[1], highZ, Facade::Ledge),
            Face(s, c[2], highZ, Facade::Ledge), Face(s, c[3], highZ, Facade::Ledge));
}

[[nodiscard]] bool WantsChimney(const BuildingShape &s) {
  if (s.Roof != RoofKind::Gable && s.Roof != RoofKind::Hip && s.Roof != RoofKind::Mansard)
    return false;
  return s.Use == BuildingUse::House || s.Use == BuildingUse::Terrace ||
         s.Use == BuildingUse::Block;
}

/* A chimney and a lift overrun stand ABOVE the structure's top on purpose: the header's top is the
 * ridge, the parapet or the cap, and roof furniture is none of those. */
void Chimney(const BuildingShape &s, const RoofSurface &roof, Site &site) {
  const double along = (((double)(s.Seed >> 9 & 0xffu) / 255.0) - 0.5) * 1.30 * s.HalfUm;
  const Plan2 foot = s.FromBox(along, 0.0);
  const double eaves = EavesZ(s);
  const double stack = eaves + roof.HeightAt(foot) + kChimneyOverRidgeM;
  Box(site, s, foot, 0.5 * kChimneyWideM, 0.4 * kChimneyWideM, eaves, stack, Facade::Trim);
}

void RoofPlant(const BuildingShape &s, double deckZ, Site &site) {
  const double halfU = std::min(2.6, 0.30 * s.HalfUm), halfV = std::min(1.9, 0.30 * s.HalfVm);
  if (halfU < 0.9 || halfV < 0.7) return;
  const double along = (((double)(s.Seed >> 13 & 0xffu) / 255.0) - 0.5) * 0.9 * s.HalfUm;
  const Plan2 foot = s.FromBox(along, 0.0);
  Box(site, s, foot, halfU, halfV, deckZ, deckZ + 2.1, Facade::Metal);
}

/* The plinth's top, level, over the structure's base — one course of stone, and the ground rises
 * against it rather than it following the ground. */
double PlinthTopZ(const BuildingShape &s, const Site2Ground &ground) {
  double highest = 0.0;
  for (const Plan2 &p : s.Ring) highest = std::max(highest, ground.At(p));
  return highest + kPlinthM;
}

void RaisePart(const BuildingShape &s, const Site2Ground &ground, Site &site) {
  const RoofSurface roof(s);
  const double plinthZ = PlinthTopZ(s, ground);
  const double lowZ = s.OnGround() ? plinthZ : s.FootM - kSinkM;
  if (s.OnGround()) Plinth(s, plinthZ, site);
  Walls(s, lowZ, site);

  if (s.Roof == RoofKind::Flat) {
    const std::vector<Plan2> inner = RoofSurface::Widened(s.Ring, -kParapetThickM);
    const std::vector<Plan2> out = RoofSurface::Widened(s.Ring, kCorniceM);
    const bool crowned = inner.size() == s.Ring.size() && out.size() == s.Ring.size() &&
                         s.HalfVm > 2.2 && s.RiseM > 0.0;
    /* Without a crown the deck IS the top: the parapet was taken out of the height, so putting the
     * deck at the eaves would leave the roof a parapet's worth below what the query answers. */
    const double deckZ = crowned ? EavesZ(s) : EavesZ(s) + s.RiseM;
    Covering(s, roof, crowned ? inner : s.Ring, deckZ, site);
    if (crowned) Crown(s, inner, out, site);
    RoofPlant(s, deckZ, site);
    return;
  }

  const std::vector<Plan2> wide = RoofSurface::Widened(s.Ring, s.OverhangM);
  Covering(s, roof, wide.empty() ? s.Ring : wide, EavesZ(s), site);
  Gables(s, roof, site);
  if (!wide.empty()) Eaves(s, roof, wide, site);
  if (WantsChimney(s)) Chimney(s, roof, site);
}

double StandBack(const Frontage &street, const Plan2 &p) {
  return (p.E - street.KerbEm) * street.ToStreetE + (p.N - street.KerbNm) * street.ToStreetN;
}

Plan2 OntoKerb(const Frontage &street, const Plan2 &p, double back) {
  return {p.E - back * street.ToStreetE, p.N - back * street.ToStreetN};
}

/* THE FOOTWAY AND THE KERB THAT ENDS IT. The band is bounded by the building line on one side and
 * the edge of the carriageway on the other, so its width is DERIVED from where the house stands and
 * how wide the way declares itself — never set. The upstand is what lifts the band clear of the
 * terrain mesh underneath, so a kerb and a depth-fight are the same 12 cm. */
void Pavement(const BuildingShape &s, const Frontage &street, const Site2Ground &ground,
              double plinthZ, Site &site) {
  if (!street.Known || !s.OnGround()) return;
  const size_t n = s.Ring.size();
  for (size_t i = 0; i < n; i++) {
    if (s.Party[i]) continue;
    const Plan2 &p = s.Ring[i], &q = s.Ring[(i + 1) % n];
    const double e = q.E - p.E, nn = q.N - p.N, len = std::hypot(e, nn);
    if (len < 1.2) continue;
    if ((nn / len) * street.ToStreetE - (e / len) * street.ToStreetN < 0.35) continue;
    double bp = StandBack(street, p), bq = StandBack(street, q);
    if (bp > -kPavementLeastM || bq > -kPavementLeastM) continue;
    if (bp < -kPavementMostM && bq < -kPavementMostM) continue;
    bp = std::max(bp, -kFootwayMostM);
    bq = std::max(bq, -kFootwayMostM);

    const Plan2 pk = OntoKerb(street, p, bp + kKerbTopM), qk = OntoKerb(street, q, bq + kKerbTopM);
    const Plan2 pe = OntoKerb(street, p, bp), qe = OntoKerb(street, q, bq);
    const auto walk = [&](const Plan2 &at) {
      return std::min(ground.At(at) + kKerbUpM, plinthZ - 0.05);
    };
    const double zp = walk(p), zq = walk(q), zpk = walk(pk), zqk = walk(qk);
    site.Quad(Face(s, p, zp, Facade::Pavement), Face(s, pk, zpk, Facade::Pavement),
              Face(s, qk, zqk, Facade::Pavement), Face(s, q, zq, Facade::Pavement));
    site.Quad(Face(s, pk, zpk, Facade::Kerb), Face(s, pe, zpk, Facade::Kerb),
              Face(s, qe, zqk, Facade::Kerb), Face(s, qk, zqk, Facade::Kerb));
    site.Quad(Face(s, pe, zpk - kKerbUpM - kKerbSkirtM, Facade::Kerb),
              Face(s, qe, zqk - kKerbUpM - kKerbSkirtM, Facade::Kerb),
              Face(s, qe, zqk, Facade::Kerb), Face(s, pe, zpk, Facade::Kerb));
  }
}

}  // namespace

void BuildingMesh::Mesh(const StructurePlan &plan, std::vector<float> &soup) const noexcept {
  if (plan.RingLatLon.Size() < 6 || !plan.AnchorEcef) return;
  const Massing mass = MassOf(plan.RingLatLon, plan.HeightM, plan.HeightMeasured, plan.Street);
  if (mass.Parts.empty()) return;

  Site site(plan, soup);
  const Site2Ground ground(plan.RingLatLon, plan.CornerAslM, plan.BaseAslM);
  for (const BuildingShape &part : mass.Parts) {
    RaisePart(part, ground, site);
    Pavement(part, plan.Street, ground, PlinthTopZ(part, ground), site);
  }
}

}  // namespace outshine::Generators
