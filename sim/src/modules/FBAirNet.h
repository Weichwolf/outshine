/* FlightBox — the `net` block ON AN AIRCRAFT, and it is the same net a ground position joins.
 *
 * `net <name>` with `control`, `member`, `period`, `hold`, `wcs` was generic from the day it was
 * written; what it had never had was a member with wings. This file is that member, and it is
 * deliberately ONE small file shared by both airframe families rather than two nearly identical
 * blocks: the runner generates the same keys for every member (core/FBMissionFile.cpp), so every
 * member must answer them the same way or the block means two things.
 *
 * WHAT AN AIRCRAFT CAN HONOUR, AND WHAT IT REFUSES — a refusal is a spawn failure with a reason, which
 * is the point: a mission may not declare a doctrine this airframe cannot fly.
 *
 *   net_link / net_period_s / net_hold   the terminal it already carries. A fighter's Link-16 IS the
 *                                        net; it needs no second radio to be on one.
 *   net_control <callsign>               it LISTENS: that node's transmitted weapons-control word
 *                                        becomes its own, and the node going quiet is what makes the
 *                                        declared autonomy apply.
 *   net_wcs <free|tight|hold>            it IS the node: it transmits that word, and it publishes an
 *                                        FBNetReport built from its own anonymous echo.
 *   net_autonomy <free|hold>             the fallback. `tight` is REFUSED, because "engage only what
 *                                        you were assigned" needs target addressing and this tree has
 *                                        none (doc/player-layer.md §9.6 (c)).
 *   net_sector                           REFUSED: a sector of responsibility is a thing a position in
 *                                        the ground has. Only generated when an author writes one.
 *
 * IT ADDS NO REGISTRY READER and no new FBState block: every line here reads a published block or an
 * argument. tools/verify_layers.py's PERCEPTION_READERS list stays at six. */
#ifndef FBAIRNET_H
#define FBAIRNET_H

#include <cmath>
#include <cstring>
#include <sstream>
#include <string>

#include "FBAvionicsBlocks.h"
#include "FBDatalinkSystem.h"
#include "FBGeodesy.h"
#include "FBNetReport.h"
#include "FBPilot.h"

namespace FlightBox::Modules {

/* NotMine = this is not a net key at all, so the module goes on looking. */
enum class FBAirNetResult { NotMine, Ok, Bad };

/* The state a netted aircraft carries, and there is no more of it. Every field defaults to "this
 * aircraft is on no net", which is the whole conservation argument: a mission that declares none is
 * byte-identical to before this file existed. */
struct FBAirNet {
  bool On = false;                                   /* the mission put this unit in a net block */
  bool IsNode = false;                               /* ...as its control node */
  FBWeaponsControl NodeWcs = FBWeaponsControl::Free; /* what a NODE transmits */
  char Control[kDatalinkCallsignLen] = {};           /* whose word a MEMBER takes */
  bool NodeHeard = false;

  bool Member() const { return On && !IsNode && Control[0] != 0; }
};

/* One runner-generated key. `why` is filled on Bad and is the module's RejectSetup message. */
inline FBAirNetResult FBApplyAirNetKey(const std::string &key, const std::string &value,
                                       Sensors::FBDatalinkSystem &terminal, Pilot::FBPilot &pilot,
                                       FBAirNet &net, const char **why) {
  auto bad = [&](const char *m) { *why = m; return FBAirNetResult::Bad; };

  if (key == "net_link") {
    std::istringstream in(value);
    std::string mode;
    double rangeM = 0.0, mastM = 0.0;
    if (!(in >> mode)) return bad("want 'wire <mastM>' or 'radio <rangeM> <mastM>'");
    if (mode == "wire") {
      in >> mastM;
      terminal.SetLinkMode(Sensors::FBDatalinkSystem::LinkMode::Wire, mastM);
    } else if (mode == "radio") {
      if (!(in >> rangeM >> mastM)) return bad("want 'radio <rangeM> <mastM>'");
      terminal.SetLinkMode(Sensors::FBDatalinkSystem::LinkMode::Radio, mastM);
      /* 0 = the terminal's own reach, which is the mission-file default and the honest one: a net
       * declaration is doctrine, not a radio specification. */
      if (rangeM > 0.0) terminal.SetMaxRangeM(rangeM);
    } else {
      return bad("want wire|radio");
    }
    net.On = true;
    terminal.SetPowered(true);
    return FBAirNetResult::Ok;
  }
  if (key == "net_period_s") {
    double s = std::atof(value.c_str());
    if (!(s > 0.0)) return bad("want a positive reporting period");
    terminal.SetNetPeriodS(s);
    return FBAirNetResult::Ok;
  }
  if (key == "net_hold") {
    double n = std::atof(value.c_str());
    if (!(n > 0.0)) return bad("want a positive number of cycles");
    terminal.SetHoldCycles(n);
    return FBAirNetResult::Ok;
  }
  if (key == "net_sector")
    return bad("a sector of responsibility belongs to a position in the ground, not to an aircraft");
  if (key == "net_autonomy") {
    FBWeaponsControl w = FBWeaponsControl::Hold;
    if (!FBWeaponsControlFromString(value.c_str(), w)) return bad("want free|tight|hold");
    if (w == FBWeaponsControl::Tight)
      return bad("'tight' needs target addressing, which this tree has none of");
    pilot.SetAutonomy(w);
    return FBAirNetResult::Ok;
  }
  if (key == "net_control") {
    if (value.empty() || value.size() >= kDatalinkCallsignLen) return bad("callsign too long");
    std::strncpy(net.Control, value.c_str(), kDatalinkCallsignLen - 1);
    net.Control[kDatalinkCallsignLen - 1] = 0;
    terminal.SetControlNode(value);
    net.On = true;
    return FBAirNetResult::Ok;
  }
  if (key == "net_wcs") {
    FBWeaponsControl w = FBWeaponsControl::Free;
    if (!FBWeaponsControlFromString(value.c_str(), w)) return bad("want free|tight|hold");
    if (w == FBWeaponsControl::Tight)
      return bad("'tight' needs target addressing, which this tree has none of");
    net.IsNode = true;
    net.NodeWcs = w;
    net.On = true;
    terminal.SetTransmit(true);
    return FBAirNetResult::Ok;
  }
  return FBAirNetResult::NotMine;
}

/* PER TICK, the member half: the node's word arrives on the cooperative block this aircraft already
 * reads, so the doctrine travels on a channel that can fail — jammed, over the horizon, or shot down.
 * When it does, pilot/FBPilot falls back to the DECLARED autonomy, which is the whole mechanism
 * doc/air-defence-network.md §5 built for the ground and this reuses unchanged. */
inline void FBRunAirNetMember(const FBDatalinkBlock &link, FBAirNet &net, Pilot::FBPilot &pilot) {
  if (!net.Member()) return;
  bool heard = false;
  FBWeaponsControl w = FBWeaponsControl::Free;
  if (link.H.Readable()) {
    for (int i = 0; i < link.TrackCount; i++) {
      if (std::strcmp(link.Tracks[i].Callsign, net.Control) != 0) continue;
      heard = true;
      w = link.Tracks[i].Net.Wcs;
      break;
    }
  }
  net.NodeHeard = heard;
  pilot.SetControlNodeHeard(heard);
  if (heard) pilot.SetWeaponsControl(w);
}

/* PER TICK, the reporting half — and the whole perception boundary is in what it does NOT do: it reads
 * this aircraft's own nearest ANONYMOUS echo (FBRadarContact has no id and no team), rebuilds the POINT
 * it sat at, and stamps its own look age on it. It cannot say hostile (no such value exists on the
 * report), it cannot name a unit (there is no id field), and it hands nobody a track. A friendly IFF
 * reply is the ONE positive identification in this tree and is therefore skipped — silence is not its
 * opposite, so NoReply and NotInterrogated are reported like anything else, unnamed. */
inline FBNetReport FBBuildAirNetReport(const FBAirNet &net, const FBRadarBlock &r, bool transmitting,
                                       double latDeg, double lonDeg, double elevM) {
  FBNetReport out;
  if (!net.On || !transmitting || !r.H.Readable()) return out;
  const FBRadarContact *best = nullptr;
  for (int i = 0; i < r.ContactCount; i++) {
    if (r.Contacts[i].Iff == FBIffReply::Friendly) continue;
    if (!best || r.Contacts[i].RangeM < best->RangeM) best = &r.Contacts[i];
  }
  if (!best) return out;
  double brg = best->BearingDeg * kDeg2Rad, elv = best->ElevAngleDeg * kDeg2Rad;
  double horiz = best->RangeM * std::cos(elv);
  double coslat = std::cos(latDeg * kDeg2Rad);
  out.Reporting = true;
  out.LatDeg = latDeg + horiz * std::cos(brg) / kMPerDeg;
  out.LonDeg = lonDeg + (std::fabs(coslat) > 1e-6 ? horiz * std::sin(brg) / (kMPerDeg * coslat) : 0.0);
  out.AltM = (float)(elevM + best->RangeM * std::sin(elv));
  out.TgtLookAgeS = best->LookAgeS;
  /* ONLY A NODE transmits the doctrine word. A member's report carries the field's default and nobody
   * reads it: a member is found by callsign, and the callsign a member listens for is the node's. */
  if (net.IsNode) out.Wcs = net.NodeWcs;
  return out;
}

} // namespace FlightBox::Modules
#endif
