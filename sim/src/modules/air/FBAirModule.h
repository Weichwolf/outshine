/* FlightBox — FBAirModule: a CATALOGUE AIRCRAFT. ONE CLASS, EIGHTEEN CATALOGUE ROWS (core/FBAircraft.h)
 * — the same value-type-over-class decision modules/ground, modules/stores and modules/missile make,
 * and the reason `C7` is a bounded round rather than eighteen airframe projects. A SECOND C++ airframe
 * class in this directory is a defect, not a precedent.
 *
 * IT IS AN FBModule AND IT IS NOT A MODULE IN THE DOCUMENTATION SENSE. A module is an airframe
 * FlightBox is JUDGED on: a pinned or anchor-measured deck, a full avionics bus, ~20 classes and a
 * reference base directory of its own; two exist and there will not be many more. A catalogue cell is
 * an airframe FlightBox is NOT judged on — one parametric class, one catalogue row, a GENERATED deck or
 * none at all, and a declared pilot tier. What it accepts about itself is the anchor BANDS of
 * doc/modules/air/flight-model-recipe.md §7.1; a row outside its band may fly in a mission and may not
 * answer a campaign question.
 *
 * TWO MOTION LAWS, ONE SEAM, and no row gets both (module.md §Spec 4). A row flies on JSBSim iff its own
 * manoeuvre decides an outcome AND its envelope is published; those are the same fact seen twice, so
 * the test never splits a row. Ten rows have a deck and eight are integrated kinematically by
 * FBAirMover — which is also the class that answers the one collision the contract named in advance: an
 * airborne unit without an FBFdm would report weight on wheels by construction, so a mover publishes
 * its own Airborne() instead.
 *
 * THE PERCEPTION BOUNDARY DOES NOT GROW. The four detectors are a configured FBAirRadar (itself
 * sensors/FBRadarSystem), and sensors/FBRwrSystem, FBIrstSystem and FBVisualSystem VERBATIM; the one
 * new slot, sensors/FBNetLinkSystem, is a two-line derivation of FBDatalinkSystem that adds no include.
 * tools/verify_layers.py's PERCEPTION_READERS list stays at SIX.
 *
 * THE MOST DANGEROUS LINE IN THE DIRECTORY is §Spec 7's, and it is enforced here rather than intended:
 * AN EARLY-WARNING AIRCRAFT MOVES AN ANTENNA, IT NEVER CREATES A TRACK. What a node publishes is an
 * FBNetReport — a point reconstructed from its own anonymous echo, with its own look age, no id field
 * and no team field. What a member does with it is re-centre its own search volume over the command bus
 * and then find, firm and gate the target with its own radar. */
#ifndef FBAIRMODULE_H
#define FBAIRMODULE_H

#include <memory>
#include <string>

#include "FBAircraft.h"
#include "FBAirFireControl.h"
#include "FBAirMover.h"
#include "FBAirPilot.h"
#include "FBAirRadar.h"
#include "FBModule.h"
#include "FBNetLinkSystem.h"
#include "FBSystemSlots.h"

namespace FlightBox::Modules {

class FBAirTelemetry;

class FBAirModule : public FBModule {
public:
  explicit FBAirModule(const FBAircraftSpec &spec);
  ~FBAirModule() override;

  const FBAircraftSpec &Spec() const { return Spec_; }

  const char *FdmModelName() const override { return Spec_.FdmModel; }
  void AttachFdm(Fdm::FBFdm &fdm) override;
  /* THE KIND IS NOT THE MOTION. An airborne mover stays `Aircraft` — doc/modules/ground/cast.md
   * proposed a `Vehicle` kind for a GROUND mover and that value stays a ground concept, because
   * FBRadarSystem filters `Aircraft` and a mover that was not one could be seen by no radar in the
   * tree, which would delete the row's only reason to exist. */
  Units::FBUnitKind UnitKind() const override { return Units::FBUnitKind::Aircraft; }

  /* THE ANSWER TO COLLISION 1 (module.md §Gaps). A unit without an FBFdm yields AnyWow = true by
   * construction, and a KC-135 at 25 000 ft claiming weight on wheels is wrong for every consumer of
   * that bit. A mover states its own condition; a ground position's default stays correct, because a
   * launcher really is on the ground. */
  bool Airborne() const override { return Spec_.IsMover(); }

  void Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units = nullptr,
           const World::FBWorld *world = nullptr) override;

  const FBDamageLayout &DamageLayout() const override { return Spec_.Layout; }
  double RadarCrossSectionM2() const override { return Spec_.RcsM2; }

  /* A POINT, AN AGE, AND NOTHING THAT NAMES ANYBODY. Published only by a row whose catalogue entry says
   * it is an early-warning node, and only while its own set is radiating and its terminal transmits. */
  FBNetReport NetReport() override { return Node_; }
  FBTelemetrySource *NetTelemetry() override;

  Systems::FBAutopilot &Autopilot() override { return AP_; }
  Systems::FBFlightControl &FlightControl() override { return FC_; }
  FBAirPilot &PilotSystem() override { return Pilot_; }   /* covariant, like every module's */
  Systems::FBAirframeControls &Controls() override { return *Ctrl_; }
  Systems::FBDisplaySystem &Displays() override { return Disp_; }
  Systems::FBAirDataSystem &AirDataSystem() override { return AirData_; }
  Systems::FBNavSystem &NavSystem() override { return Nav_; }
  Systems::FBWarningSystem &WarningSystem() override { return Warn_; }
  Systems::FBRadarAltimeter &RadarAltimeter() override { return RadarAlt_; }
  FBCommandBus &Commands() override { return Cmds_; }
  /* THE COOPERATIVE terminal, and it is a DIFFERENT box from the controller's feed below: on a T4 row
   * this block carries Link-16 PPLI, which is exactly why a cue may not be written into it. */
  Sensors::FBDatalinkSystem &Datalink() override { return Datalink_; }
  FBAirRadar &Radar() override { return Radar_; }
  Sensors::FBRwrSystem &Rwr() override { return Rwr_; }
  Sensors::FBIrstSystem &Irst() override { return Irst_; }
  Sensors::FBVisualSystem &Visual() override { return Eye_; }
  Sensors::FBCountermeasureSystem &Countermeasures() override { return Cm_; }
  Weapons::FBStoresSystem &Stores() override { return Stores_; }
  Weapons::FBGunSystem &Guns() override { return Gun_; }
  const FBState &Telemetry() const override { return State_; }
  const Systems::FBGuidance &LastGuidance() const override { return LastG_; }
  int LastSubsteps() const override { return LastSub_; }

  void SetUnitIdentity(int unitId, FBUnitTeam team) override {
    Radar_.SetIdentity(unitId, team);
    Rwr_.SetIdentity(unitId, team);
    Irst_.SetIdentity(unitId, team);
    Eye_.SetIdentity(unitId);   /* the id only — an eye is given no team to read */
    Datalink_.SetIdentity(unitId, team);
    NetLink_.SetIdentity(unitId, team);
    Stores_.SetUnitId(unitId);
    Gun_.SetUnitId(unitId);
  }

  void SetCloudSky(const FBCloudSky &sky) override { Irst_.SetSky(sky); Eye_.SetSky(sky); }
  void SetSolar(const FBSolar &solar) override { FBSolarToEnv(solar, State_); }
  void SetGroundAsl(float m) override { GroundAslM_ = m; }
  FBFlightPlan &FlightPlan() override { return Plan_; }
  void SetRunway(const FBRunway &rwy) override {
    Rwy_ = rwy;
    HaveRunway_ = true;
    Nav_.SetBullseye(rwy.ThresholdLatDeg, rwy.ThresholdLonDeg);
  }

  bool ApplySetup(const std::string &key, const std::string &value) override;

private:
  friend class FBAirTelemetry;

  void ServiceCommands(FBCommandGroup group);
  void ApplyCommand(const FBAvionicsCommand &c, FBCommandOutcome &outcome, FBCommandReason &reason);
  void ApplyPilotCommands(const Pilot::FBPilotCommands &c);
  void PublishPlatform(const Fdm::fb_fdm_state &st);
  /* THE NODE HALF of §Spec 7: rebuild the report from this aircraft's OWN nearest anonymous contact. */
  void PublishNetReport(const Fdm::fb_fdm_state &st);
  /* THE MEMBER HALF: two command-bus entries per decision tick, each latency-charged and each
   * rejectable — re-centre the own search volume's azimuth on the bearing to the cued point and its
   * elevation on atan2(dAlt, planar range). NOTHING ELSE crosses. */
  void ConsumeCue(const Fdm::fb_fdm_state &st);
  /* The BRIEFED half of the same seam: the controller's call as mission data, entered through the same
   * two command-bus pointings the live cue uses, on the first decision tick this aircraft flies. */
  void EnterBriefedGci(const Fdm::fb_fdm_state &st);
  static bool Due(double &accS, double dt, double hz);

  const FBAircraftSpec &Spec_;

  Systems::FBAutopilot AP_;
  Systems::FBFlightControl FC_;
  FBAirRadar Radar_;
  Sensors::FBRwrSystem Rwr_;
  Sensors::FBIrstSystem Irst_;
  Sensors::FBVisualSystem Eye_;
  Sensors::FBDatalinkSystem Datalink_;   /* the cooperative net: PPLI, T4 rows only */
  Sensors::FBNetLinkSystem NetLink_;     /* the controller's feed, its own FBState block */
  Sensors::FBCountermeasureSystem Cm_;
  Weapons::FBStoresSystem Stores_;
  Weapons::FBGunSystem Gun_;
  /* NULL FOR EVERY ROW THAT DECLARES NO WEAPON, and that is the tier expressed as a member rather than
   * as a branch: a tanker has no fire-control computer, so FBState::FireControl stays Invalid for it
   * and every employment gate in pilot/FBPilot reads that as "this aeroplane cannot shoot". */
  std::unique_ptr<FBAirFireControl> Fc_;
  FBAirPilot Pilot_;
  std::unique_ptr<Systems::FBAirframeControls> Ctrl_;   /* NoOp until AttachFdm, FDM-bound after */
  Systems::FBDisplaySystem Disp_;
  Systems::FBAirDataSystem AirData_;
  Systems::FBNavSystem Nav_;
  Systems::FBWarningSystem Warn_;
  Systems::FBRadarAltimeter RadarAlt_;
  FBAirMover Mover_;
  FBCommandBus Cmds_;
  FBFlightPlan Plan_;
  FBRunway Rwy_;
  FBState State_{};
  Systems::FBGuidance LastG_{};
  std::unique_ptr<FBAirTelemetry> Tele_;

  Fdm::FBFdm *Fdm_ = nullptr;   /* borrowed, never owned; null for every mover row */
  FBNetReport Node_{};
  bool   HaveRunway_ = false;
  float  GroundAslM_ = 0.0f;
  double SimTimeS_ = 0.0, AccS_ = 0.0;
  double SensorAccS_ = 0.0, PilotAccS_ = 0.0;
  int    LastSub_ = 0;
  /* The two staleness terms of a cue, summed, and -1 when this aircraft is uncued: the node's own look
   * age at the moment it reported plus the age the receiver computed. Both are published so a mission
   * can read WHICH HALF hurt. */
  double CueAgeS_ = -1.0;
  double CueAzDeg_ = 0.0, CueElDeg_ = 0.0;
  bool   HaveCue_ = false;
  int    CueCount_ = 0;
  /* THE BRIEFED CALL IS TYPED WHEN THE AEROPLANE IS FLYING, not when the mission file is parsed: a BRAA
   * is a TRUE bearing and an ABSOLUTE altitude, and both become antenna angles only against this
   * aircraft's own heading and altimeter — neither of which exists at ApplySetup time. */
  bool   GciPending_ = false;
  double GciBrgDeg_ = 0.0, GciRangeM_ = 0.0, GciAltM_ = 0.0;
  double NextCueS_ = 0.0;   /* one antenna pointing per NET cycle: a cue is a transmission, not a tick */
};

} // namespace FlightBox::Modules
#endif
