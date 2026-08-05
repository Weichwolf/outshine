#include "FBAircraftCatalogue.h"

namespace FlightBox {
namespace {

/* ---- THE FRAGILITY LADDER, in J/m^2, TAKEN OVER VERBATIM from modules/f16/FBF16Damage.cpp and
 * modules/mig29/FBMig29Damage.cpp rather than re-chosen — the two file names stay because a verbatim
 * copy whose source is unnamed is drift waiting to happen. It describes what a blast-fragmentation
 * warhead does to a fighter-sized aluminium airframe, and nothing in doc/modules/air/ distinguishes one
 * catalogue skin from another at that resolution. A ladder per row would express a difference nobody
 * measured, which is why it lives here and not in the manifest. ---- */
constexpr double kAvionicsDegrade = 1.2e4, kAvionicsFail = 3.0e4;
constexpr double kEngineDegrade = 5.0e4, kEngineFail = 1.5e5;
constexpr double kFlcsDegrade = 5.0e4, kFlcsFail = 1.5e5;
constexpr double kStructDegrade = 8.0e4, kStructFail = 2.5e5;

/* FOUR ZONES, and the SYSTEM CONTENT of each is shared by every deck row because six of the fourteen
 * existing system ids carry everything a catalogue aircraft has (module.md §Spec 8). What is per-row is
 * only WHERE the zones sit, which is that row's own length. Engine2 is declared in the aft zone of
 * every row and is simply never failed on a single-engine one — the id already exists and a twin that
 * loses one is a different aircraft. */
constexpr FBZoneSystem kNoseSystems[] = {
    {FBSystemId::Radar, kAvionicsDegrade, kAvionicsFail},
    {FBSystemId::AirData, kAvionicsFail, kAvionicsFail},
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};
constexpr FBZoneSystem kForwardSystems[] = {
    {FBSystemId::Gun, kStructFail, kStructFail},
    {FBSystemId::FireControl, kAvionicsFail, kAvionicsFail},
    {FBSystemId::Datalink, kAvionicsFail, kAvionicsFail},
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};
constexpr FBZoneSystem kCenterSystems[] = {
    {FBSystemId::Stores, kAvionicsFail, kAvionicsFail},
    {FBSystemId::FlightControls, kFlcsDegrade, kFlcsFail},
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};
constexpr FBZoneSystem kAftSystems[] = {
    {FBSystemId::Engine, kEngineDegrade, kEngineFail},
    {FBSystemId::Engine2, kEngineDegrade, kEngineFail},
    {FBSystemId::Rwr, kAvionicsFail, kAvionicsFail},
    {FBSystemId::Countermeasures, kAvionicsFail, kAvionicsFail},
    {FBSystemId::Structure, kStructDegrade, kStructFail},
};
/* A MOVER DECLARES ONE ZONE AND `Structure` ALONE — exactly what `target_hard` declares and for the
 * same reason: nothing else about it has a behaviour to lose. */
constexpr FBZoneSystem kHullSystems[] = {{FBSystemId::Structure, kStructDegrade, kStructFail}};

} // namespace

/* The zone boundaries are metres FORWARD of the CG, and the CG is the deck's own station at half the
 * row's length: nose tip at +L/2, tail at -L/2, with the cockpit sill at +0.24 L and the engine bay
 * front at -0.16 L. Presented AREA and EXTENT are the declared span and length — half span frontal,
 * half length lateral — which is also what feeds the EYE (units/FBSimUnit publishes 2x each into
 * FBVisualSignature). Span and length are the ONE sensor input a published source carries for every
 * airframe without exception, which is why the eye is the catalogue's best-modelled channel.
 * Frontal area ~ 0.06 x span x length and lateral ~ 0.35 x span x length [SET], the same two shape
 * factors for every row: a gun burst's presented-area law needs a number, and one declared pair of
 * factors applied to published dimensions is honester than a table of invented areas. */
void FBAircraftCatalogue::Add(const std::string &key, const std::string &name,
                              const std::string &fdmModel, const FBAircraftSpec &spec,
                              double spanM, double lenM) {
  Rows_.push_back(std::make_unique<Row>());
  Row &r = *Rows_.back();
  r.Key = key;
  r.Name = name;
  r.FdmModel = fdmModel;
  r.Spec = spec;
  r.Spec.Key = r.Key.c_str();
  r.Spec.Name = r.Name.c_str();
  r.Spec.FdmModel = r.FdmModel.c_str();

  const int zones = r.Spec.IsMover() ? 1 : 4;
  if (r.Spec.IsMover()) {
    r.Zones[0] = {FBDamageZone::Center, -0.50 * (lenM), 0.50 * (lenM), kHullSystems, 1};
  } else {
    r.Zones[0] = {FBDamageZone::Nose, 0.24 * (lenM), 0.50 * (lenM), kNoseSystems, 3};
    r.Zones[1] = {FBDamageZone::Forward, 0.0, 0.24 * (lenM), kForwardSystems, 4};
    r.Zones[2] = {FBDamageZone::Center, -0.16 * (lenM), 0.0, kCenterSystems, 3};
    r.Zones[3] = {FBDamageZone::Aft, -0.50 * (lenM), -0.16 * (lenM), kAftSystems, 5};
  }
  r.Spec.Layout = FBDamageLayout{r.Zones, zones, 0.06 * (spanM) * (lenM), 0.35 * (spanM) * (lenM),
                                 0.5 * (spanM), 0.5 * (lenM), 0.5 * (lenM)};
}

} // namespace FlightBox
