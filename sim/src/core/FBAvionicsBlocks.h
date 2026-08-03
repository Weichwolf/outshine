/* The avionics OUTPUT blocks: the typed payload of core/FBState, one block per SOURCE system.
 * THE ONE RULE: every block has exactly ONE writer — named in its comment below — and any number of
 * readers. doc/core.md, Abschnitt 1. */
#ifndef FBAVIONICSBLOCKS_H
#define FBAVIONICSBLOCKS_H

#include "FBArmState.h"
#include "FBBlockStatus.h"
#include "FBCountermeasure.h"
#include "FBDatalinkTrack.h"
#include "FBDirector.h"
#include "FBIrstContact.h"
#include "FBMfdPage.h"
#include "FBMode.h"
#include "FBRadarContact.h"
#include "FBRwrThreat.h"
#include "FBVisualContact.h"
#include <cstdint>

namespace FlightBox {

/* ---- Platform: the airframe's own pose/velocity, what everything conformal is drawn against.
 * WRITER: the owner of the FDM state (module from `st`; client re-publishes the live frame pose). */
struct FBPlatformBlock {
  FBBlockHeader H;
  float RollDeg = 0.0f, PitchDeg = 0.0f, YawDeg = 0.0f;
  float AltM = 0.0f;                 /* ASL (geodetic) */
  float EastM = 0.0f, NorthM = 0.0f; /* ENU offset from the sim origin (home) */
  float GsMs = 0.0f, TasMs = 0.0f, VsMs = 0.0f;
  float HomeDistM = 0.0f, HomeBearingDeg = 0.0f;   /* bearing relative to the nose, -180..180 */
  FBMode Mode = FBMode::Manual;      /* the REAL, confirmed guidance mode (MIL-STD-1787) */
};

/* ---- Environment: sky/weather for lighting and haze. WRITER: the client (ephemeris + live weather),
 * never a module system. An unfed weather source is Invalid, not "zero cover". */
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

/* ---- Radar altitude. WRITER: systems/FBRadarAltimeter. The reference case for Invalid: unpowered it
 * publishes no 0 ft (doc/modules/f16/controls-commands.md §6.4). */
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

/* ---- Cruise: the DED CRUS page's COMPUTED fields. WRITER: systems/FBNavSystem.
 * Own head because gear-down freezes exactly these while bearing/distance keep working — the origin of
 * FBBlockStatus::Held (doc/modules/f16/controls-commands.md, CRUS-Tabelle). */
struct FBCruiseBlock {
  FBBlockHeader H;
  float SteerTtgS = 0.0f;   /* time-to-go to the steerpoint at the current groundspeed */
};

/* ---- Fire control: FOUR products under one head (ranging, DLZ, gun solution, air-to-ground release).
 * WRITER: modules/f16/FBF16FireControl. All four go invalid together with the sources they fuse; each
 * carries its own "is there a SOLUTION" bit on top. SI throughout; only displays convert to nm.
 * doc/core.md, Abschnitt 1.3. */
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

  /* ---- The GUN solution (doc/modules/f16/weapons.md §2.5's EEGS). The funnel is a SIGHT: its walls are the
   * target's wingspan drawn at the range the gun is correctly aimed for, so the out-of-range test is
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

  /* ---- The AIR-TO-GROUND delivery solution (CCIP/CCRP). ONE integration (core/FBBallistics.h), two
   * questions: a consumer picks the mode by WHICH numbers it reads, not by putting this box in one.
   * The only geodetic doubles on this bus — at 1e-5 deg a float is a metre, the unit it is measured in. */
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
  bool   AgInRange = false;       /* release moment not gone past AND a release now would arm in time */

  /* ---- THE DIRECTOR (doc/modules/mig29/weapons.md §5.4). APPENDED, and every field is zero on an
   * aircraft that has none — which is every module here except the MiG-29, whose Ag* half above stays
   * permanently false for the same reason in reverse. The two are alternatives, never both.
   * WHAT THE ORDER OF THESE FIELDS MEANS: before consent the instrument shows a RANGE and a mark
   * error; after consent the range scale IS REPLACED by a time scale and a commanded load factor.
   * A consumer that reads a time before `DirArmed` is reading a number the pilot cannot see. */
  uint8_t DirState = 0;             /* FBDirectorState ordinal */
  uint8_t DirRefusal = 0;           /* FBDirectorRefusal ordinal of the LAST refused consent */
  bool   DirEngaged = false;        /* an unguided store is selected: the A-G branch is live */
  bool   DirRanged = false;         /* a slant range exists and is inside the device's reach */
  float  DirRangeM = 0.0f;          /* what it measured — the pre-consent scale */
  float  DirRangeAgeS = 0.0f;       /* how old it is: the documented 1..10 s consent window runs on it */
  float  DirMarkErrM = 0.0f;        /* + = the aiming mark falls SHORT of the target, along the track */
  bool   DirArmed = false;          /* consent held AND a plan committed to */
  float  DirTimeToReleaseS = 0.0f;  /* the FROZEN countdown — never re-solved, which is the whole point */
  float  DirCmdG = 0.0f;            /* the RING: the normal load factor the plan requires */
  float  DirCurG = 0.0f;            /* the VECTOR: what the aircraft is actually pulling */
  bool   DirAudio = false;          /* the documented 1.5-3 s pre-release tone */
};

/* ---- UFC/DED entered values: what the pilot typed into the control head and the jet committed.
 * WRITER: modules/f16/FBF16Ufc (the one system that owns committed DED field state). */
struct FBUfcBlock {
  FBBlockHeader H;
  float AlowFt = 0.0f;         /* CARA ALOW floor */
  /* Two numbers because the jet keeps two (doc/modules/f16/controls-commands.md §6.8): the DED shows what was
   * typed, the warning fires at the system ceiling. */
  float BingoLbs = 0.0f;       /* what was entered (0 = none) */
  float BingoEffectiveLbs = 0.0f;   /* what actually governs the warning */
  int   SteerNum = 0;       /* selected steerpoint number */
};

/* ---- Stores/SMS: the loadout, station by station. WRITER: systems/FBStoresSystem (F-16 slot:
 * modules/f16/FBF16Sms). `Station[i]` = FBStoreKind ordinal on station i+1, 0 = empty. */
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
  /* Whether this jet is holding a spot on the point it released a laser-guided round against — the
   * shooter's OWN instrument reading of its own obligation, and the one thing that tells its pilot the
   * run is not over yet. False for every jet that never released one. doc/air-to-ground.md §3.2. */
  bool  Designating = false;
};

/* ---- Gun: the internal gun's books. WRITER: systems/FBGunSystem (F-16 slot: modules/f16/FBF16Gun).
 * Own block, not stores fields: the SMS knows pylons, and a gun hangs on nothing. */
struct FBGunBlock {
  FBBlockHeader H;
  FBArmState Arm = FBArmState::Sim;
  uint8_t Kind = 0;
  int   RoundsRemaining = 0;
  int   RoundsFired = 0;        /* this sortie */
  bool  Firing = false;         /* a squeeze is running right now */
  bool  Ready = false;          /* a gun is fitted, armed and has rounds — what a pilot checks */
};

/* ---- Airframe/propulsion: the readbacks OTHER systems regulate against (gear drives the cruise
 * freeze, fuel drives BINGO). WRITER: systems/FBAirframeControls. */
struct FBAirframeBlock {
  FBBlockHeader H;
  float GearPosition = 0.0f;     /* 0 = up .. 1 = down, kinematic-lagged */
  bool  WeightOnWheels = false;
  float SpeedbrakeNorm = 0.0f;
  float FuelLbs = 0.0f;
  float FuelPct = 0.0f;          /* 0..100 of declared capacity */
  bool  EngineRunning = false;
};

/* ---- Warnings: the caution/warning set as a BITMASK. WRITER: systems/FBWarningSystem. */
enum FBWarningBit : uint32_t {
  FBWarnAlow = 1u << 0,        /* below the CARA ALOW floor */
  FBWarnBingo = 1u << 1,       /* fuel at or below the committed BNGO threshold */
  FBWarnGearUnsafe = 1u << 2,  /* on the wheels with the gear not down-and-locked */
};

struct FBWarningBlock {
  FBBlockHeader H;
  uint32_t Active = 0;            /* OR of FBWarningBit */
  uint32_t Inhibited = 0;         /* source block Invalid: cannot be evaluated — not "not warning" */
};

/* ---- Radar (FCR): the ACTIVE sensor picture. WRITER: systems/FBRadarSystem.
 * Held is the steady state, not an exception: between antenna frames only the contact age moves. */
struct FBRadarBlock {
  FBBlockHeader H;
  bool Radiating = false;      /* powered AND the active mode radiates */
  int  ModeOrdinal = 0;        /* the module's own mode label, not logic */
  int  ContactCount = 0;
  int  LockIndex = -1;         /* index into Contacts of the STT track; -1 = no lock */
  bool IffTransponder = false; /* own transponder answering — what other interrogators get back */
  /* HOW WIDE THE SET IS ACTUALLY LOOKING, off the nose. Without it a scope has to guess its own
   * azimuth scale, and every contact then sits at the wrong place on it. */
  float ScanAzHalfDeg = 60.0f;
  FBRadarContact Contacts[kMaxRadarContacts]{};
};

/* ---- Datalink: the cooperative net picture. WRITER: systems/FBDatalinkSystem.
 * Held between net cycles, as the radar is between frames. */
struct FBDatalinkBlock {
  FBBlockHeader H;
  bool Powered = false;
  bool Transmitting = false;
  int  TrackCount = 0;
  FBDatalinkTrack Tracks[kMaxDatalinkTracks]{};
};

/* ---- RWR: WHO IS LOOKING AT THIS AIRCRAFT — the passive counterpart of the radar block.
 * WRITER: systems/FBRwrSystem. Held as above, while a fading threat is carried before it is dropped. */
struct FBRwrBlock {
  FBBlockHeader H;
  bool Powered = false;
  int  ThreatCount = 0;
  int  PriorityIndex = -1;     /* index of the highest-priority threat (the diamond); -1 = none */
  bool MissileLaunch = false;  /* any threat guiding a missile at this aircraft — the LAUNCH light */
  bool Activity = false;       /* any threat tracking — the ACT half of the ACT/PWR indicator */
  bool HiddenSearch = false;   /* heard but filtered off the display — suppression is distinguishable
                                * from absence (doc/modules/f16/defence-rwr-cm.md §2.1) */
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

/* ---- BFM trackfile: the fused target estimate the fight is flown on. WRITER: systems/FBBfmTrack.
 * The head carries the fusion's three states verbatim: Invalid = never seen, Valid = fresh or inside
 * the credible extrapolation window, Held = past it (last MEASURED position, nothing to lead on).
 * StampS is the LOOK the estimate stands on, not the publication time. */
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

/* ---- IRST: the PASSIVE infrared picture. WRITER: sensors/FBIrstSystem.
 * Held between scan frames like the radar's, Invalid when the head is caged or unpowered. The block
 * carries no identity and no range (except the laser's, per contact) — the austerity is the model, and
 * doc/sensors.md §6 argues it. APPENDED LAST, which is the telemetry column rule made structural: the
 * block's own status column is declared by its own system, not by FBStateBusTelemetry's list. */
struct FBIrstBlock {
  FBBlockHeader H;
  bool Powered = false;
  bool Searching = false;       /* the head is uncaged and looking */
  int  ModeOrdinal = 0;
  int  ContactCount = 0;
  int  LockIndex = -1;          /* index into Contacts of the tracked source; -1 = none */
  bool LaserArmed = false;      /* the rangefinder is ON — the one ACTIVE thing this sensor can do */
  int  CloudMaskedCount = 0;    /* sources rejected this frame because a deck stood in the line of sight */
  FBIrstContact Contacts[kMaxIrstContacts]{};
};

/* ---- VISUAL: what the pilot's own eyes have. WRITER: sensors/FBVisualSystem.
 * The austerity of the contact type (no id, no team, no range, no closure) is doc/sensors.md §9's whole
 * argument; the block adds only what the CHANNEL as a whole measured. It is the one block whose writer
 * READS another: FBEnvironmentBlock's sun angles decide whether an eye works at all and how badly it is
 * dazzled — a documented cross-block read, not accidental coupling, and it is one-way.
 * APPENDED LAST, same column rule as the IRST block above it. */
struct FBVisualBlock {
  FBBlockHeader H;
  bool  Powered = false;
  int   ContactCount = 0;
  int   CloudMaskedCount = 0;   /* contacts rejected this frame by the cloud transmittance */
  float GlareFactor = 1.0f;     /* the sun's contrast factor at the WORST-placed contact; 1 = no glare */
  FBVisualContact Contacts[kMaxVisualContacts]{};
};

/* ---- NETLINK: what a GROUND CONTROLLER told this aircraft to look at. WRITER:
 * sensors/FBNetLinkSystem, a thin derivation of FBDatalinkSystem that publishes HERE instead of into
 * FBDatalinkBlock. Same type, different block, and the reason is the whole of it: a block has one
 * writer, the Datalink block already carries the cooperative flight's Link-16 PPLI, and a controller
 * feed written into it would overwrite the picture doc/formation.md's station keeping, sort and cover
 * deferral all read. What arrives here is a POINT with an age and no identity (core/FBNetReport.h) —
 * it moves an antenna and it is never a track. doc/modules/air/module.md §Spec 7.
 * APPENDED LAST, same column rule as the two blocks above it. */
using FBNetLinkBlock = FBDatalinkBlock;

/* ---- GROUND MAP: the radar's air-to-ground picture as a RASTER of BACKSCATTER, not of terrain.
 * WRITER: sensors/FBGroundMap, driven by sensors/FBRadarSystem. A cell carries how much energy came
 * back out of that resolution cell (0 = nothing, incl. everything the beam never reached because a
 * nearer crest stood in the way) — a display that coloured it by height would be drawing a map the
 * aircraft has no instrument for. The block is aircraft-relative and NOSE-UP by construction: column 0
 * is the left sector edge, row 0 the nearest range bin.
 * APPENDED LAST, same telemetry-column rule as the blocks above it. */
inline constexpr int kGroundMapAz = 64;      /* beam positions across the scanned sector */
inline constexpr int kGroundMapRange = 48;   /* range bins per beam */

struct FBGroundMapBlock {
  FBBlockHeader H;
  bool  Mapping = false;       /* the set is in a ground-mapping mode right now */
  float AzHalfDeg = 60.0f;     /* the scanned sector, +/- off the nose */
  float RangeM = 0.0f;         /* ground range of the outermost bin */
  float SweepFrac = 0.0f;      /* 0..1 of the current sweep already painted (left to right) */
  float GroundAslM = 0.0f;     /* terrain height under the aircraft, the map's own altitude reference */
  uint8_t Cell[kGroundMapRange][kGroundMapAz]{};
};

/* ---- MFD BANK: which page stands on which bay, and which pages can be chosen AT ALL right now.
 * WRITER: systems/FBMfdSystem. The block exists so that the thing a display shows is itself a
 * published reading rather than a fact the renderer keeps on the side — the same rule every other
 * symbol on the screen obeys.
 * `Pages` is the MODULE's catalogue and is fixed for the aircraft's life, so an ordinal means the same
 * page for the whole sortie (that is what makes it a command VALUE). `Available` is the LOADOUT and
 * block-validity cut over it and changes as the sortie does: a jet that has dropped its last bomb can
 * no longer choose its stores page. APPENDED LAST, same telemetry-column rule as the three above. */
struct FBMfdBlock {
  FBBlockHeader H;
  int       PageCount = 0;
  FBMfdPage Pages[kMfdMaxPages]{};
  uint32_t  Available = 0;         /* bit i = ordinal i is selectable right now */
  int       Bay[kMfdBays] = {-1, -1, -1};   /* the ordinal on each bay; -1 = dark */
  int       LastSelectPage = -1;   /* the ordinal the last COMMITTED select put on the attention bay */
  double    LastSelectS = -1.0;    /* ...and when the box committed it: the "when" a viewer reads */

  bool Selectable(int ord) const {
    return ord >= 0 && ord < PageCount && (Available & (1u << ord)) != 0;
  }
};

} // namespace FlightBox
#endif
