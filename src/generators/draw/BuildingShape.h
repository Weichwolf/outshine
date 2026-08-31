#ifndef OUTSHINE_GENERATORS_DRAW_BUILDINGSHAPE_H
#define OUTSHINE_GENERATORS_DRAW_BUILDINGSHAPE_H

#include <cstdint>
#include <vector>

#include "Span.h"
#include "StructureMesher.h"

namespace outshine::Generators {

struct En {
  double E = 0.0, N = 0.0;
};

enum class RoofKind : uint8_t { Flat, Gable, Hip, Shed, Mansard, Sawtooth, Dome };

enum class BuildingUse : uint8_t { Outbuilding, House, Terrace, Block, Hall, Tower, Spire };

struct BuildingShape {
  std::vector<En> Ring;
  size_t TidiedAway = 0;

  std::vector<uint8_t> Party;
  double AreaM2 = 0.0;
  En Centre;
  En AxisU;
  double HalfUm = 0.0, HalfVm = 0.0;
  double Fill = 0.0;

  BuildingUse Use = BuildingUse::House;
  RoofKind Roof = RoofKind::Flat;
  int Storeys = 1;
  double FloorM = 2.9;
  double FootM = 0.0;
  double SeatM = 0.0;
  double SoleM = 0.0;
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

  [[nodiscard]] double TopM() const { return SeatM + FootM + EavesM + RiseM; }

  [[nodiscard]] En AxisV() const { return {-AxisU.N, AxisU.E}; }

  void ToBox(const En &p, double *u, double *v) const;
  [[nodiscard]] En FromBox(double u, double v) const;
};

struct Massing {
  std::vector<En> Outline;
  std::vector<BuildingShape> Parts;
};

Massing
MassOf(Span<const double> ringLatLon, double heightM, bool heightMeasured, const Frontage &street);

} // namespace outshine::Generators
#endif
