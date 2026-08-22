#include "Assembly.h"

namespace outshine {

bool Assemble(const Scenario &declared, Store &into, Assembled &out, std::string &error) {
  out = Assembled{};
  for (const Vehicle &vehicle : declared.Vehicles) {
    const Entity body = into.Add(Role::Body);
    if (!into.Alive(body)) {
      error = into.Error();
      return false;
    }
    const bool steers = vehicle.TurningCircleM > 0.0;
    const bool drives = vehicle.PeakTorqueNm > 0.0 && vehicle.FinalDrive > 0.0;
    const bool brakes = vehicle.BrakeTorqueNm > 0.0;
    if ((steers && !into.Give(body, tags::DoesSteer)) ||
        (drives && !into.Give(body, tags::DoesDrive)) ||
        (brakes && !into.Give(body, tags::DoesBrake))) {
      error = into.Error();
      return false;
    }
    out.Bodies.push_back(body);
    if (!declared.Played.Is.empty() && declared.Played.Is == vehicle.Name) {
      out.PlayerBody = body;
    }
  }
  if (!declared.Played.Is.empty()) {
    if (out.PlayerBody == kNoEntity) {
      error = "the player is '" + declared.Played.Is + "', which no vehicle declares";
      return false;
    }
    out.PlayerMind = into.Add(Role::Mind);
    if (!into.Alive(out.PlayerMind) ||
        !into.Link(out.PlayerBody, Relation::DrivenBy, out.PlayerMind)) {
      error = into.Error();
      return false;
    }
  }
  return true;
}

} // namespace outshine
