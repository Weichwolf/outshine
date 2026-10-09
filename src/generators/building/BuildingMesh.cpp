#include "math/Units.h"
#include "BuildingMaterials.h"
#include "Digest.h"
#include <generation/Generate.h>

#include "BuildingMesh.h"
#include "BuildingWallNormals.h"
#include "math/Box.h"

#include "ground/TileMeshes.h"
#include "math/Vec3.h"

#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <expected>
#include <cstdint>
#include <map>
#include <memory>
#include <limits>
#include <optional>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include "BuildingScratch.h"
#include "BuildingShape.h"
#include "BuildingPreparation.h"
#include "BuildingSurface.h"
#include "Geodesy.h"
#include "RoofSurface.h"

namespace outshine::Generators {

namespace {

#include "FacadeOpeningValues.h"

constexpr double kSameHeightM = 1.0e-3;
constexpr double kLeastEdgeM = 0.05;
constexpr double kLeastRiseM = 0.03;

}

namespace {

bool ValidFootprintCoordinates(std::span<const double> ring) {
  constexpr double maxLatitudeDeg = 90.0;
  constexpr double maxLongitudeDeg = 180.0;
  if (ring.size() < 6 || ring.size() % 2 != 0) { return false; }
  for (size_t at = 0; at < ring.size(); at += 2) {
    if (!(std::abs(ring[at]) <= maxLatitudeDeg) || !(std::abs(ring[at + 1]) <= maxLongitudeDeg)) {
      return false;
    }
  }
  return true;
}

bool ValidPlanParameters(const StructurePlan &plan) {
  if (!plan.CornerAslM.empty() && plan.CornerAslM.size() != plan.RingLatLon.size() / 2) {
    return false;
  }
  const std::array values{plan.BaseAslM,
                          plan.SeatAslM,
                          plan.FootAslM,
                          plan.HeightM,
                          plan.MinimumHeightM,
                          plan.AnchorEcef[0],
                          plan.AnchorEcef[1],
                          plan.AnchorEcef[2],
                          plan.PitchedShare,
                          plan.Street.KerbEm,
                          plan.Street.KerbNm,
                          plan.Street.AlongE,
                          plan.Street.AlongN,
                          plan.Street.ToStreetE,
                          plan.Street.ToStreetN};
  const auto finite = [](double value) { return std::isfinite(value); };
  const bool colourValid = !plan.WallColour || std::ranges::all_of(*plan.WallColour, [](float c) {
    return std::isfinite(c) && c >= 0.0f && c <= 1.0f;
  });
  const bool facadeValid = !plan.Facade || (*plan.Facade >= FacadeStyle::Outbuilding &&
                                            *plan.Facade <= FacadeStyle::Glazing);
  return colourValid && facadeValid && std::ranges::all_of(values, finite) &&
         std::ranges::all_of(plan.RingLatLon, finite) &&
         std::ranges::all_of(plan.CornerAslM, finite);
}

std::expected<std::span<const BuildingShape>, StructureMeshError>
ShapesFor(const StructurePlan &plan, BuildingScratch &scratch) {
  if (plan.Prepared != nullptr) {
    const auto ready = plan.Prepared->RequireShapes();
    if (!ready) { return std::unexpected(ready.error()); }
    return plan.Prepared->Shapes();
  }
  const auto shapes = PrepareBuildingShapes(plan, scratch);
  if (!shapes) { return std::unexpected(shapes.error()); }
  return std::span<const BuildingShape>(*shapes);
}

template <typename T> void TrimAppend(std::vector<T> &output, size_t previousSize) noexcept {
  while (output.size() > previousSize) { output.pop_back(); }
}

void AppendWallColours(const StructurePlan &plan, size_t first, Raised &into) {
  if (!plan.WallColour && into.WallColours.empty()) { return; }
  if (into.WallColours.empty()) { into.WallColours.resize(first * 4, 1.0f); }
  for (size_t at = first; at < into.WallCorners.size(); ++at) {
    const float u = into.WallCorners[at].uv()[0];
    const bool wall = (u >= 0.0f && !IsGlazingUv(u)) || std::fmod(-u - 1.0f, 16.0f) == 0.0f;
    for (size_t channel = 0; channel < 3; ++channel) {
      into.WallColours.push_back(plan.WallColour && wall
                                     ? (*plan.WallColour)[channel] / kBuildingWallColour[channel]
                                     : 1.0f);
    }
    into.WallColours.push_back(1.0f);
  }
}

constexpr double kSlabM = 0.20;

struct Vtx {
  EastNorth P;
  double Z = 0.0;
  float U = 0.0f, V = 0.0f;
};

double EavesZ(const BuildingShape &s) {
  return s.SeatM + s.FootM + s.EavesM;
}

Vtx Wall(const BuildingShape &s, const EastNorth &p, double z, double bays, Fields stand) {
  return {.P = p,
          .Z = z,
          .U = FacadeUvX(s.OpeningStyle, stand, s.WallVariant, static_cast<float>(bays)),
          .V = FacadeUvY(static_cast<float>((z - s.SeatM - s.FootM) / s.FloorM))};
}

Vtx Face(const BuildingShape &s, const EastNorth &p, double z, Facade kind) {
  return {.P = p, .Z = z, .U = FaceUvX(kind, s.Ident), .V = static_cast<float>(z)};
}

class Site {
public:
  Site(const StructurePlan &plan, BuildingScratch &scratch, Raised &into)
      : Out_(into), Scratch_(scratch) {
    Scratch_.ClearWelds();
    Coarseness_ = plan.Coarseness;
    RecessedOpenings_ = plan.RecessedOpenings;
    MinimumHeightM_ = plan.MinimumHeightM;
    if (plan.Prepared != nullptr) {
      Origin_ = plan.Prepared->Origin();
      East_ = plan.Prepared->Axes().East;
      North_ = plan.Prepared->Axes().North;
      Up_ = plan.Prepared->Axes().Up;
      return;
    }
    const double lat = plan.RingLatLon[0];
    const double lon = plan.RingLatLon[1];
    Vec3 origin;
    GeoToEcef({.LongitudeDeg = lon, .LatitudeDeg = lat, .HeightM = plan.BaseAslM}, origin);
    const EnuAxes axes = EnuAxesEcef({.LongitudeDeg = lon, .LatitudeDeg = lat});
    East_ = axes.East;
    North_ = axes.North;
    Up_ = axes.Up;
    for (int c = 0; c < 3; c++) { Origin_[c] = origin[c] - plan.AnchorEcef[c]; }
    Coarseness_ = plan.Coarseness;
    RecessedOpenings_ = plan.RecessedOpenings;
    MinimumHeightM_ = plan.MinimumHeightM;
  }

  Site(const Vec3 &origin,
       const EnuAxes &axes,
       double minimumHeightM,
       BuildingScratch &scratch,
       Raised &into)
      : Out_(into),
        Scratch_(scratch),
        Origin_(origin),
        East_(axes.East),
        North_(axes.North),
        Up_(axes.Up),
        Coarseness_(LevelOfDetail::Shell),
        RecessedOpenings_(false),
        MinimumHeightM_(minimumHeightM) {
    Scratch_.ClearWelds();
  }

  [[nodiscard]] LevelOfDetail Coarseness() const { return Coarseness_; }

  [[nodiscard]] Vec3 PositionOf(const Vec3 &local) const noexcept {
    return Origin_ + East_ * local[0] + Up_ * local[1] + North_ * local[2];
  }

  [[nodiscard]] bool RecessedOpenings() const { return RecessedOpenings_; }

  [[nodiscard]] double LowerZ(const BuildingShape &shape) const {
    return BuildingBottomM(shape, MinimumHeightM_);
  }

  [[nodiscard]] BuildingScratch &Scratch() { return Scratch_; }

  void UseShape(const BuildingShape &shape) noexcept {
    CurvedShape_ = HasCurvedShaftWalls(shape) ? &shape : nullptr;
  }

  [[nodiscard]] double CoordinateMagnitudeM(const BuildingShape &shape) const noexcept {
    double acrossM = 0.0;
    const auto include = [&acrossM](std::span<const EastNorth> ring) {
      for (const auto &point : ring) {
        acrossM = std::max(acrossM, std::hypot(point.EastM, point.NorthM));
      }
    };
    include(shape.Ring);
    for (const auto &hole : shape.Holes) { include(hole); }
    const double heightM = std::max(std::abs(LowerZ(shape)), std::abs(shape.TopM()));
    return std::max({std::abs(Origin_[0]), std::abs(Origin_[1]), std::abs(Origin_[2])}) + acrossM +
           heightM + kOpeningDepthM + kSlabM;
  }

  [[nodiscard]] static Vtx Snapped(const Vtx &v) {
    Vtx out = v;
    out.P.EastM = std::round(v.P.EastM * kBuildingWeldPerM) / kBuildingWeldPerM;
    out.P.NorthM = std::round(v.P.NorthM * kBuildingWeldPerM) / kBuildingWeldPerM;
    out.Z = std::round(v.Z * kBuildingWeldPerM) / kBuildingWeldPerM;
    return out;
  }

  [[nodiscard]] std::expected<void, StructureMeshError> Status() const noexcept { return Status_; }

  [[nodiscard]] static bool Representable(const Vtx &v) noexcept {
    constexpr double exclusiveLimit = 0x1p63;
    const std::array coordinates{v.P.EastM, v.P.NorthM, v.Z};
    return std::ranges::all_of(coordinates, [](double coordinate) {
      const double millimetres = coordinate * kBuildingWeldPerM;
      return millimetres >= -exclusiveLimit && millimetres < exclusiveLimit;
    });
  }

  [[nodiscard]] uint32_t Index(const Vtx &v) {
    if (!Status_) { return 0; }
    const auto ce = static_cast<int64_t>(std::llround(v.P.EastM * kBuildingWeldPerM));
    const auto cn = static_cast<int64_t>(std::llround(v.P.NorthM * kBuildingWeldPerM));
    const auto cz = static_cast<int64_t>(std::llround(v.Z * kBuildingWeldPerM));
    const BuildingPositionKey key{.EastMm = ce, .NorthMm = cn, .HeightMm = cz};
    const auto made = Scratch_.Welded.Emplace(key, static_cast<uint32_t>(Scratch_.Welded.Size()));
    if (!made) {
      Status_ = std::unexpected(StructureMeshError::BuildFailed);
      return 0;
    }
    return *made->first;
  }

  void Tri(const Vtx &given0, const Vtx &given1, const Vtx &given2) {
    if (!Status_) { return; }
    const Vtx a = Snapped(given0);
    const Vtx b = Snapped(given1);
    const Vtx c = Snapped(given2);
    if (!Representable(a) || !Representable(b) || !Representable(c)) {
      Status_ = std::unexpected(StructureMeshError::InvalidPlan);
      return;
    }
    const uint32_t ia = Index(a);
    const uint32_t ib = Index(b);
    const uint32_t ic = Index(c);
    if (!Status_ || ia == ib || ib == ic || ic == ia) { return; }
    const double e1 = b.P.EastM - a.P.EastM;
    const double n1 = b.P.NorthM - a.P.NorthM;
    const double z1 = b.Z - a.Z;
    const double e2 = c.P.EastM - a.P.EastM;
    const double n2 = c.P.NorthM - a.P.NorthM;
    const double z2 = c.Z - a.Z;
    Vec3 nrm = {{n1 * z2 - z1 * n2, z1 * e2 - e1 * z2, e1 * n2 - n1 * e2}};
    const double len = std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (len < kParallelCross) { return; }
    for (double &c2 : nrm) { c2 /= len; }
    const bool trim = a.U < 0.0f && std::fmod(-a.U - 1.0f, static_cast<float>(kFacadeStride)) ==
                                        static_cast<float>(Facade::Trim);
    const int side = nrm[2] > kSteepestRoof && !trim ? 1 : 0;
    std::vector<uint32_t> &run = side == 1 ? Out_.RoofRun : Out_.WallRun;
    const std::array triangle{
        Corner(side, a, ia, nrm), Corner(side, b, ib, nrm), Corner(side, c, ic, nrm)};
    if (!Status_) { return; }
    run.insert(run.end(), triangle.begin(), triangle.end());
  }

  void Quad(const Vtx &a, const Vtx &b, const Vtx &c, const Vtx &d) {
    Tri(a, b, c);
    Tri(a, c, d);
  }

private:
  [[nodiscard]] uint32_t Corner(int side, const Vtx &v, uint32_t at, const Vec3 &nrm) {
    if (!Status_) { return 0; }
    if (!std::isfinite(v.U) || !std::isfinite(v.V)) {
      Status_ = std::unexpected(StructureMeshError::InvalidPlan);
      return 0;
    }
    std::vector<StoredVertex> &soup = side == 1 ? Out_.RoofCorners : Out_.WallCorners;
    Vec3f placeM{};
    Vec3f turned{};
    const Vec3 normal =
        CurvedShape_ != nullptr ? BuildingWallShadingNormal(*CurvedShape_, v.P, nrm) : nrm;
    for (int c = 0; c < 3; c++) {
      placeM[static_cast<size_t>(c)] = static_cast<float>(Origin_[c] + v.P.EastM * East_[c] +
                                                          v.P.NorthM * North_[c] + v.Z * Up_[c]);
      turned[static_cast<size_t>(c)] =
          static_cast<float>(normal[0] * East_[c] + normal[1] * North_[c] + normal[2] * Up_[c]);
    }
    const StoredVertex vertex = StoredVertex::Of(placeM, Vec2f{{v.U, v.V}}, turned);
    const BuildingCornerKey key{.Position = at,
                                .Normal = vertex.normWord,
                                .TextureU = std::bit_cast<uint32_t>(vertex.texture[0]),
                                .TextureV = std::bit_cast<uint32_t>(vertex.texture[1])};
    auto &corners = Scratch_.Corners[static_cast<size_t>(side)];
    const auto made = corners.Emplace(key, static_cast<uint32_t>(soup.size()));
    if (!made) {
      Status_ = std::unexpected(StructureMeshError::BuildFailed);
      return 0;
    }
    if (made->second) { soup.push_back(vertex); }
    return *made->first;
  }

  std::expected<void, StructureMeshError> Status_;
  Raised &Out_;
  BuildingScratch &Scratch_;
  Vec3 Origin_, East_, North_, Up_;
  LevelOfDetail Coarseness_ = LevelOfDetail::Fine;
  bool RecessedOpenings_ = true;
  double MinimumHeightM_ = 0.0;
  const BuildingShape *CurvedShape_ = nullptr;
};

double EdgeLength(const EastNorth &p, const EastNorth &q) {
  return std::hypot(q.EastM - p.EastM, q.NorthM - p.NorthM);
}

EastNorth Along(const EastNorth &p, const EastNorth &q, double t) {
  return {.EastM = p.EastM + (q.EastM - p.EastM) * t,
          .NorthM = p.NorthM + (q.NorthM - p.NorthM) * t};
}

void WallPanel(const BuildingShape &s,
               const EastNorth &p,
               const EastNorth &q,
               double bay0,
               double bay1,
               double lowZ,
               double highZ,
               Fields stand,
               Site &site) {
  site.Quad(Wall(s, p, lowZ, bay0, stand),
            Wall(s, q, lowZ, bay1, stand),
            Wall(s, q, highZ, bay1, stand),
            Wall(s, p, highZ, bay0, stand));
}

struct OpeningCoordinates {
  double Bay = 0.0;
  Fields Standing = Fields::Back;
};

void Opening(const BuildingShape &s,
             const EastNorth &p,
             const EastNorth &q,
             double lowZ,
             double highZ,
             OpeningCoordinates coordinates,
             Site &site) {
  const double length = EdgeLength(p, q);
  const EastNorth inset{.EastM = -(q.NorthM - p.NorthM) * kOpeningDepthM / length,
                        .NorthM = (q.EastM - p.EastM) * kOpeningDepthM / length};
  const EastNorth a{.EastM = p.EastM + inset.EastM, .NorthM = p.NorthM + inset.NorthM};
  const EastNorth b{.EastM = q.EastM + inset.EastM, .NorthM = q.NorthM + inset.NorthM};
  const auto glass = [&s, coordinates](const EastNorth &point, double z, float offset) {
    auto vertex = Wall(s, point, z, coordinates.Bay + offset, coordinates.Standing);
    vertex.U = FacadeUvX(FacadeStyle::Glazing,
                         coordinates.Standing,
                         s.WallVariant,
                         static_cast<float>(coordinates.Bay) + offset);
    return vertex;
  };
  site.Quad(glass(a, lowZ, kOpeningLowU),
            glass(b, lowZ, kOpeningHighU),
            glass(b, highZ, kOpeningHighU),
            glass(a, highZ, kOpeningLowU));
  site.Quad(Face(s, p, lowZ, Facade::Trim),
            Face(s, q, lowZ, Facade::Trim),
            Face(s, b, lowZ, Facade::Trim),
            Face(s, a, lowZ, Facade::Trim));
  site.Quad(Face(s, a, highZ, Facade::Trim),
            Face(s, b, highZ, Facade::Trim),
            Face(s, q, highZ, Facade::Trim),
            Face(s, p, highZ, Facade::Trim));
  site.Quad(Face(s, p, highZ, Facade::Trim),
            Face(s, p, lowZ, Facade::Trim),
            Face(s, a, lowZ, Facade::Trim),
            Face(s, a, highZ, Facade::Trim));
  site.Quad(Face(s, b, highZ, Facade::Trim),
            Face(s, b, lowZ, Facade::Trim),
            Face(s, q, lowZ, Facade::Trim),
            Face(s, q, highZ, Facade::Trim));
}

struct FacadeWall {
  EastNorth From;
  EastNorth To;
  double BottomM = 0.0;
  double TopM = 0.0;
  int Bays = 0;
  Fields Standing = Fields::Back;
};

bool RecessedWall(const BuildingShape &s, const FacadeWall &wall, Site &site) {
  const bool housing = s.OpeningStyle == FacadeStyle::House ||
                       s.OpeningStyle == FacadeStyle::Terrace ||
                       s.OpeningStyle == FacadeStyle::Block;
  const auto &p = wall.From;
  const auto &q = wall.To;
  const double bays = wall.Bays;
  const Fields stand = wall.Standing;
  if (site.Coarseness() != LevelOfDetail::Fine || !site.RecessedOpenings() || !housing ||
      bays < 1.0) {
    return false;
  }
  double below = wall.BottomM;
  for (int storey = 0; storey < s.Storeys; ++storey) {
    const double base = s.SeatM + s.FootM + storey * s.FloorM;
    const double bottom = base + kOpeningLowV * s.FloorM;
    const double top = base + kOpeningHighV * s.FloorM;
    WallPanel(s, p, q, 0.0, bays, below, bottom, stand, site);
    for (int bay = 0; bay < wall.Bays; ++bay) {
      const double axis = bay;
      const auto left = Along(p, q, (axis + kOpeningLowU) / bays);
      const auto right = Along(p, q, (axis + kOpeningHighU) / bays);
      WallPanel(s,
                Along(p, q, static_cast<double>(bay) / bays),
                left,
                bay,
                axis + kOpeningLowU,
                bottom,
                top,
                stand,
                site);
      WallPanel(s,
                right,
                Along(p, q, (bay + 1.0) / bays),
                axis + kOpeningHighU,
                bay + 1.0,
                bottom,
                top,
                stand,
                site);
      Opening(s, left, right, bottom, top, {.Bay = axis, .Standing = stand}, site);
    }
    below = top;
  }
  WallPanel(s, p, q, 0.0, bays, below, wall.TopM, stand, site);
  return true;
}

struct Stretch {
  EastNorth From;
  EastNorth To;
};

struct Breaking {
  Stretch Face;
  Stretch Eave;
  bool Overhung = false;
};

void BreaksBoth(const RoofSurface &roof,
                Breaking along,
                BuildingScratch &scratch,
                std::vector<double> &at) {
  const Stretch &face = along.Face;
  const Stretch &eave = along.Eave;
  const bool overhung = along.Overhung;
  std::vector<double> &other = scratch.Other;
  roof.BreaksAlong(face.From, face.To, at);
  if (overhung) {
    roof.BreaksAlong(eave.From, eave.To, other);
    at.insert(at.end(), other.begin(), other.end());
    std::ranges::sort(at);
    at.erase(std::ranges::unique(at,

                                 [](double a, double b) { return std::fabs(a - b) < kSameHeightM; })
                 .begin(),
             at.end());
  }
}

void Refined(std::span<const EastNorth> ring,
             std::span<const EastNorth> wide,
             const RoofSurface &roof,
             bool takeWide,
             BuildingScratch &scratch,
             std::vector<EastNorth> &out) {
  const size_t n = ring.size();
  const bool overhung = wide.size() == n;
  out.clear();
  std::vector<double> &at = scratch.At;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    BreaksBoth(roof,
               {.Face = {.From = ring[i], .To = ring[j]},
                .Eave = {.From = overhung ? wide[i] : ring[i], .To = overhung ? wide[j] : ring[j]},
                .Overhung = overhung},
               scratch,
               at);
    const EastNorth &from = takeWide && overhung ? wide[i] : ring[i];
    const EastNorth &to = takeWide && overhung ? wide[j] : ring[j];
    out.push_back(from);
    for (const double t : at) { out.push_back(Along(from, to, t)); }
  }
}

void Walls(const BuildingShape &s,
           const RoofSurface &roof,
           std::span<const EastNorth> wide,
           double lowZ,
           double topZ,
           Site &site,
           std::span<const EastNorth> boundary = {}) {
  const bool exterior = boundary.empty();
  const std::span<const EastNorth> ring = exterior ? std::span(s.Ring) : boundary;
  const size_t n = ring.size();
  std::vector<double> &breaks = site.Scratch().Breaks;
  for (size_t i = 0; i < n; i++) {
    const EastNorth &p = ring[i];
    const EastNorth &q = ring[(i + 1) % n];
    const double len = EdgeLength(p, q);
    if (len < kLeastEdgeM) { continue; }
    const double bays = (exterior && s.PartyWallEdges[i] != 0u) ? 0.0 : FacadeBays(len, s.BayM);
    const Fields stand =
        exterior && std::cmp_equal(i, s.FrontEdge) ? Fields::Entrance : Fields::Back;
    if (RecessedWall(s,
                     {.From = p,
                      .To = q,
                      .BottomM = lowZ,
                      .TopM = topZ,
                      .Bays = static_cast<int>(bays),
                      .Standing = stand},
                     site)) {
      continue;
    }
    const bool overhung = wide.size() == n;
    BreaksBoth(roof,
               {.Face = {.From = p, .To = q},
                .Eave = {.From = overhung ? wide[i] : p, .To = overhung ? wide[(i + 1) % n] : q},
                .Overhung = overhung},
               site.Scratch(),
               breaks);
    double was = 0.0;
    for (size_t step = 0; step <= breaks.size(); ++step) {
      const double now = step < breaks.size() ? breaks[step] : 1.0;
      WallPanel(
          s, Along(p, q, was), Along(p, q, now), bays * was, bays * now, lowZ, topZ, stand, site);
      was = now;
    }
  }
}

void Floor(const BuildingShape &s, std::span<const EastNorth> ring, double atZ, Site &site) {
  std::vector<EastNorth> &tris = site.Scratch().Tris;
  tris.clear();
  (void)RoofSurface::Fill(ring, site.Scratch(), tris, s.Holes);
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    site.Tri(Face(s, tris[i + 2], atZ, Facade::Plinth),
             Face(s, tris[i + 1], atZ, Facade::Plinth),
             Face(s, tris[i], atZ, Facade::Plinth));
  }
}

void Gables(const BuildingShape &s,
            const RoofSurface &roof,
            std::span<const EastNorth> wide,
            Site &site,
            std::span<const EastNorth> boundary = {}) {
  const std::span<const EastNorth> ring = boundary.empty() ? std::span(s.Ring) : boundary;
  const size_t n = ring.size();
  const double eaves = EavesZ(s);
  std::vector<double> &breaks = site.Scratch().Breaks;
  for (size_t i = 0; i < n; i++) {
    const EastNorth &p = ring[i];
    const EastNorth &q = ring[(i + 1) % n];
    const double len = EdgeLength(p, q);
    if (len < kLeastEdgeM) { continue; }
    const bool overhung = wide.size() == n;
    BreaksBoth(roof,
               {.Face = {.From = p, .To = q},
                .Eave = {.From = overhung ? wide[i] : p, .To = overhung ? wide[(i + 1) % n] : q},
                .Overhung = overhung},
               site.Scratch(),
               breaks);
    double was = 0.0;
    for (size_t step = 0; step <= breaks.size(); ++step) {
      const double now = step < breaks.size() ? breaks[step] : 1.0;
      const EastNorth a = Along(p, q, was);
      const EastNorth b = Along(p, q, now);
      const double ha = std::max(roof.HeightAt(a), 0.0);
      const double hb = std::max(roof.HeightAt(b), 0.0);
      was = now;
      if (ha < kLeastRiseM && hb < kLeastRiseM) { continue; }
      site.Quad(Wall(s, a, eaves, 0.0, Fields::Back),
                Wall(s, b, eaves, 0.0, Fields::Back),
                Wall(s, b, eaves + hb, 0.0, Fields::Back),
                Wall(s, a, eaves + ha, 0.0, Fields::Back));
    }
  }
}

void Covering(const BuildingShape &s,
              const RoofSurface &roof,
              std::span<const EastNorth> plan,
              double deckZ,
              Site &site) {
  std::vector<EastNorth> &tris = site.Scratch().Tris;
  tris.clear();
  roof.Cover(plan, site.Scratch(), tris);
  const Facade kind = s.Roof == RoofKind::Flat ? Facade::RoofFlat : Facade::RoofPitch;
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    std::array<Vtx, 3> v{};
    for (int k = 0; k < 3; k++) {
      v[k] = Face(s,
                  tris[i + static_cast<size_t>(k)],
                  deckZ + roof.HeightAt(tris[i + static_cast<size_t>(k)]) + kSlabM,
                  kind);
    }
    site.Tri(v[0], v[1], v[2]);
  }
}

[[nodiscard]] std::array<EastNorth, 4> Hull(std::span<const EastNorth> ring) {
  const size_t n = ring.size();
  double bestArea = kBeyondAnyCoordinate;
  double axE = 1.0;
  double axN = 0.0;
  double minU = 0.0;
  double maxU = 0.0;
  double minV = 0.0;
  double maxV = 0.0;
  for (size_t i = 0; i < n; i++) {
    const EastNorth &a = ring[i];
    const EastNorth &b = ring[(i + 1) % n];
    const double dE = b.EastM - a.EastM;
    const double dN = b.NorthM - a.NorthM;
    const double len = std::hypot(dE, dN);
    if (len < kLeastRunM) { continue; }
    const double uE = dE / len;
    const double uN = dN / len;
    double loU = kBeyondAnyCoordinate;
    double hiU = -kBeyondAnyCoordinate;
    double loV = kBeyondAnyCoordinate;
    double hiV = -kBeyondAnyCoordinate;
    for (const EastNorth &p : ring) {
      const double u = p.EastM * uE + p.NorthM * uN;
      const double v = -p.EastM * uN + p.NorthM * uE;
      loU = std::min(loU, u);
      hiU = std::max(hiU, u);
      loV = std::min(loV, v);
      hiV = std::max(hiV, v);
    }
    const double area = (hiU - loU) * (hiV - loV);
    if (area < bestArea) {
      bestArea = area;
      axE = uE;
      axN = uN;
      minU = loU;
      maxU = hiU;
      minV = loV;
      maxV = hiV;
    }
  }
  const auto at = [&](double u, double v) {
    return EastNorth{.EastM = u * axE - v * axN, .NorthM = u * axN + v * axE};
  };
  return {at(minU, minV), at(maxU, minV), at(maxU, maxV), at(minU, maxV)};
}

void RaiseMassedEnvelope(const BuildingShape &s, std::span<const EastNorth> ring, Site &site) {
  const double lowZ = s.SoleM;
  const double topZ = s.TopM();
  const Facade roof = s.Roof == RoofKind::Flat ? Facade::RoofFlat : Facade::RoofPitch;
  for (size_t i = 0; i < 4; i++) {
    const size_t j = (i + 1) % 4;
    const double bays = FacadeBays(EdgeLength(ring[i], ring[j]), s.BayM);
    site.Quad(Wall(s, ring[i], lowZ, 0.0, Fields::Back),
              Wall(s, ring[j], lowZ, bays, Fields::Back),
              Wall(s, ring[j], topZ, bays, Fields::Back),
              Wall(s, ring[i], topZ, 0.0, Fields::Back));
  }
  site.Quad(Face(s, ring[0], topZ, roof),
            Face(s, ring[1], topZ, roof),
            Face(s, ring[2], topZ, roof),
            Face(s, ring[3], topZ, roof));
  Floor(s, ring, lowZ, site);
}

void RaiseShell(const BuildingShape &s, Site &site) {
  const RoofSurface roof(s);
  BuildingScratch &scratch = site.Scratch();
  const double lowZ = site.LowerZ(s);
  const double topZ = EavesZ(s) + (s.Roof == RoofKind::Flat ? s.RiseM : 0.0);
  std::vector<EastNorth> &covered = scratch.Covered;
  Refined(s.Ring, {}, roof, false, scratch, covered);
  const std::span<const EastNorth> ring =
      covered.empty() ? std::span<const EastNorth>(s.Ring) : std::span<const EastNorth>(covered);
  Floor(s, ring, lowZ, site);
  Walls(s, roof, {}, lowZ, topZ, site);
  Covering(s, roof, ring, topZ - kSlabM, site);
  if (s.Roof != RoofKind::Flat) { Gables(s, roof, {}, site); }
}

void RaiseCourtyard(const BuildingShape &s, Site &site) {
  const RoofSurface roof(s);
  const double low = site.LowerZ(s);
  const double top = EavesZ(s) + (s.Roof == RoofKind::Flat ? s.RiseM : 0.0);
  Floor(s, s.Ring, low, site);
  Walls(s, roof, {}, low, top, site);
  for (const auto &hole : s.Holes) { Walls(s, roof, {}, low, top, site, hole); }
  Covering(s, roof, s.Ring, top - kSlabM, site);
  if (s.Roof != RoofKind::Flat) {
    Gables(s, roof, {}, site);
    for (const auto &hole : s.Holes) { Gables(s, roof, {}, site, hole); }
  }
}

void RaisePart(const BuildingShape &s, Site &site) {
  site.UseShape(s);
  if (!s.Holes.empty()) {
    RaiseCourtyard(s, site);
  } else if (site.Coarseness() >= LevelOfDetail::Massed) {
    RaiseMassedEnvelope(s, Hull(s.Ring), site);
  } else {
    RaiseShell(s, site);
  }
}

double ShellErrorAtMagnitude(double magnitudeM) noexcept {
  const double weldM = std::numbers::sqrt3 / kBuildingWeldPerM;
  const double roundingM = 4.0 * std::numbers::sqrt3 * std::numeric_limits<float>::epsilon() *
                           (magnitudeM + kOpeningDepthM + weldM + 1.0);
  constexpr double kErrorUnitsPerM = 4.0;
  return std::ceil((kOpeningDepthM + weldM + roundingM) * kErrorUnitsPerM) / kErrorUnitsPerM;
}

}

std::unique_ptr<MeshScratch> BuildingMesh::Scratch() const {
  return std::make_unique<BuildingScratch>();
}

std::optional<Box> BuildingMesh::SourceEnvelopeBounds(const StructurePlan &plan,
                                                      MeshScratch &lent) const noexcept {
  if (!ValidFootprintCoordinates(plan.RingLatLon) || !ValidPlanParameters(plan)) {
    return std::nullopt;
  }
  auto *scratch = dynamic_cast<BuildingScratch *>(&lent);
  if (scratch == nullptr) { return std::nullopt; }
  if (const auto *selection =
          plan.Prepared != nullptr ? plan.Prepared->PreparedSelection(plan) : nullptr) {
    return selection->Envelope;
  }
  const auto parts = ShapesFor(plan, *scratch);
  if (!parts || parts->empty()) { return std::nullopt; }
  Raised unused;
  const Site site(plan, *scratch, unused);
  Box bounds;
  double magnitudeM = 0.0;
  for (const auto &part : *parts) {
    Box local;
    for (const auto &point : part.Ring) {
      local.Cover(Vec3{{point.EastM, site.LowerZ(part), point.NorthM}});
      local.Cover(Vec3{{point.EastM, part.TopM(), point.NorthM}});
    }
    for (unsigned corner = 0; corner < 8; ++corner) {
      bounds.Cover(site.PositionOf(local.Corner(corner)));
    }
    magnitudeM = std::max(magnitudeM, site.CoordinateMagnitudeM(part));
  }
  const double paddingM = ShellErrorAtMagnitude(magnitudeM);
  for (size_t axis = 0; axis < 3; ++axis) {
    bounds.Min[axis] -= paddingM;
    bounds.Max[axis] += paddingM;
  }
  return bounds;
}

std::optional<double>
BuildingMesh::ShellSurfaceErrorM(std::span<const StoredVertex> walls) const noexcept {
  if (walls.empty()) { return std::nullopt; }
  double magnitudeM = 0.0;
  for (const auto &wall : walls) {
    for (const float coordinate : wall.pos) {
      if (!std::isfinite(coordinate)) { return std::nullopt; }
      magnitudeM = std::max(magnitudeM, std::abs(static_cast<double>(coordinate)));
    }
  }
  return ShellErrorAtMagnitude(magnitudeM);
}

std::optional<double> BuildingMesh::ShellSurfaceErrorM(const StructurePlan &plan,
                                                       MeshScratch &lent) const noexcept {
  if (!ValidFootprintCoordinates(plan.RingLatLon) || !ValidPlanParameters(plan)) {
    return std::nullopt;
  }
  auto *scratch = dynamic_cast<BuildingScratch *>(&lent);
  if (scratch == nullptr) { return std::nullopt; }
  if (const auto *selection =
          plan.Prepared != nullptr ? plan.Prepared->PreparedSelection(plan) : nullptr) {
    return selection->ShellErrorM;
  }
  const auto parts = ShapesFor(plan, *scratch);
  if (!parts || parts->empty()) { return std::nullopt; }
  Raised unused;
  const Site site(plan, *scratch, unused);
  double magnitudeM = 0.0;
  for (const auto &part : *parts) {
    const double extentM = site.CoordinateMagnitudeM(part);
    if (!std::isfinite(extentM)) { return std::nullopt; }
    magnitudeM = std::max(magnitudeM, extentM);
  }
  return ShellErrorAtMagnitude(magnitudeM);
}

std::expected<void, StructureMeshError>
BuildingMesh::Mesh(const StructurePlan &plan, MeshScratch &lent, Raised &into) const noexcept {
  if (!ValidFootprintCoordinates(plan.RingLatLon) || !ValidPlanParameters(plan)) {
    return std::unexpected(StructureMeshError::InvalidPlan);
  }
  auto *buildingScratch = dynamic_cast<BuildingScratch *>(&lent);
  if (buildingScratch == nullptr) {
    return std::unexpected(StructureMeshError::IncompatibleScratch);
  }
  auto &scratch = *buildingScratch;
  const std::array sizes{
      into.WallCorners.size(), into.RoofCorners.size(), into.WallRun.size(), into.RoofRun.size()};
  const auto rollback = [&] noexcept {
    TrimAppend(into.WallCorners, sizes[0]);
    TrimAppend(into.RoofCorners, sizes[1]);
    TrimAppend(into.WallRun, sizes[2]);
    TrimAppend(into.RoofRun, sizes[3]);
  };
  const auto mass = ShapesFor(plan, scratch);
  if (!mass) { return std::unexpected(mass.error()); }
  const std::span<const BuildingShape> parts = *mass;
  if (parts.empty()) { return std::unexpected(StructureMeshError::UnsupportedFootprint); }

  for (const auto &part : parts) {
    if (part.Holes.empty()) { continue; }
    scratch.Tris.clear();
    if (!RoofSurface::Fill(part.Ring, scratch, scratch.Tris, part.Holes)) {
      return std::unexpected(StructureMeshError::BuildFailed);
    }
  }
  Site site(plan, scratch, into);
  for (const BuildingShape &part : parts) {
    RaisePart(part, site);
    if (const auto status = site.Status(); !status) {
      rollback();
      return status;
    }
  }
  AppendWallColours(plan, sizes[0], into);
  return {};
}

namespace {
void GableFace(const BuildingShape &shape,
               const RoofSurface &roof,
               const EastNorth &p,
               const EastNorth &q,
               double deck,
               Site &site) {
  auto &scratch = site.Scratch();
  roof.BreaksAlong(p, q, scratch.Breaks);
  double was = 0.0;
  for (size_t at = 0; at <= scratch.Breaks.size(); ++at) {
    const double now = at < scratch.Breaks.size() ? scratch.Breaks[at] : 1.0;
    const EastNorth a = Along(p, q, was);
    const EastNorth b = Along(p, q, now);
    const double ha = std::max(roof.HeightAt(a), 0.0);
    const double hb = std::max(roof.HeightAt(b), 0.0);
    was = now;
    if (ha < kLeastRiseM && hb < kLeastRiseM) { continue; }
    site.Quad(Wall(shape, a, deck, 0.0, Fields::Back),
              Wall(shape, b, deck, 0.0, Fields::Back),
              Wall(shape, b, deck + hb, 0.0, Fields::Back),
              Wall(shape, a, deck + ha, 0.0, Fields::Back));
  }
}

bool WallFace(const BuildingShape &shape,
              const RoofSurface &roof,
              size_t edge,
              Site &site,
              double bottom,
              double deck) {
  std::span<const EastNorth> ring = shape.Ring;
  bool exterior = true;
  if (edge >= ring.size()) {
    edge -= ring.size();
    exterior = false;
    for (const auto &hole : shape.Holes) {
      if (edge < hole.size()) {
        ring = hole;
        break;
      }
      edge -= hole.size();
    }
  }
  if (edge >= ring.size()) { return false; }
  const auto &p = ring[edge];
  const auto &q = ring[(edge + 1) % ring.size()];
  const double bays =
      exterior && shape.PartyWallEdges[edge] != 0 ? 0.0 : FacadeBays(EdgeLength(p, q), shape.BayM);
  const Fields stand =
      exterior && std::cmp_equal(edge, shape.FrontEdge) ? Fields::Entrance : Fields::Back;
  WallPanel(shape, p, q, 0.0, bays, bottom, deck, stand, site);
  if (shape.Roof != RoofKind::Flat) { GableFace(shape, roof, p, q, deck, site); }
  return true;
}

}

std::expected<void, StructureMeshError> BuildingSurface::MeshVisible(std::span<const Face> faces,
                                                                     BuildingScratch &scratch,
                                                                     Raised &into) const {
  if (faces.empty()) { return {}; }
  const auto ready = RequireShapes();
  if (!ready) { return std::unexpected(ready.error()); }
  const size_t first = into.WallCorners.size();
  Site site(Origin_, Axes_, MinimumHeightM_, scratch, into);
  for (const auto &face : faces) {
    const auto &shape = Resident().Shapes_[face.Part];
    site.UseShape(shape);
    const RoofSurface roof(shape);
    const double bottom = site.LowerZ(shape);
    const double deck = EavesZ(shape) + (shape.Roof == RoofKind::Flat ? shape.RiseM : 0.0);
    if (face.Side == 0) {
      Floor(shape, shape.Ring, bottom, site);
      continue;
    }
    if (face.Side == 1) {
      Covering(shape, roof, shape.Ring, deck - kSlabM, site);
      continue;
    }
    if (!WallFace(shape, roof, face.Side - 2, site, bottom, deck)) {
      return std::unexpected(StructureMeshError::InvalidPlan);
    }
  }
  StructurePlan paint;
  paint.WallColour = WallColour_;
  AppendWallColours(paint, first, into);
  return site.Status();
}
}
