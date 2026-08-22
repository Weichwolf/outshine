#include "Assembly.h"

namespace outshine {

bool Assemble(const Scenario &declared, Store &into, Column<Vehicle> &vehicles,
              Column<Drive> &driven, Assembled &out, std::string &error) {
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
    if (!vehicles.Put(body, vehicle)) {
      error = "the vehicle's numbers found no column seat for '" + vehicle.Name + "'";
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
  if (declared.Driven.Declared) {
    if (!into.Alive(out.PlayerMind)) {
      error = "a drive is declared and no mind stands to take it -- declare a player";
      return false;
    }
    out.Nav = into.Add(Role::Tool);
    out.Assignment = into.Add(Role::Assignment);
    if (!into.Alive(out.Nav) || !into.Alive(out.Assignment) ||
        !into.Link(out.PlayerMind, Relation::Uses, out.Nav) ||
        !into.Link(out.PlayerMind, Relation::Assigned, out.Assignment) ||
        !driven.Put(out.Assignment, declared.Driven)) {
      error = into.Error().empty() ? "the drive's numbers found no column seat" : into.Error();
      return false;
    }
  }
  return true;
}

} // namespace outshine
