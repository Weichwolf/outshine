/* The GROUND-THREAT catalogue: what an air-defence position IS, as data — two antennas, one weapon, an
 * envelope and a clock. The value-type-over-class decision core/FBStore.h and core/FBGun.h already make,
 * for the same reason: nine systems, one behaviour. Every number's source and confidence tier is
 * doc/modules/ground/catalogue.md; only what a number MEANS to the code is repeated here.
 *
 * The catalogue is deliberately thin about the hardware and precise about what decides a mission:
 * detection range and frame time, track range, the four envelope numbers, the reaction time and the
 * engagement channel count. Band letters, transmit power, PRF and antenna mechanics reach no code —
 * doc/modules/ground/module.md §Spec 2 argues which side of that line each parameter is on. */
#ifndef FBSITE_H
#define FBSITE_H

#include <cstring>
#include "FBDamageModel.h"
#include "FBEmitter.h"
#include "FBGun.h"
#include "FBStore.h"

namespace FlightBox {

struct FBSiteSpec {
  const char *Key = "";           /* the mission-file `module <name>` / FBModuleRegistry name */

  /* ---- the ACQUISITION set. RangeM 0 = this position has no radar at all and sees with the eye. */
  double SearchRangeM = 0.0;
  double SearchAzHalfDeg = 180.0;   /* a rotating antenna sweeps the circle */
  double SearchElCenterDeg = 0.0, SearchElHalfDeg = 0.0;
  double SearchFrameS = 6.0;        /* one antenna revolution */

  /* ---- the FIRE-CONTROL set: a pencil the engagement machine slews onto the track it is working.
   * RangeM 0 = no tracking radar (the two MANPADS rows and the optical gun). */
  double TrackRangeM = 0.0;
  double TrackAzHalfDeg = 10.0, TrackElHalfDeg = 10.0;   /* the slewed sector, not the gimbal */
  double TrackFrameS = 0.5;                              /* a fire control stares, it does not sweep */

  /* The clutter filter's half-width, applied to BOTH sets. 0 = a conical-scan/CW set with no Doppler
   * gate at all, which is the honest statement about the Fan Song class — and the reason chaff has no
   * channel against it (doc/modules/ground/module.md G4). */
  double DopplerNotchMs = 0.0;
  bool   NotchRejects = false;

  /* ---- the EYE. AzHalfDeg 0 = this position has no optical channel. Its REACH is not a number here:
   * it falls out of sensors/FBVisualSystem's own inequality, which is why a MANPADS is a two-kilometre
   * weapon by day and nothing at all at night without one line of this file saying so. */
  double OpticalAzHalfDeg = 0.0, OpticalElHalfDeg = 0.0;

  /* ---- what it shoots. Exactly one of the two is set. */
  FBStoreKind Store = FBStoreKind::None;
  FBGunKind   Gun = FBGunKind::None;
  double LaunchElevDeg = 0.0;   /* rail elevation at launch — the body pitch of the round that leaves it */

  /* ---- the ENVELOPE, the four numbers the low-altitude escape and the stand-off distance ARE. */
  double EnvMinM = 0.0, EnvMaxM = 0.0;
  double EnvAltMinM = 0.0, EnvAltMaxM = 0.0;

  /* ---- the CLOCK of an engagement. */
  double ReactionS = 0.0;       /* first firm track -> launch */
  double WarmupS = 60.0;        /* [SET] `set alert cold`: how long the sets take to come up */
  double SalvoS = 2.0;          /* [SET] between two rounds of one salvo */
  int    Channels = 0;          /* simultaneous engagements; 0 = it has no weapon */
  /* THE SALVO, and it is doctrine expressed as a number: an S-75 battalion fired two or three rounds at
   * one target. Per catalogue row rather than in the code, because it is the difference between one
   * launcher and one battery. */
  int    RoundsPerEngagement = 1;

  /* ---- THE MAGAZINE. Two numbers plus a timer, and they are what makes "run the gauntlet" an option:
   * RailCount rounds can leave without a pause, then the position is out of action for ReloadS while a
   * crew brings the next ones up, and it can do that until RoundsDefault is spent. A self-contained Osa
   * with six rounds in containers and a fixed battalion with six launchers plus six trailer reloads are
   * fundamentally different weapons, and this is where that difference lives. */
  int    RailCount = 0;         /* rounds ready to fire before a reload pause; 0 = RoundsDefault */
  double ReloadS = 0.0;         /* how long the position is out of action while the rails are refilled */
  int    RoundsDefault = 0;     /* the whole magazine (a gun: the drum) */

  FBDamageLayout Layout{};
};

namespace Sites {

/* ONE ZONE, five system ids, comparable thresholds — doc/modules/ground/module.md §Spec 8, option A.
 * The reason it is one zone and not two: a battery whose radar and rails are dead but whose van stands
 * still counts as combat-effective (CombatEffective() asks about engine, controls and structure, and a
 * position has one of the three), so SEAD and destruction would be the same word. Putting antenna, van
 * and rails in ONE zone with thresholds within a factor of 1.4 means a burst that kills the antenna
 * usually kills the position — physically defensible, because a radar van IS its own structure. It
 * MITIGATES and does not solve; the real answer is an objective kind of its own (gap G2).
 *
 * The five thresholds are [SET], anchored to the `target_soft` figures (themselves [SET] against the
 * open lethal-radius figure for a 500 lb bomb) and ordered by what the hardware is: a parabolic
 * antenna is the most fragile thing on the position, a rail and a barrel next, the van last. */
constexpr FBZoneSystem kRadarSystems[] = {
    {FBSystemId::Radar, /*Degrade*/ 0.9e3, /*Fail*/ 2.0e3},
    {FBSystemId::FireControl, /*Degrade*/ 1.0e3, /*Fail*/ 2.2e3},
    {FBSystemId::Structure, /*Degrade*/ 1.2e3, /*Fail*/ 2.8e3},
};
constexpr FBZoneSystem kSamSystems[] = {
    {FBSystemId::Radar, /*Degrade*/ 0.9e3, /*Fail*/ 2.0e3},
    {FBSystemId::FireControl, /*Degrade*/ 1.0e3, /*Fail*/ 2.2e3},
    {FBSystemId::Stores, /*Degrade*/ 1.1e3, /*Fail*/ 2.4e3},
    {FBSystemId::Structure, /*Degrade*/ 1.2e3, /*Fail*/ 2.8e3},
};
constexpr FBZoneSystem kGunSystems[] = {
    {FBSystemId::Radar, /*Degrade*/ 0.9e3, /*Fail*/ 2.0e3},
    {FBSystemId::FireControl, /*Degrade*/ 1.0e3, /*Fail*/ 2.2e3},
    {FBSystemId::Gun, /*Degrade*/ 1.1e3, /*Fail*/ 2.4e3},
    {FBSystemId::Structure, /*Degrade*/ 1.2e3, /*Fail*/ 2.8e3},
};
constexpr FBZoneSystem kManpadsSystems[] = {
    {FBSystemId::Stores, /*Degrade*/ 1.1e3, /*Fail*/ 2.4e3},
    {FBSystemId::FireControl, /*Degrade*/ 1.0e3, /*Fail*/ 2.2e3},
    {FBSystemId::Structure, /*Degrade*/ 1.2e3, /*Fail*/ 2.8e3},
};

/* The stretch is the position's half-length along its declared heading, as for a ground target: a
 * battalion earthwork is bigger than a shoulder-launched team. Presented AREA stays zero for the same
 * reason `target_soft`'s does — gun bundles resolve against aircraft only, so an area here would claim
 * a capability the projectile side does not have. */
constexpr FBDamageZoneSpec kBattalionZones[] = {{FBDamageZone::Center, -30.0, 30.0, kSamSystems, 4}};
constexpr FBDamageZoneSpec kBatteryZones[] = {{FBDamageZone::Center, -10.0, 10.0, kSamSystems, 4}};
constexpr FBDamageZoneSpec kRadarZones[] = {{FBDamageZone::Center, -12.0, 12.0, kRadarSystems, 3}};
constexpr FBDamageZoneSpec kGunZones[] = {{FBDamageZone::Center, -4.0, 4.0, kGunSystems, 4}};
constexpr FBDamageZoneSpec kManpadsZones[] = {{FBDamageZone::Center, -2.0, 2.0, kManpadsSystems, 3}};

/* The P-18: a search set, a health register and nothing else — the cheapest row in the catalogue and
 * the one seven campaigns want, because it is a radiating object that can be switched off. Behind this
 * antenna sits a telephone; it cues nobody in FlightBox, because the only legal cross-unit channel is a
 * sensor (doc/modules/ground/module.md §Knowledge 5). SearchFrameS 6.0 is the ONE sourced antenna rate
 * in the catalogue (10 rpm) and the reference every [SET] frame time below points at. */
inline constexpr FBSiteSpec kP18{
    /*Key*/ "p18",
    /*SearchRangeM*/ 250000.0, /*SearchAzHalfDeg*/ 180.0,
    /*SearchElCenterDeg*/ 5.0, /*SearchElHalfDeg*/ 10.0, /*SearchFrameS*/ 6.0,
    /*TrackRangeM*/ 0.0, /*TrackAzHalfDeg*/ 0.0, /*TrackElHalfDeg*/ 0.0, /*TrackFrameS*/ 0.5,
    /*DopplerNotchMs*/ 0.0, /*NotchRejects*/ false,
    /*OpticalAzHalfDeg*/ 0.0, /*OpticalElHalfDeg*/ 0.0,
    /*Store*/ FBStoreKind::None, /*Gun*/ FBGunKind::None, /*LaunchElevDeg*/ 0.0,
    /*EnvMinM*/ 0.0, /*EnvMaxM*/ 0.0, /*EnvAltMinM*/ 0.0, /*EnvAltMaxM*/ 0.0,
    /*ReactionS*/ 0.0, /*WarmupS*/ 60.0, /*SalvoS*/ 0.0,
    /*Channels*/ 0, /*RoundsPerEngagement*/ 0, /*RailCount*/ 0, /*ReloadS*/ 0.0,
    /*RoundsDefault*/ 0,
    FBDamageLayout{kRadarZones, 1, 0.0, 0.0, 0.0, 0.0}};

/* S-75: the fixed strategic battalion. Command guidance binds the Fan Song to target AND round to
 * impact. No Doppler notch — a conical-scan tracker had no MTI, so a beam manoeuvre does nothing to it
 * and chaff has no channel at all. */
inline constexpr FBSiteSpec kSa2{
    /*Key*/ "sa2",
    /*SearchRangeM*/ 275000.0, /*SearchAzHalfDeg*/ 180.0,
    /*SearchElCenterDeg*/ 10.0, /*SearchElHalfDeg*/ 20.0, /*SearchFrameS*/ 6.0,
    /*TrackRangeM*/ 120000.0, /*TrackAzHalfDeg*/ 10.0, /*TrackElHalfDeg*/ 10.0, /*TrackFrameS*/ 0.5,
    /*DopplerNotchMs*/ 0.0, /*NotchRejects*/ false,
    /*OpticalAzHalfDeg*/ 0.0, /*OpticalElHalfDeg*/ 0.0,
    /*Store*/ FBStoreKind::V750, /*Gun*/ FBGunKind::None, /*LaunchElevDeg*/ 80.0,
    /*EnvMinM*/ 8000.0, /*EnvMaxM*/ 30000.0, /*EnvAltMinM*/ 450.0, /*EnvAltMaxM*/ 25000.0,
    /*ReactionS*/ 30.0, /*WarmupS*/ 60.0, /*SalvoS*/ 3.0,
    /*Channels*/ 1, /*RoundsPerEngagement*/ 3, /*RailCount*/ 6, /*ReloadS*/ 300.0,
    /*RoundsDefault*/ 6,
    FBDamageLayout{kBattalionZones, 1, 0.0, 0.0, 0.0, 0.0}};

/* S-125: the low-altitude complement, and a separate row for exactly one reason — the 100 m altitude
 * floor against the S-75's 450 m is the whole difference between "go low and live" and "go low and
 * die", which is why the two were always deployed together. */
inline constexpr FBSiteSpec kSa3{
    /*Key*/ "sa3",
    /*SearchRangeM*/ 250000.0, /*SearchAzHalfDeg*/ 180.0,
    /*SearchElCenterDeg*/ 10.0, /*SearchElHalfDeg*/ 20.0, /*SearchFrameS*/ 6.0,
    /*TrackRangeM*/ 40000.0, /*TrackAzHalfDeg*/ 10.0, /*TrackElHalfDeg*/ 10.0, /*TrackFrameS*/ 0.5,
    /*DopplerNotchMs*/ 0.0, /*NotchRejects*/ false,
    /*OpticalAzHalfDeg*/ 0.0, /*OpticalElHalfDeg*/ 0.0,
    /*Store*/ FBStoreKind::V601, /*Gun*/ FBGunKind::None, /*LaunchElevDeg*/ 70.0,
    /*EnvMinM*/ 3500.0, /*EnvMaxM*/ 35000.0, /*EnvAltMinM*/ 100.0, /*EnvAltMaxM*/ 18000.0,
    /*ReactionS*/ 30.0, /*WarmupS*/ 60.0, /*SalvoS*/ 2.5,
    /*Channels*/ 1, /*RoundsPerEngagement*/ 2, /*RailCount*/ 4, /*ReloadS*/ 300.0,
    /*RoundsDefault*/ 4,
    FBDamageLayout{kBatteryZones, 1, 0.0, 0.0, 0.0, 0.0}};

/* 2K12: the mobile battery, and the one row whose weapon side FlightBox already had — the 3M9 is
 * semi-active, so breaking the illumination breaks the shot, measured before this file existed. The
 * only radar row with a Doppler notch: a CW illuminator IS a Doppler device by construction, so a beam
 * manoeuvre must work against it, and the tree's generic 40 m/s is used rather than an invented figure.
 * TrackRangeM is the ILLUMINATION range (28 km), not the acquisition range, because that is what the
 * weapon needs. Envelope + reaction time from the [T3] monograph's base variant. */
inline constexpr FBSiteSpec kSa6{
    /*Key*/ "sa6",
    /*SearchRangeM*/ 50000.0, /*SearchAzHalfDeg*/ 180.0,
    /*SearchElCenterDeg*/ 15.0, /*SearchElHalfDeg*/ 25.0, /*SearchFrameS*/ 6.0,
    /*TrackRangeM*/ 28000.0, /*TrackAzHalfDeg*/ 10.0, /*TrackElHalfDeg*/ 10.0, /*TrackFrameS*/ 0.5,
    /*DopplerNotchMs*/ 40.0, /*NotchRejects*/ false,
    /*OpticalAzHalfDeg*/ 0.0, /*OpticalElHalfDeg*/ 0.0,
    /*Store*/ FBStoreKind::M3m9, /*Gun*/ FBGunKind::None, /*LaunchElevDeg*/ 45.0,
    /*EnvMinM*/ 6000.0, /*EnvMaxM*/ 22000.0, /*EnvAltMinM*/ 100.0, /*EnvAltMaxM*/ 7000.0,
    /*ReactionS*/ 26.0, /*WarmupS*/ 60.0, /*SalvoS*/ 2.0,
    /*Channels*/ 1, /*RoundsPerEngagement*/ 1, /*RailCount*/ 3, /*ReloadS*/ 600.0,
    /*RoundsDefault*/ 3,
    FBDamageLayout{kBatteryZones, 1, 0.0, 0.0, 0.0, 0.0}};

/* 9K33: search, track, guidance and six rounds in ONE vehicle — which is why it matters even though its
 * envelope overlaps the 2K12's: one bomb ends the whole engagement capability, the exact opposite of a
 * battalion with six launchers around one van, and a genuinely different SEAD problem. The envelope is
 * [SET WITHIN the sourced band] at the earlier variant's figures, because that is what the campaigns
 * fly against; the file says so instead of averaging. */
inline constexpr FBSiteSpec kSa8{
    /*Key*/ "sa8",
    /*SearchRangeM*/ 30000.0, /*SearchAzHalfDeg*/ 180.0,
    /*SearchElCenterDeg*/ 15.0, /*SearchElHalfDeg*/ 25.0, /*SearchFrameS*/ 6.0,
    /*TrackRangeM*/ 20000.0, /*TrackAzHalfDeg*/ 10.0, /*TrackElHalfDeg*/ 10.0, /*TrackFrameS*/ 0.5,
    /*DopplerNotchMs*/ 40.0, /*NotchRejects*/ false,
    /*OpticalAzHalfDeg*/ 0.0, /*OpticalElHalfDeg*/ 0.0,
    /*Store*/ FBStoreKind::M9m33, /*Gun*/ FBGunKind::None, /*LaunchElevDeg*/ 60.0,
    /*EnvMinM*/ 1500.0, /*EnvMaxM*/ 10000.0, /*EnvAltMinM*/ 25.0, /*EnvAltMaxM*/ 5000.0,
    /*ReactionS*/ 26.0, /*WarmupS*/ 60.0, /*SalvoS*/ 1.5,
    /*Channels*/ 1, /*RoundsPerEngagement*/ 2, /*RailCount*/ 6, /*ReloadS*/ 300.0,
    /*RoundsDefault*/ 6,
    FBDamageLayout{kBatteryZones, 1, 0.0, 0.0, 0.0, 0.0}};

/* ZSU-23-4: radar-directed AAA, and the row that makes a 15 000 ft floor a DECISION rather than an
 * arbitrary altitude — its ceiling is 1 500 m and the floor is 4 572 m. TrackRangeM 8 000 [SET], well
 * outside the gun's 2.5 km reach, so the GUN and never the radar is the binding constraint. The sourced
 * 60 m ground-clutter limit is NOT modelled (that is terrain physics the tree does not have, `C4`) and
 * is quoted in the catalogue instead. */
inline constexpr FBSiteSpec kZsu23{
    /*Key*/ "zsu23",
    /*SearchRangeM*/ 20000.0, /*SearchAzHalfDeg*/ 180.0,
    /*SearchElCenterDeg*/ 40.5, /*SearchElHalfDeg*/ 44.5, /*SearchFrameS*/ 6.0,
    /*TrackRangeM*/ 8000.0, /*TrackAzHalfDeg*/ 10.0, /*TrackElHalfDeg*/ 20.0, /*TrackFrameS*/ 0.5,
    /*DopplerNotchMs*/ 0.0, /*NotchRejects*/ false,
    /*OpticalAzHalfDeg*/ 0.0, /*OpticalElHalfDeg*/ 0.0,
    /*Store*/ FBStoreKind::None, /*Gun*/ FBGunKind::Azp23, /*LaunchElevDeg*/ 0.0,
    /*EnvMinM*/ 0.0, /*EnvMaxM*/ 2500.0, /*EnvAltMinM*/ 0.0, /*EnvAltMaxM*/ 1500.0,
    /*ReactionS*/ 8.0, /*WarmupS*/ 60.0, /*SalvoS*/ 1.0,
    /*Channels*/ 1, /*RoundsPerEngagement*/ 1, /*RailCount*/ 0, /*ReloadS*/ 0.0,
    /*RoundsDefault*/ 2000,
    FBDamageLayout{kGunZones, 1, 0.0, 0.0, 0.0, 0.0}};

/* ZU-23-2: THE ROW THAT PROVES THE DESIGN. It has no radar at all, so its detection range is not a
 * number in this file — it is sensors/FBVisualSystem's own measured behaviour: 3 784 m beam-on and
 * 2 493 m head-on against an F-16, and ZERO at night. The gun therefore engages a crossing target at
 * the edge of its own reach, a head-on target barely at all, and nothing after dark. Nothing was tuned
 * to produce that. */
inline constexpr FBSiteSpec kZu23{
    /*Key*/ "zu23",
    /*SearchRangeM*/ 0.0, /*SearchAzHalfDeg*/ 0.0,
    /*SearchElCenterDeg*/ 0.0, /*SearchElHalfDeg*/ 0.0, /*SearchFrameS*/ 6.0,
    /*TrackRangeM*/ 0.0, /*TrackAzHalfDeg*/ 0.0, /*TrackElHalfDeg*/ 0.0, /*TrackFrameS*/ 0.5,
    /*DopplerNotchMs*/ 0.0, /*NotchRejects*/ false,
    /*OpticalAzHalfDeg*/ 180.0, /*OpticalElHalfDeg*/ 85.0,
    /*Store*/ FBStoreKind::None, /*Gun*/ FBGunKind::Zu23, /*LaunchElevDeg*/ 0.0,
    /*EnvMinM*/ 0.0, /*EnvMaxM*/ 2500.0, /*EnvAltMinM*/ 0.0, /*EnvAltMaxM*/ 1500.0,
    /*ReactionS*/ 8.0, /*WarmupS*/ 60.0, /*SalvoS*/ 1.0,
    /*Channels*/ 1, /*RoundsPerEngagement*/ 1, /*RailCount*/ 100, /*ReloadS*/ 10.0,
    /*RoundsDefault*/ 100,
    FBDamageLayout{kGunZones, 1, 0.0, 0.0, 0.0, 0.0}};

/* 9K32 Strela-2M: rear-aspect MANPADS. It binds nothing — free at launch, and nothing radiates, so the
 * target gets no warning of any kind (there is no missile-warning set in the tree). Its rear-aspect
 * limitation costs no code: the round's seeker reach is scaled by the infrared aspect law, so 4.2 km
 * astern is under 2 km head-on. */
inline constexpr FBSiteSpec kSa7{
    /*Key*/ "sa7",
    /*SearchRangeM*/ 0.0, /*SearchAzHalfDeg*/ 0.0,
    /*SearchElCenterDeg*/ 0.0, /*SearchElHalfDeg*/ 0.0, /*SearchFrameS*/ 6.0,
    /*TrackRangeM*/ 0.0, /*TrackAzHalfDeg*/ 0.0, /*TrackElHalfDeg*/ 0.0, /*TrackFrameS*/ 0.5,
    /*DopplerNotchMs*/ 0.0, /*NotchRejects*/ false,
    /*OpticalAzHalfDeg*/ 180.0, /*OpticalElHalfDeg*/ 85.0,
    /*Store*/ FBStoreKind::Strela2, /*Gun*/ FBGunKind::None, /*LaunchElevDeg*/ 30.0,
    /*EnvMinM*/ 800.0, /*EnvMaxM*/ 4200.0, /*EnvAltMinM*/ 50.0, /*EnvAltMaxM*/ 2300.0,
    /*ReactionS*/ 8.0, /*WarmupS*/ 30.0, /*SalvoS*/ 5.0,
    /*Channels*/ 1, /*RoundsPerEngagement*/ 1, /*RailCount*/ 1, /*ReloadS*/ 10.0,
    /*RoundsDefault*/ 2,
    FBDamageLayout{kManpadsZones, 1, 0.0, 0.0, 0.0, 0.0}};

/* 9K38 Igla: the same team with a longer round and a wider aspect window. Its documented flare
 * resistance is NOT modelled, so in FlightBox today it is a Strela with a kilometre more reach and is
 * EQUALLY flare-defeatable — a declared understatement, not an oversight. */
inline constexpr FBSiteSpec kSa18{
    /*Key*/ "sa18",
    /*SearchRangeM*/ 0.0, /*SearchAzHalfDeg*/ 0.0,
    /*SearchElCenterDeg*/ 0.0, /*SearchElHalfDeg*/ 0.0, /*SearchFrameS*/ 6.0,
    /*TrackRangeM*/ 0.0, /*TrackAzHalfDeg*/ 0.0, /*TrackElHalfDeg*/ 0.0, /*TrackFrameS*/ 0.5,
    /*DopplerNotchMs*/ 0.0, /*NotchRejects*/ false,
    /*OpticalAzHalfDeg*/ 180.0, /*OpticalElHalfDeg*/ 85.0,
    /*Store*/ FBStoreKind::Igla, /*Gun*/ FBGunKind::None, /*LaunchElevDeg*/ 30.0,
    /*EnvMinM*/ 500.0, /*EnvMaxM*/ 5200.0, /*EnvAltMinM*/ 50.0, /*EnvAltMaxM*/ 3500.0,
    /*ReactionS*/ 8.0, /*WarmupS*/ 30.0, /*SalvoS*/ 5.0,
    /*Channels*/ 1, /*RoundsPerEngagement*/ 1, /*RailCount*/ 1, /*ReloadS*/ 10.0,
    /*RoundsDefault*/ 2,
    FBDamageLayout{kManpadsZones, 1, 0.0, 0.0, 0.0, 0.0}};

} // namespace Sites

inline constexpr const FBSiteSpec *kSiteCatalogue[] = {
    &Sites::kP18, &Sites::kSa2, &Sites::kSa3, &Sites::kSa6, &Sites::kSa8,
    &Sites::kZsu23, &Sites::kZu23, &Sites::kSa7, &Sites::kSa18};

inline const FBSiteSpec *FBFindSite(const char *key) {
  if (!key) return nullptr;
  for (const FBSiteSpec *s : kSiteCatalogue)
    if (std::strcmp(s->Key, key) == 0) return s;
  return nullptr;
}

} // namespace FlightBox
#endif
