#include "FBSystemHealth.h"

namespace FlightBox {

const char *FBSystemIdStr(FBSystemId id) {
  switch (id) {
    case FBSystemId::Engine: return "engine";
    case FBSystemId::FlightControls: return "flight_controls";
    case FBSystemId::Structure: return "structure";
    case FBSystemId::AirData: return "air_data";
    case FBSystemId::RadarAlt: return "radar_alt";
    case FBSystemId::Nav: return "nav";
    case FBSystemId::Radar: return "radar";
    case FBSystemId::FireControl: return "fire_control";
    case FBSystemId::Stores: return "stores";
    case FBSystemId::Gun: return "gun";
    case FBSystemId::Datalink: return "datalink";
    case FBSystemId::Rwr: return "rwr";
    case FBSystemId::Countermeasures: return "countermeasures";
    case FBSystemId::Engine2: return "engine2";
    case FBSystemId::Count: break;
  }
  return "?";
}

double FBSystemHealth::AddKinetic(int zone, double fluxJm2) {
  if (zone < 0 || zone >= kMaxZones || fluxJm2 <= 0.0) return 0.0;
  Kinetic_[zone] += fluxJm2;
  return Kinetic_[zone];
}

const char *FBHealthStateStr(FBHealthState s) {
  switch (s) {
    case FBHealthState::Intact: return "intact";
    case FBHealthState::Degraded: return "degraded";
    case FBHealthState::Failed: return "failed";
  }
  return "?";
}

bool FBSystemHealth::Worsen(FBSystemId id, FBHealthState s) {
  FBHealthState &cur = S_[(int)id];
  if (s <= cur) return false;   /* monotone (class banner): nothing ever heals */
  cur = s;
  uint32_t bit = 1u << (int)id;
  Degraded_ &= ~bit;
  Failed_ &= ~bit;
  if (s == FBHealthState::Failed) Failed_ |= bit;
  else Degraded_ |= bit;
  return true;
}

void FBSystemHealthTelemetry::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("dmg_hits");
  schema.Add("dmg_failed");     /* bitmask over FBSystemId */
  schema.Add("dmg_degraded");
  schema.Add("dmg_effective");  /* FBSystemHealth::CombatEffective */
}

void FBSystemHealthTelemetry::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(H_.Hits());
  row.Push((int)H_.FailedMask());
  row.Push((int)H_.DegradedMask());
  row.Push(H_.CombatEffective());
}

} // namespace FlightBox
