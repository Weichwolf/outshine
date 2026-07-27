/* FlightBox — the avionics OUTPUT blocks: the typed payload of core/FBState, one block per SOURCE
 * system, each with an FBBlockHeader (core/FBBlockStatus.h) saying whether its numbers currently mean
 * anything. This file IS the bus layout, which is why the blocks live together in it rather than one
 * per file: a block is a message definition, not a class — no behaviour, no invariants of its own, and
 * a reader needs to see the whole set to know what the bus carries.
 *
 * THE ONE RULE: every block has exactly ONE writer (named in its comment) and any number of readers.
 * A reader never writes a block, and no block is written by two systems — that is what makes "who put
 * this number here" answerable at compile time instead of by grep, and it is the property the flat
 * FBState had already lost (a growing struct where the fire-control system read a field the App wrote
 * and nobody could see it).
 *
 * Semantics, not addresses (MIL-STD-1553 as the model): defined data groups with a validity head and a
 * known producer. Deliberately NOT taken over: bus addressing, word packing, message scheduling —
 * FlightBox's transport is a typed struct passed by reference in one address space, and inventing
 * remote-terminal addresses for it would be cargo cult. */
#ifndef FBAVIONICSBLOCKS_H
#define FBAVIONICSBLOCKS_H

#include "FBArmState.h"
#include "FBBlockStatus.h"
#include "FBCountermeasure.h"
#include "FBDatalinkTrack.h"
#include "FBMode.h"
#include "FBRadarContact.h"
#include "FBRwrThreat.h"
#include <cstdint>

namespace FlightBox {

/* ---- Platform: the airframe's own pose/velocity, the block everything conformal is drawn against.
 * WRITER: the owner of the FDM state — the module publishes it from the `st` it is handed each Run(),
 * and the client re-publishes it with this frame's live pose before handing the bus to the renderer
 * (units/FBSimUnit::HudState). One writer per bus instance; the module's bus and the client's frame
 * copy are the same block filled from the same source. */
struct FBPlatformBlock {
  FBBlockHeader H;
  float RollDeg = 0.0f, PitchDeg = 0.0f, YawDeg = 0.0f;
  float AltM = 0.0f;                 /* ASL (geodetic) */
  float EastM = 0.0f, NorthM = 0.0f; /* ENU offset from the sim origin (home) */
  float GsMs = 0.0f, TasMs = 0.0f, VsMs = 0.0f;
  float HomeDistM = 0.0f, HomeBearingDeg = 0.0f;   /* bearing relative to the nose, -180..180 */
  FBMode Mode = FBMode::Manual;      /* the REAL, confirmed guidance mode (MIL-STD-1787) */
};

/* ---- Environment: sky/weather inputs the renderer's lighting and the HUD's haze read.
 * WRITER: the client (ephemeris + live weather), never a module system. Not avionics in the airframe
 * sense, but it is shared per-frame state with exactly the same producer/consumer question, so it
 * carries the same head — an unfed weather source is Invalid, not "zero cover". */
struct FBEnvironmentBlock {
  FBBlockHeader H;
  float CloudCover = 0.0f;                                     /* 0..1 total (HUD/haze legacy total) */
  float CloudLow = 0.0f, CloudMid = 0.0f, CloudHigh = 0.0f;    /* 0..1 layer cover -> volumetric mix */
  float CloudBaseAglM = 0.0f;                                  /* 0 = unknown -> shader default */
  float SunElDeg = 0.0f, SunAzDeg = 0.0f;                      /* + = above horizon, az 0=N 90=E */
  float MoonElDeg = 0.0f, MoonAzDeg = 0.0f, MoonPhase = 0.0f;  /* phase = illuminated fraction */
};

/* ---- Air data (ADC class). WRITER: systems/FBAirDataSystem. */
struct FBAirDataBlock {
  FBBlockHeader H;
  float CasKt = 0.0f, Mach = 0.0f;
  float GLoad = 0.0f, GLoadPeak = 0.0f;   /* body normal load factor, running peak since boot */
  float TrackDeg = 0.0f;                  /* ground track, true, 0..360 (velocity vector) */
  float FpaDeg = 0.0f;                    /* flight-path angle, + = climbing (the FPM's elevation) */
};

/* ---- Radar altitude. WRITER: systems/FBRadarAltimeter.
 * The block that MUST be able to say Invalid: the CARA is a powered box, and doc/f16/
 * controls-commands.md §6.4 documents the consequence literally — the ALOW warning only fires with the
 * radar altimeter powered and transmitting, however happily the DED accepted the threshold. */
struct FBRadarAltBlock {
  FBBlockHeader H;
  float AglFt = 0.0f;
};

/* ---- Navigation: the active steerpoint + the bullseye reference. WRITER: systems/FBNavSystem. */
struct FBNavBlock {
  FBBlockHeader H;
  float SteerBearingDeg = 0.0f;     /* true bearing aircraft -> steerpoint, 0..360 */
  float SteerElevAngleDeg = 0.0f;   /* elevation angle to it, + = above */
  float SteerDistNm = 0.0f;         /* horizontal distance */
  float SteerElevFt = 0.0f;         /* the steerpoint's own ground elevation, ft ASL */
  float BullBearingDeg = 0.0f;      /* bearing FROM the bullseye TO the aircraft, 0..360 */
  float BullDistNm = 0.0f;
  float MagVarDeg = 0.0f;           /* magnetic variation (placeholder: 0) */
};

/* ---- Cruise: the DED CRUS page's COMPUTED fields, split off the nav block for one documented reason
 * (doc/f16/controls-commands.md, CRUS table): with the gear down these stop updating and freeze at
 * their last value while bearing/distance keep working. That is a per-MESSAGE property, so it needs its
 * own head — the freeze is expressed as FBBlockStatus::Held, which is where the third state earns its
 * existence. WRITER: systems/FBNavSystem (a source system may publish more than one message). */
struct FBCruiseBlock {
  FBBlockHeader H;
  float SteerTtgS = 0.0f;   /* time-to-go to the steerpoint at the current groundspeed */
};

/* ---- Fire control: the 'B' (baro/steerpoint-elevation) slant-range method, PLUS the air-to-air launch
 * zone for the selected weapon. WRITER: modules/f16/FBF16FireControl.
 *
 * The DLZ half is the block's second message in all but name (doc/f16/weapons.md §2.5's DLZ table:
 * Raero, Rtr, the activation range and the two post-launch countdowns). It shares the head with the
 * slant range because both are the same box's output and both go invalid together when the sources they
 * fuse do; DlzValid says whether there is a SOLUTION on top of that — a fire control with no locked
 * target publishes its block and reports no launch zone, which is a different fact from an unpublished
 * block. Every field is metres/seconds (the bus is SI; only displays convert to nm). */
struct FBFireControlBlock {
  FBBlockHeader H;
  float SteerSlantNm = 0.0f;
  char  RangeProvider = 'B';   /* range-provider letter shown next to the number */

  bool  DlzValid = false;      /* a locked target AND a weapon selected that has a launch zone */
  float TargetRangeM = 0.0f;   /* the locked target's slant range — what the limits below bracket */
  float ClosureMs = 0.0f;      /* + = closing */
  float RaeroM = 0.0f;         /* max kinematic range against a NON-manoeuvring target */
  float RtrM = 0.0f;           /* turn-and-run: a hit even if the target reverses at launch */
  float RminM = 0.0f;          /* closest range that still allows arm + terminal homing */
  float TimeToActiveS = 0.0f;  /* predicted seconds from launch to the seeker going active */
  float TimeToImpactS = 0.0f;  /* predicted total time of flight; < 0 = no intercept from here */
  bool  InZone = false;        /* Rmin <= range <= Raero — what the SMS's launch interlock reads */

  /* ---- THE GUN SOLUTION (doc/f16/weapons.md §2.5's EEGS), the third product of the same box. It is
   * published under the same head for the same reason the launch zone is: it is a fusion of the target
   * estimate and own motion, and it goes invalid when they do. GunValid says whether there is a
   * SOLUTION, exactly as DlzValid does — a fire control with nothing to shoot at still publishes.
   *
   * The EEGS funnel is a SIGHT: its walls are the target's known wingspan drawn at the range for which
   * the gun is correctly aimed, so a target that fills the funnel is at the range the lead was computed
   * for. What that geometry says about a SHOT is three numbers, and they are what a pilot (or an AI)
   * actually needs: how far the required lead is from where the nose is pointing right now
   * (GunAimErrorDeg), how big the target is in angle at this range (GunSpanMr), and the two spans that
   * bracket the funnel's own range window (GunFunnelTopMr at its minimum range, GunFunnelBottomMr at its
   * maximum). "Target smaller than the funnel bottom" — the guide's own out-of-range test — is then
   * literally GunSpanMr < GunFunnelBottomMr. */
  bool  GunValid = false;
  float GunRangeM = 0.0f;        /* to the predicted intercept point, not to the target now */
  float GunTofS = 0.0f;          /* projectile time of flight to it */
  float GunAimErrorDeg = 0.0f;   /* angle between the required bore direction and the aircraft's nose */
  float GunLeadAzDeg = 0.0f;     /* the required bore, body-referenced: + = right */
  float GunLeadElDeg = 0.0f;     /* ...+ = above */
  float GunSpreadM = 0.0f;       /* the round pattern's sigma at the intercept point */
  float GunSpanMr = 0.0f;        /* the target's angular span there, milliradians */
  float GunFunnelTopMr = 0.0f;   /* its span at the funnel's minimum range (the funnel's narrow end) */
  float GunFunnelBottomMr = 0.0f;/* ...and at its maximum range */
  float GunTolDeg = 0.0f;        /* the funnel's own aiming tolerance at this range (see GunInFunnel) */
  bool  GunInRange = false;      /* inside the funnel's own range window */
  bool  GunInFunnel = false;     /* in range AND the nose is inside the lead solution's own tolerance */

  /* ---- THE AIR-TO-GROUND DELIVERY SOLUTION (doc/f16/weapons.md §2.5's CCIP/CCRP), the FOURTH product
   * of the same box, under the same head and for the same reason as the two above: it fuses the nav and
   * platform blocks and goes invalid when they do. AgValid says whether there is a SOLUTION — an
   * aircraft with no unguided store selected still publishes its fire-control block.
   *
   * ONE integration (core/FBBallistics.h), two questions. The IMPACT POINT is the CCIP pipper: where the
   * selected round lands if the pickle is pressed now. The three ERRORS are that point measured against
   * the designated aim point along the current ground track, which is what CCRP's Solution Cue counts
   * down. A consumer picks the mode by which numbers it reads, not by asking this box to be in one.
   *
   * The impact point is the one place on this bus carrying geodetic DOUBLES: at 1e-5 deg a float is a
   * metre, and a metre is the quantity the whole solution is measured in. */
  bool   AgValid = false;
  double AgImpactLatDeg = 0.0, AgImpactLonDeg = 0.0;
  float  AgImpactElevM = 0.0f;    /* the plane it was solved against, m ASL (the ranging source's) */
  float  AgTofS = 0.0f;           /* time of fall from a release now */
  float  AgRangeM = 0.0f;         /* horizontal aircraft -> impact point: the bomb's own range */
  float  AgAlongErrM = 0.0f;      /* + = the round falls SHORT of the aim point; 0 = release now */
  float  AgCrossErrM = 0.0f;      /* + = it falls RIGHT of it — the steering-line error */
  float  AgMissM = 0.0f;          /* the two combined: the pipper's distance from the aim point */
  float  AgTimeToReleaseS = 0.0f; /* AgAlongErrM at the current groundspeed; <= 0 = the cue has passed */
  float  AgArmMarginS = 0.0f;     /* fall time left once the fuze's arming delay has run; < 0 = a dud */
  /* THE IN-RANGE CUE: the aim point can still be hit from here — it is beyond the current impact point
   * (the release moment has not gone past) AND a release now would arm before arrival. Both halves are
   * the guide's own two conditions for a valid delivery, expressed as the one bit a decision reads. */
  bool   AgInRange = false;
};

/* ---- UFC/DED entered values: what the pilot typed into the control head and the jet committed.
 * WRITER: modules/f16/FBF16Ufc (the one system that owns committed DED field state). */
struct FBUfcBlock {
  FBBlockHeader H;
  float AlowFt = 0.0f;         /* CARA ALOW floor */
  /* Two numbers because the jet keeps two (doc/f16/controls-commands.md §6.8): the DED field shows what
   * the pilot TYPED, the warning fires at the system ceiling. Displays read the first, the warning
   * system reads the second — collapsing them would have made the documented clamp invisible. */
  float BingoLbs = 0.0f;       /* what was entered (0 = none) */
  float BingoEffectiveLbs = 0.0f;   /* what actually governs the warning */
  int   SteerNum = 0;       /* selected steerpoint number */
};

/* ---- Stores/SMS: the loadout, station by station. WRITER: systems/FBStoresSystem (the F-16 fills the
 * slot with modules/f16/FBF16Sms, which only adds this airframe's pylon geometry).
 * `Station[i]` carries the FBStoreKind ordinal on station i+1 — 0 = empty — rather than a pointer or a
 * name, for the same reason every other block carries plain numbers: a bus message is data, and the
 * catalogue behind the ordinal (core/FBStore.h) is a compile-time table both writer and reader share. */
constexpr int kMaxStoreStations = 12;

struct FBStoresBlock {
  FBBlockHeader H;
  FBArmState Arm = FBArmState::Arm;
  int   StationCount = 0;
  int   SelectedStation = -1;      /* 1-based station number; -1 = nothing selected */
  uint8_t Station[kMaxStoreStations]{};
  int   LoadedCount = 0;
  float LoadedLbs = 0.0f;          /* total carried store weight */
  int   ReleasedCount = 0;         /* stores this jet has let go of this sortie */
};

/* ---- Gun: the internal gun's books. WRITER: systems/FBGunSystem (the F-16 fills the slot with
 * modules/f16/FBF16Gun, which only adds this airframe's gun and where it is bolted).
 * Its own block rather than fields on the stores block above for the reason the two systems are
 * separate: the SMS knows about pylons and what hangs on them, and a gun hangs on nothing. `Kind` is the
 * FBGunKind ordinal (0 = no gun fitted), the same convention the stores block uses for its stations. */
struct FBGunBlock {
  FBBlockHeader H;
  FBArmState Arm = FBArmState::Sim;
  uint8_t Kind = 0;
  int   RoundsRemaining = 0;
  int   RoundsFired = 0;        /* this sortie */
  bool  Firing = false;         /* a squeeze is running right now */
  bool  Ready = false;          /* a gun is fitted, armed and has rounds — what a pilot checks */
};

/* ---- Airframe/propulsion: gear + weight-on-wheels + the fuel state, the readbacks OTHER systems
 * regulate against (the gear signal drives the cruise freeze above; the fuel state drives BINGO).
 * WRITER: systems/FBAirframeControls (through the module, which owns the FDM handle the fuel totals
 * come from — the same object every gear/brake command already goes through). */
struct FBAirframeBlock {
  FBBlockHeader H;
  float GearPosition = 0.0f;     /* 0 = up .. 1 = down, kinematic-lagged */
  bool  WeightOnWheels = false;
  float SpeedbrakeNorm = 0.0f;
  float FuelLbs = 0.0f;
  float FuelPct = 0.0f;          /* 0..100 of declared capacity */
  bool  EngineRunning = false;
};

/* ---- Warnings: the caution/warning set, a BITMASK so one block carries the whole annunciator panel
 * without growing a field per lamp. WRITER: systems/FBWarningSystem. */
enum FBWarningBit : uint32_t {
  FBWarnAlow = 1u << 0,        /* below the CARA ALOW floor */
  FBWarnBingo = 1u << 1,       /* fuel at or below the committed BNGO threshold */
  FBWarnGearUnsafe = 1u << 2,  /* on the wheels with the gear not down-and-locked */
};

struct FBWarningBlock {
  FBBlockHeader H;
  uint32_t Active = 0;            /* OR of FBWarningBit */
  uint32_t Inhibited = 0;         /* conditions whose SOURCE block is Invalid — the warning cannot be
                                   * evaluated, which is a different fact from "not warning" */
};

/* ---- Radar (FCR): the ACTIVE sensor picture. WRITER: systems/FBRadarSystem.
 * Held is the normal steady state here, not an exception: between antenna frames the geometry stands
 * still and only the per-contact age moves (systems/FBRadarSystem's banner) — exactly freeze-at-last-
 * value, so the head says Held until the next completed sweep republishes it. */
struct FBRadarBlock {
  FBBlockHeader H;
  bool Radiating = false;      /* powered AND the active mode radiates */
  int  ModeOrdinal = 0;        /* the module's own mode label, not logic */
  int  ContactCount = 0;
  int  LockIndex = -1;         /* index into Contacts of the STT track; -1 = no lock */
  bool IffTransponder = false; /* own transponder answering — what other interrogators get back */
  FBRadarContact Contacts[kMaxRadarContacts]{};
};

/* ---- Datalink: the cooperative net picture. WRITER: systems/FBDatalinkSystem.
 * Same Held story as the radar, arrived at from the opposite direction: the net refreshes once per
 * cycle and the picture is frozen in between. */
struct FBDatalinkBlock {
  FBBlockHeader H;
  bool Powered = false;
  bool Transmitting = false;
  int  TrackCount = 0;
  FBDatalinkTrack Tracks[kMaxDatalinkTracks]{};
};

/* ---- RWR: WHO IS LOOKING AT THIS AIRCRAFT — the passive counterpart of the radar block above.
 * WRITER: systems/FBRwrSystem.
 * Held has the same meaning it has for the radar and arrives the same way: a threat whose emission
 * stopped being heard is carried for a moment before it is dropped, and while nothing new is being
 * received the picture stands still and only the per-threat age moves. */
struct FBRwrBlock {
  FBBlockHeader H;
  bool Powered = false;
  int  ThreatCount = 0;
  int  PriorityIndex = -1;     /* index of the highest-priority threat (the diamond); -1 = none */
  bool MissileLaunch = false;  /* any threat guiding a missile at this aircraft — the LAUNCH light */
  bool Activity = false;       /* any threat tracking — the ACT half of the ACT/PWR indicator */
  bool HiddenSearch = false;   /* search-class emitters are being heard but filtered off the display
                                * (doc/f16/defence-rwr-cm.md §2.1: suppression is visually
                                * distinguishable from absence) */
  FBRwrThreat Threats[kMaxRwrThreats]{};
};

/* ---- CMDS: the countermeasure dispenser's own state. WRITER: systems/FBCountermeasureSystem. */
struct FBCmdsBlock {
  FBBlockHeader H;
  FBCmdsMode   Mode = FBCmdsMode::Off;
  FBCmdsStatus Status = FBCmdsStatus::NoGo;
  int  ProgramSelected = 1;    /* the PRGM knob */
  int  ChaffRemaining = 0, FlareRemaining = 0;
  bool ChaffLow = false, FlareLow = false;   /* at or below the pilot-set BINGO quantity: the "LO" lamp */
  bool Dispensing = false;     /* a program is running right now */
  int  ChaffDispensed = 0, FlareDispensed = 0;   /* this sortie */
  int  ActiveClouds = 0;       /* chaff clouds still worth something (core/FBCountermeasure.h) */
};

/* ---- BFM trackfile: the fused target estimate the fight is flown on. WRITER: systems/FBBfmTrack
 * (published onto the bus by the module after the pilot's decision tick, the same way the platform
 * block is published from `st`).
 * The head carries the fusion's own three states verbatim: Invalid = never seen; Valid = fresh or
 * within the credible extrapolation window; Held = past it, where the estimate reverts to the last
 * MEASURED position and stops being something to lead on. StampS is the LOOK the estimate stands on,
 * not the publication time — age is age since the sensor last actually saw him. */
struct FBBfmBlock {
  FBBlockHeader H;
  bool   Locked = false;      /* the radar is holding this target RIGHT NOW (STT) */
  double RangeM = 0.0;
  double AzDeg = 0.0, ElDeg = 0.0;      /* body-referenced to the estimate (ATA) */
  double ClosureMs = 0.0;               /* + = closing */
  double AspectDeg = 0.0;               /* at the target: 0 = we are on his tail, 180 = head-on */
  double HcaDeg = 0.0;                  /* heading crossing angle */
  double EastM = 0.0, NorthM = 0.0, UpM = 0.0;   /* estimated offset from own position */
  double VelE = 0.0, VelN = 0.0, VelU = 0.0;     /* estimated target velocity (ENU, m/s) */
};

} // namespace FlightBox
#endif
