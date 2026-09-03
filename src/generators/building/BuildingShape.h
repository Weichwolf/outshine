#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGSHAPE_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGSHAPE_H

#include <span>
#include "Earth.h"
#include <cstdint>
#include <vector>

#include "StructureMesher.h"

namespace outshine::Generators {

constexpr double kFloorUnsaidM = 2.9;

using En = outshine::EastNorth;

enum class RoofKind : uint8_t { Flat, Gable, Hip, Shed, Mansard, Sawtooth, Dome };

enum class BuildingUse : uint8_t { Outbuilding, House, Terrace, Block, Hall, Tower, Spire };

struct Boxed {
  double U = 0.0;
  double V = 0.0;
};

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
  double FloorM = kFloorUnsaidM;
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

  [[nodiscard]] En AxisV() const { return {.EastM = -AxisU.NorthM, .NorthM = AxisU.EastM}; }

  [[nodiscard]] Boxed ToBox(const En &p) const;
  [[nodiscard]] En FromBox(Boxed at) const;
};

struct Massing {
  std::vector<En> Outline;
  std::vector<BuildingShape> Parts;
};

Massing MassOf(std::span<const double> ringLatLon,
               double heightM,
               bool heightMeasured,
               const Frontage &street);

} // namespace outshine::Generators
#endif
