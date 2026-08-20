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

enum class BuildingUse : uint8_t { Outbuilding, House, Terrace, Block, Hall, Tower, Spire };

struct BuildingShape {
  std::vector<Plan2> Ring;

  std::vector<uint8_t> Party;
  double AreaM2 = 0.0;
  Plan2 Centre;
  Plan2 AxisU;
  double HalfUm = 0.0, HalfVm = 0.0;
  double Fill = 0.0;

  BuildingUse Use = BuildingUse::House;
  RoofKind Roof = RoofKind::Flat;
  int Storeys = 1;
  double FloorM = 2.9;
  double FootM = 0.0;
  double EavesM = 0.0;
  double RiseM = 0.0;
  double BreakFracV = 0.0;
  double BreakRiseM = 0.0;
  double PeriodM = 0.0;
  double BayM = 3.0;
  double OverhangM = 0.0;
  uint32_t Seed = 0;
  int Ident = 0;
  int FrontEdge = -1;

  [[nodiscard]] bool Valid() const { return Ring.size() >= 3 && AreaM2 > 1.0; }
  [[nodiscard]] bool OnGround() const { return FootM <= 0.0; }
  double TopM() const { return FootM + EavesM + RiseM; }

  Plan2 AxisV() const { return {-AxisU.N, AxisU.E}; }
  void ToBox(const Plan2 &p, double *u, double *v) const;
  Plan2 FromBox(double u, double v) const;
};

struct Massing {
  std::vector<Plan2> Outline;
  std::vector<BuildingShape> Parts;
};

Massing MassOf(Span<const double> ringLatLon, double heightM, bool heightMeasured,
               const Frontage &street);

}
#endif
