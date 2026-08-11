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
/* [SET] a flat roof's parapet: 0.90 m is the German fall-protection minimum for a roof that is
 * walked on, and it is why a flat-roofed block reads as taller than its eaves. */
constexpr double kParapetM = 0.90;
constexpr double kParapetThickM = 0.32;
constexpr double kCorniceM = 0.16;
constexpr double kChimneyWideM = 0.55;
constexpr double kChimneyOverRidgeM = 0.85;

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

Vtx Wall(const BuildingShape &s, const Plan2 &p, double z, double bays) {
  return {p, z, FacadeUvX(StyleOf(s.Use), (float)bays), (float)(z / s.FloorM)};
}

Vtx Face(const Plan2 &p, double z, Facade kind) {
  return {p, z, -(float)(int)kind, (float)z};
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

double EdgeLength(const Plan2 &p, const Plan2 &q) { return std::hypot(q.E - p.E, q.N - p.N); }

/* Whole bays, so a pier lands on both corners and a window is never cut by one. A face too narrow
 * for an opening gets no rhythm at all, which the facade reads as a blind wall. */
double BaysOn(double lengthM, double bayM) {
  if (lengthM < 1.9) return 0.0;
  return std::max(1.0, std::round(lengthM / bayM));
}

void Walls(const BuildingShape &s, Site &site) {
  const size_t n = s.Ring.size();
  const double topZ = s.EavesM, botZ = -kSinkM;
  for (size_t i = 0; i < n; i++) {
    const Plan2 &p = s.Ring[i], &q = s.Ring[(i + 1) % n];
    const double len = EdgeLength(p, q);
    if (len < 0.05) continue;
    const double bays = BaysOn(len, s.BayM);
    site.Quad(Wall(s, p, botZ, 0.0), Wall(s, q, botZ, bays), Wall(s, q, topZ, bays),
              Wall(s, p, topZ, 0.0));
  }
}

/* The wall that closes a roof against the sky, wherever the roof stands over the outline: a gable,
 * a shed's high side and a sawtooth's glazing are the same statement made at different corners. */
void Gables(const BuildingShape &s, const RoofSurface &roof, Site &site) {
  const size_t n = s.Ring.size();
  for (size_t i = 0; i < n; i++) {
    const Plan2 &p = s.Ring[i], &q = s.Ring[(i + 1) % n];
    const double hp = std::max(roof.HeightAt(p), 0.0), hq = std::max(roof.HeightAt(q), 0.0);
    if (hp < 0.03 && hq < 0.03) continue;
    const double len = EdgeLength(p, q);
    if (len < 0.05) continue;
    const double bays = BaysOn(len, s.BayM);
    site.Quad(Wall(s, p, s.EavesM, 0.0), Wall(s, q, s.EavesM, bays),
              Wall(s, q, s.EavesM + hq, bays), Wall(s, p, s.EavesM + hp, 0.0));
  }
}

void Covering(const BuildingShape &s, const RoofSurface &roof, const std::vector<Plan2> &plan,
              Site &site) {
  std::vector<Plan2> tris;
  roof.Cover(plan, tris);
  const Facade kind = s.Roof == RoofKind::Flat ? Facade::RoofFlat : Facade::RoofPitch;
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    Vtx v[3];
    for (int k = 0; k < 3; k++)
      v[k] = Face(tris[i + (size_t)k], s.EavesM + roof.HeightAt(tris[i + (size_t)k]) + kSlabM, kind);
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
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    const double zi = s.EavesM + roof.HeightAt(wide[i]), zj = s.EavesM + roof.HeightAt(wide[j]);
    const double ri = s.EavesM + roof.HeightAt(s.Ring[i]), rj = s.EavesM + roof.HeightAt(s.Ring[j]);
    site.Quad(Face(s.Ring[i], ri, Facade::Soffit), Face(s.Ring[j], rj, Facade::Soffit),
              Face(wide[j], zj, Facade::Soffit), Face(wide[i], zi, Facade::Soffit));
    site.Quad(Face(wide[i], zi, Facade::Trim), Face(wide[j], zj, Facade::Trim),
              Face(wide[j], zj + kSlabM, Facade::Trim), Face(wide[i], zi + kSlabM, Facade::Trim));
  }
}

/* A CORNICE AND THE PARAPET OVER IT. The cornice is what a flat roof has instead of an eaves
 * overhang: a course standing 0.16 m proud of the wall, which puts the same hard cast shadow line
 * along the top of the facade that a verge puts under a pitched roof. The parapet then stands back
 * on it, so its own foot is in that shadow. */
void Crown(const BuildingShape &s, const std::vector<Plan2> &inner, const std::vector<Plan2> &out,
           Site &site) {
  const size_t n = s.Ring.size();
  const double band = s.EavesM - 0.34, lo = s.EavesM, hi = s.EavesM + kParapetM;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    site.Quad(Face(out[i], band, Facade::Ledge), Face(out[j], band, Facade::Ledge),
              Face(out[j], lo, Facade::Ledge), Face(out[i], lo, Facade::Ledge));
    site.Quad(Face(s.Ring[i], band, Facade::Soffit), Face(out[i], band, Facade::Soffit),
              Face(out[j], band, Facade::Soffit), Face(s.Ring[j], band, Facade::Soffit));
    site.Quad(Face(out[i], lo, Facade::Ledge), Face(out[j], lo, Facade::Ledge),
              Face(s.Ring[j], lo, Facade::Ledge), Face(s.Ring[i], lo, Facade::Ledge));

    site.Quad(Face(s.Ring[i], lo, Facade::Parapet), Face(s.Ring[j], lo, Facade::Parapet),
              Face(s.Ring[j], hi, Facade::Parapet), Face(s.Ring[i], hi, Facade::Parapet));
    site.Quad(Face(inner[j], lo, Facade::Parapet), Face(inner[i], lo, Facade::Parapet),
              Face(inner[i], hi, Facade::Parapet), Face(inner[j], hi, Facade::Parapet));
    site.Quad(Face(inner[i], hi, Facade::Ledge), Face(inner[j], hi, Facade::Ledge),
              Face(s.Ring[j], hi, Facade::Ledge), Face(s.Ring[i], hi, Facade::Ledge));
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
    site.Quad(Face(c[i], lowZ, side), Face(c[j], lowZ, side), Face(c[j], highZ, side),
              Face(c[i], highZ, side));
  }
  site.Quad(Face(c[0], highZ, Facade::Ledge), Face(c[1], highZ, Facade::Ledge),
            Face(c[2], highZ, Facade::Ledge), Face(c[3], highZ, Facade::Ledge));
}

bool WantsChimney(const BuildingShape &s) {
  if (s.Roof != RoofKind::Gable && s.Roof != RoofKind::Hip && s.Roof != RoofKind::Mansard)
    return false;
  return s.Use == BuildingUse::House || s.Use == BuildingUse::Terrace ||
         s.Use == BuildingUse::Block;
}

void Chimney(const BuildingShape &s, const RoofSurface &roof, Site &site) {
  const double along = (((double)(s.Seed >> 9 & 0xffu) / 255.0) - 0.5) * 1.30 * s.HalfUm;
  const Plan2 foot = s.FromBox(along, 0.0);
  const double stack = s.EavesM + roof.HeightAt(foot) + kChimneyOverRidgeM;
  Box(site, s, foot, 0.5 * kChimneyWideM, 0.4 * kChimneyWideM, s.EavesM, stack, Facade::Trim);
}

/* What stands on a flat roof, because a flat roof is never empty: the lift overrun and the plant
 * that goes with it are what break a block's silhouette against the sky. */
void RoofPlant(const BuildingShape &s, Site &site) {
  const double halfU = std::min(2.6, 0.30 * s.HalfUm), halfV = std::min(1.9, 0.30 * s.HalfVm);
  if (halfU < 0.9 || halfV < 0.7) return;
  const double along = (((double)(s.Seed >> 13 & 0xffu) / 255.0) - 0.5) * 0.9 * s.HalfUm;
  const Plan2 foot = s.FromBox(along, 0.0);
  Box(site, s, foot, halfU, halfV, s.EavesM, s.EavesM + 2.1, Facade::Metal);
}

}  // namespace

void BuildingMesh::Mesh(const StructurePlan &plan, std::vector<float> &soup) const noexcept {
  if (plan.RingLatLon.Size() < 6 || !plan.AnchorEcef) return;
  const BuildingShape shape = Analyse(plan.RingLatLon, plan.HeightM, plan.HeightMeasured);
  if (!shape.Valid()) return;

  Site site(plan, soup);
  const RoofSurface roof(shape);
  Walls(shape, site);

  if (shape.Roof == RoofKind::Flat) {
    const std::vector<Plan2> inner = RoofSurface::Widened(shape.Ring, -kParapetThickM);
    const std::vector<Plan2> out = RoofSurface::Widened(shape.Ring, kCorniceM);
    const bool crowned = !inner.empty() && !out.empty() && shape.HalfVm > 2.2;
    Covering(shape, roof, crowned ? inner : shape.Ring, site);
    if (crowned) Crown(shape, inner, out, site);
    RoofPlant(shape, site);
    return;
  }

  const std::vector<Plan2> wide = RoofSurface::Widened(shape.Ring, shape.OverhangM);
  Covering(shape, roof, wide.empty() ? shape.Ring : wide, site);
  Gables(shape, roof, site);
  if (!wide.empty()) Eaves(shape, roof, wide, site);
  if (WantsChimney(shape)) Chimney(shape, roof, site);
}

}  // namespace outshine::Generators
