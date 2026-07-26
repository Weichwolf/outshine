/* FlightBox — FBRadarSystem: the module's Sensors slot, filled with a REAL default the way
 * FBDatalinkSystem filled Comms and FBDisplaySystem filled Displays (which is why no FBSensorSystem NoOp
 * remains in FBSystemSlots.h). Interface plus a generic active air-to-air radar; a module whose set
 * genuinely differs subclasses it — modules/f16/FBF16Fcr is the APG-68 with its ACM sub-modes.
 *
 * WHAT IT MODELS, and how it differs from the datalink (read FBDatalinkSystem.h's banner first — the two
 * are meant to be compared). The net is COOPERATIVE: the sender broadcasts who and where it is, so range
 * is a radio question and identity is free. A radar is UNCOOPERATIVE: it illuminates a volume and gets
 * echoes back. Consequences this class implements literally:
 *   - a SCAN VOLUME tied to the airframe's nose (FBRadarScanVolume, body-referenced azimuth/elevation
 *     about the boresight, plus a range gate). Outside it there is no contact at all — not a stale one,
 *     not a faint one. The volume rolls and pitches with the jet, which is what makes the F-16's
 *     HUD-referenced ACM boxes behave the way the pilot expects in a turn.
 *   - a FRAME TIME, not a live feed: the antenna needs FrameS to sweep its volume once. Between looks
 *     the contact's geometry stands still and its LookAgeS counts up — the same "the picture is as old
 *     as the last look" property the datalink has, arrived at from the opposite direction.
 *   - CONTACT BUILD-UP: one look is a hit, not a track. kHitsToFirm consecutive looks are needed before
 *     an echo becomes a contact the rest of the aircraft may see, so acquisition costs
 *     kHitsToFirm * FrameS seconds and a target that flashes through the beam is never reported.
 *   - CONTACT LOSS with COAST: a firm track that stops being seen is HELD for kCoastFrames looks before
 *     it is dropped, because a real track filter extrapolates through a missed look rather than blinking
 *     the symbol out. The coast is observable (FBRadarContact::Coasting/LookAgeS) and is what makes the
 *     loss a process rather than an event.
 *   - ANONYMITY (core/FBRadarContact.h, and the anti-cheat point of this whole file): the registry knows
 *     who every unit is; the contact this class publishes carries geometry only. The internal track
 *     table keys on the unit id because SOMETHING has to correlate look to look and this simulator has no
 *     measurement-association filter — that key never leaves this object.
 *   - IFF as the ONE identity channel (doc/f16/datalink-iff.md): a valid Mode-4 reply proves friendly,
 *     no reply proves nothing. See Interrogate().
 *
 * DELIBERATELY NOT MODELLED — TERRAIN MASKING. A real look-down picture is cut by the horizon and by
 * terrain along the line of sight, and this class does neither: it takes no FBWorld and samples no DEM.
 * For air-to-air between two airborne units — the training fight this sensor exists for — the line of
 * sight is free and the scan volume plus the range gate already decide everything. Masking needs a DEM
 * ray-march per contact per look and belongs to whichever system first has a reason to pay for it (the
 * same call FBDatalinkSystem::RadioHorizonM's banner makes about the radio path). Nothing here silently
 * approximates it, so nobody can mistake the picture for one that has it.
 *
 * WHERE THE REGISTRY MAY GO (CLAUDE.md "Kein Cheaten"): this class and FBDatalinkSystem are the only two
 * things in systems/ or modules/ that ever hold a units/FBUnitRegistry. Everything downstream — above
 * all FBPilot — reads contacts out of FBState like any other instrument. */
#ifndef FBRADARSYSTEM_H
#define FBRADARSYSTEM_H

#include "FBRadarContact.h"
#include "FBState.h"
#include "FBTeam.h"
#include "FBTelemetry.h"
#include "FBUnits.h"
#include "FBFdm.h"

namespace FlightBox {

class FBUnit;
class FBUnitRegistry;

/* One antenna scan pattern, complete: where it points relative to the NOSE, how wide and how tall it is,
 * how far it reaches, how long one sweep takes, and whether it locks what it finds. A mode IS one of
 * these (modules/f16/FBF16Fcr's table) — which is why ActiveVolume() below is the single override point
 * a module needs to give the pilot a whole mode set.
 *
 * The elevation window is centre+half rather than a symmetric half-angle because a real vertical-scan
 * pattern is not centred on the boresight (it reaches far above it and barely below), and the same field
 * carries a slewable pattern's commanded position. */
struct FBRadarScanVolume {
  double AzCenterDeg = 0.0;    /* pattern centre off the nose, + = right */
  double AzHalfDeg = 30.0;     /* half-width; the pattern spans centre +/- this */
  double ElCenterDeg = 0.0;    /* pattern centre above/below the boresight plane */
  double ElHalfDeg = 10.5;
  double RangeM = 40.0 * kNmToM;
  double FrameS = 2.0;         /* one complete sweep of this volume */
  bool   AutoAcquire = false;  /* lock the nearest firm track without being asked (the ACM property) */
  bool   Active = true;        /* false = the set is not radiating (an OFF mode) */
  /* All power on ONE target: while a lock is held, this pattern looks at the locked track and at nothing
   * else, so every other trackfile stops being refreshed and coasts away. That is what a single-target
   * track IS (doc/f16/radar-sensors.md: "STT — all radar power on one target"), and expressing it as a
   * property of the PATTERN keeps the whole ACM->STT transition inside the one volume hook. */
  bool   SingleTarget = false;
};

class FBRadarSystem : public FBTelemetrySource {
public:
  /* Generic placeholders for "some fighter-class air-to-air set", deliberately NOT a real radar's
   * figures — the same role FBDatalinkSystem::kGenericRangeNm and FBPilot's generic V-speeds play.
   * FBF16Fcr supplies the APG-68's. */
  static constexpr double kGenericRangeNm = 40.0;
  static constexpr double kGenericAzHalfDeg = 30.0;
  static constexpr double kGenericElHalfDeg = 10.5;
  static constexpr double kGenericFrameS = 2.0;

  /* Two consecutive looks before an echo is a contact: the classic minimum that separates a track from a
   * single-look return, and the smallest number that still makes acquisition take measurable TIME
   * (kHitsToFirm * FrameS) rather than happening in the tick the geometry first allows it. */
  static constexpr int kHitsToFirm = 2;
  /* Missed looks a firm track is extrapolated through before it is dropped — the same "hold it briefly
   * rather than blink it out" rule as FBDatalinkSystem::kDropAfterCycles, in looks instead of cycles.
   * The floor is what keeps that meaningful for a FAST pattern: an STT revisiting its target every
   * 0.1 s would otherwise drop it 0.3 s after the gimbal limit, whereas a track filter extrapolates a
   * lost target for a second or so however often the antenna was hitting it. Coast is therefore
   * max(kMinCoastS, kCoastFrames * FrameS) — frames for a search sweep, seconds for a stare. */
  static constexpr double kCoastFrames = 3.0;
  static constexpr double kMinCoastS = 1.0;
  /* How often a held contact is re-challenged. An interrogation is a transmission; a set that challenged
   * every contact on every look would be radiating far more than the real box does. */
  static constexpr double kIffPeriodS = 5.0;

  ~FBRadarSystem() override = default;

  /* Boot wiring, once per unit (units/FBSimUnit's constructor through FBModule::SetUnitIdentity): who
   * this aircraft is, so the set skips its own echo and the interrogator knows whose crypto it holds. */
  void SetIdentity(int selfId, FBUnitTeam team) { SelfId_ = selfId; SelfTeam_ = team; }

  void SetPowered(bool on);
  bool Powered() const { return Powered_; }

  /* The IFF box (AN/APX-113, doc/f16/datalink-iff.md) is a combined INTERROGATOR + TRANSPONDER. It is
   * modelled here rather than as a slot of its own because the only thing in this simulator that ever
   * challenges anything is a radar contact, and the only thing that ever reads a reply is this class —
   * a separate system would be one bool and one function reachable from nowhere else. The transponder
   * half is published in the unit's emission signature (units/FBUnit.h's FBUnitSignature) exactly like
   * the datalink's XMT switch: what this aircraft answers with is observable, not private. */
  void SetIffTransponder(bool on) { IffXpdr_ = on; }
  void SetIffInterrogator(bool on) { IffInterrogator_ = on; }
  bool IffTransponder() const { return IffXpdr_; }
  bool IffInterrogator() const { return IffInterrogator_; }

  /* The configured SEARCH pattern — what ActiveVolume() returns unless a module overrides it. */
  void SetSearchVolume(const FBRadarScanVolume &v) { Search_ = v; }
  const FBRadarScanVolume &SearchVolume() const { return Search_; }

  bool Locked() const { return LockedNum_ != 0; }
  int LockedTrackNum() const { return LockedNum_; }

  /* The override point (class banner). `st` is this aircraft's own pose/attitude, `net` the borrowed
   * registry of published unit snapshots (null = nothing to see), `simTimeS` the module's own sim clock
   * — absolute, so the picture cannot depend on how often the module happens to cycle this slot. Writes
   * contacts into `state` and nothing else. */
  virtual void Run(FBState &state, const fb_fdm_state &st, const FBUnitRegistry *net, double simTimeS);

  const char *TelemetryName() const override { return "fcr"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

protected:
  /* THE hook a module overrides (modules/f16/FBF16Fcr): the pattern in force RIGHT NOW. A mode set is
   * nothing but a choice among FBRadarScanVolumes, and a lock-follows-the-target behaviour is nothing but
   * returning a different one while Locked() — so one virtual carries a whole FCR. */
  virtual const FBRadarScanVolume &ActiveVolume() const { return Search_; }

  /* What a module labels its current mode with, for telemetry only (0 = the generic search). Kept as an
   * ordinal because FBState/telemetry carry numbers, and kept OUT of FBRadarContact because a contact
   * knows nothing about the mode that found it. */
  virtual int ModeOrdinal() const { return 0; }

  /* Slant range + the WORLD-referenced pair (true bearing, elevation angle above the local horizontal)
   * + the BODY-referenced azimuth/elevation from `own` to a geodetic point. Body-referenced means the
   * full roll/pitch/yaw rotation is applied, so those angles are what the antenna sees, not what a map
   * would show. Protected + static: an override that wants its own geometry (a gimbal limit, a HUD-window
   * test) computes it the one way this file does. */
  static void RelativeLos(const fb_fdm_state &own, double tgtLatDeg, double tgtLonDeg, double tgtAltM,
                          double &rangeM, double &bearingDeg, double &elevAngleDeg, double &azDeg,
                          double &elDeg);

private:
  /* One internal track file. UnitId is the look-to-look correlation key and NEVER leaves this object
   * (class banner); everything published is the FBRadarContact built from the rest. */
  struct Track {
    int    UnitId = 0;
    int    TrackNum = 0;
    double RangeM = 0.0, BearingDeg = 0.0, ElevAngleDeg = 0.0, AzDeg = 0.0, ElDeg = 0.0;
    double ClosureMs = 0.0;
    double LastLookS = 0.0;     /* sim time of the last real detection */
    double PrevRangeM = 0.0, PrevLookS = 0.0;   /* the pair the range rate is differenced from */
    double LastIffS = -1e9;
    int    Hits = 0;
    bool   Firm = false;
    FBIffReply Iff = FBIffReply::NotInterrogated;
  };

  void ScanFrame(const fb_fdm_state &st, const FBUnitRegistry &net, double simTimeS);
  void UpdateLock(double simTimeS, bool autoAcquire);
  void DropAllTracks(const char *reason, double simTimeS);
  /* Mode-4 challenge/reply (doc/f16/datalink-iff.md): a reply is valid only from a transponder that is
   * ON and holds the SAME coalition's crypto. Everything else — enemy, friend with the box off, friend
   * with the wrong code — is one and the same NoReply. This is the only line in the class that reads a
   * unit's team, and it converts it to a two-valued answer that cannot name a hostile. */
  FBIffReply Interrogate(const FBUnit &u) const;

  Track Tracks_[kMaxRadarContacts]{};
  int TrackCount_ = 0;
  int NextTrackNum_ = 1;
  int LockedNum_ = 0;

  int SelfId_ = 0;
  FBUnitTeam SelfTeam_ = FBUnitTeam::Friendly;
  bool Powered_ = true;
  bool IffXpdr_ = true;          /* IFF Master NORM at startup (doc/f16/procedures-startup.md step 46) */
  bool IffInterrogator_ = true;
  FBRadarScanVolume Search_{};
  double NextScanS_ = 0.0;       /* the antenna's own frame grid, independent of the slot's cadence */

  /* Telemetry's view of the last Run (the loop needs numbers, not a table). -1 = nothing to report. */
  float LockNm_ = -1.0f, LockAzDeg_ = 0.0f, LockElDeg_ = 0.0f, LockClosureKt_ = 0.0f;
  float LockAgeS_ = 0.0f;
  int LockIff_ = 0;
  int ContactCount_ = 0;
};

} // namespace FlightBox
#endif
