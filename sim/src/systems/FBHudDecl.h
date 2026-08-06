/* A HUD AS DATA. The vocabulary a `.fbh` may use and the resolution of that vocabulary against the
 * bus — no drawing (systems/FBDeclaredHud) and no file access (missions/FBHudBoot).
 *
 * The three enums below ARE the contract with a mod: a name that is not in one of them does not
 * exist, and the parser says so with a line number instead of dropping the row. Names are resolved
 * ONCE, at parse time, so a frame never compares a string. Format: doc/render/hud-declaration.md. */
#ifndef FBHUDDECL_H
#define FBHUDDECL_H

#include <cstdint>
#include <string>
#include <vector>

#include "FBState.h"

namespace FlightBox::Systems {

struct FBHudEnv;

/* WHAT A SPECTATOR MAY BE TOLD, and it is deliberately not FBState: nobody is in the seat, so the
 * facts are the ones a broadcast knows — who is on screen, why the camera cut there, who just died.
 * The client fills it from its own cast copy and its director; core writes none of it. */
struct FBHudWatch {
  const char *Title = "";        /* the mod's own name */
  const char *Mission = "";
  const char *Subject = "";      /* the unit the cut is on */
  const char *SubjectTeam = "";
  const char *SubjectKind = "";
  const char *Shot = "";         /* why the camera is there: home/takeoff/launch/landing/impact/wreck */
  const char *Event = "";        /* the last unit to be destroyed, and its side */
  const char *EventTeam = "";
  float EventAgeS = -1.0f;       /* < 0 = nothing has been destroyed yet */
  float Friendly = 0.0f, Hostile = 0.0f;   /* aircraft still flying per side */
  float SimT = 0.0f;
  bool Held = false;             /* the watcher took the cutting off the director */
};

enum class FBHudKind : uint8_t {
  Text, Line, Box, Circle, Bar, Cross, Rose, Scope, Vector,
  Compass, Tape, Ladder, Horizon, Ils,
  Fpm, Contacts, Ccip, Funnel, Dlz,
};

/* A NUMBER A DECLARATION MAY NAME. Every entry is a published block field or an arithmetic
 * conversion of one — never a new measurement. */
enum class FBHudNum : uint8_t {
  None,
  CasKt, Mach, TasKt, GsKt, GsMs,
  AltFt, AltM, AglFt, AglM, VsFpm, VsMs,
  Heading, Track, Fpa, Pitch, Roll, GLoad, GPeak,
  FuelLb, FuelPct, Gear, Speedbrake,
  GunRounds, GunFired, Station, StoresLoaded, StoresReleased, Chaff, Flare,
  SteerNum, SteerBrg, SteerRelBrg, SteerDistNm, SteerTtgS, SteerElDeg,
  HomeDistNm, HomeRelBrg,
  Contacts, TgtRangeNm, TgtClosureKt, TgtTtiS, TgtAz, TgtEl, TgtAspect,
  RminNm, RtrNm, RaeroNm,
  AgRangeNm, AgTtrS, AgMissM,
  GunLeadAz, GunLeadEl, GunSpanMr,
  WeaponCount,
  SunElDeg, CloudCover, SimTimeS,
  /* the watch feed */
  WatchFriendly, WatchHostile, WatchEventAgeS, WatchSimT,
};

/* A STRING A DECLARATION MAY NAME. Short, because a HUD that prints prose is a HUD with a message
 * system, and this tree has none (doc/render/hud-declaration.md §Gaps). */
enum class FBHudStr : uint8_t {
  None, Mode, RangeProvider, Weapon,
  WatchTitle, WatchMission, WatchSubject, WatchSubjectTeam, WatchSubjectKind,
  WatchShot, WatchEvent, WatchEventTeam,
};

/* A CONDITION A ROW MAY CARRY. `when !flag` inverts; there is no `and`, because a row that needs two
 * conditions is two rows or a missing flag. */
enum class FBHudFlag : uint8_t {
  Always, Telemetry, Airborne, WeightOnWheels, GearDown, Speedbrake, EngineRunning,
  RadarOn, RadarContact, Locked, IffFriendly,
  GunReady, GunFiring, GunValid, GunInRange, GunInFunnel,
  DlzValid, InZone, AgValid, AgInRange, AgRelease,
  Armed, Designating, StoresSelected, Supersonic,
  Bingo, Alow, GearUnsafe,
  NavValid, IlsWindow,
  WatchHeld, WatchEvent,
};

enum class FBHudFrame : uint8_t { Screen, World, Body };
enum class FBHudAlign : uint8_t { Left, Centre, Right };

struct FBHudElement {
  FBHudKind Kind = FBHudKind::Text;
  FBHudFrame Frame = FBHudFrame::Screen;
  FBHudAlign Align = FBHudAlign::Left;
  /* Position: a FRACTION of the drawn window plus an offset in scaled pixels, so one declaration
   * holds at every window size the same way the symbology's angular sizes do. */
  float X = 0.0f, Y = 0.0f, Dx = 0.0f, Dy = 0.0f;
  float W = 0.0f, H = 0.0f;
  float Size = 1.0f, Size2 = 0.0f;
  float Span = 0.0f, Step = 0.0f, Gap = 0.0f, Len = 0.0f;
  float R = 0.0f, G = 0.0f, B = 0.0f;
  bool HaveColour = false;
  int Count = 0;
  FBHudNum Src = FBHudNum::None, Src2 = FBHudNum::None;
  FBHudStr Str = FBHudStr::None;
  FBHudFlag When = FBHudFlag::Always;
  bool Invert = false;
  /* THE NUMERIC HALF OF A CONDITION. A flag answers yes/no; a caption that must fade after eight
   * seconds needs a THRESHOLD, and the threshold belongs to the declaration and not to the engine —
   * so a row may gate itself on any number in the vocabulary being inside a band. */
  FBHudNum Gate = FBHudNum::None;
  float Lo = 0.0f, Hi = 0.0f;
  bool HaveLo = false, HaveHi = false;
  std::string Fmt, Literal;
};

struct FBHudDeck {
  std::string Name;
  float Scale = 1.0f;
  float R = 0.30f, G = 1.0f, B = 0.40f;      /* MIL-STD-1787 monochrome green, the tree's default */
  float TfovDeg = 0.0f;                      /* 0 = symbols at true angular size, no magnification */
  float InsetPx = 10.0f;
  std::vector<FBHudElement> Elements;

  bool Empty() const { return Elements.empty(); }
};

/* Text in, deck out. Every diagnostic carries its line number; a row with an unknown word is an
 * ERROR and never a silently dropped line — a mod must be able to see that its HUD did not load. */
bool FBParseHud(const std::string &text, FBHudDeck &out, std::string *err);

float FBHudNumber(FBHudNum id, const FBState &s, const FBHudEnv &env);
const char *FBHudString(FBHudStr id, const FBState &s, const FBHudEnv &env);
bool FBHudFlagOn(FBHudFlag id, const FBState &s, const FBHudEnv &env);

} // namespace FlightBox::Systems
#endif
