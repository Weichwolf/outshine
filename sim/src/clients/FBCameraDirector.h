#ifndef FBCAMERADIRECTOR_H
#define FBCAMERADIRECTOR_H

#include <cstdint>
#include <string>
#include <vector>
#include "FBUnit.h"

namespace FlightBox::Clients {

/* ONE UNIT AS A SPECTATOR SEES IT. Deliberately NOT `const FBUnit *`: the director must not be able to
 * reach a simulation object at all, so the client that owns the cast copies out the two things a
 * foreign observer may legitimately notice (units/FBUnit.h) — where the unit is and what a hit did to
 * it — and nothing else. That is also why this header names no registry: adding one would move
 * verify-layers' perception count, and a camera is not a sensor. */
struct FBStageUnit {
  int Id = -1;
  Units::FBUnitKind Kind = Units::FBUnitKind::Aircraft;
  FBUnitTeam Team = FBUnitTeam::Neutral;
  const char *Name = "";              /* borrowed: the actor's own callsign, mission data */
  Units::FBUnitPose Pose;
  Units::FBDamageSignature Damage;
  float DimM = 0.0f;                  /* largest published silhouette dimension, 0 = nothing to see */
};

/* WHY THE CAMERA IS WHERE IT IS. The order IS the priority order: a higher one may cut a lower one
 * mid-shot, an equal or lower one waits for the running shot's hold to expire.
 * WRECK OUTRANKS IMPACT, and that is the whole finding this class was built for: a burst and the fire
 * it leaves happen at the same PLACE, so the longer shot contains the shorter one — while the other
 * order swallows the wreck, because the impact is posted on the same tick and outranks it. Measured on
 * `suppress-killed`: with Impact above Wreck the camera left the burning battery after 7 s and went
 * back to the aircraft, and the fire was never in frame again. */
enum class FBShotKind { Home, Takeoff, Launch, Landing, Impact, Wreck };

const char *FBShotName(FBShotKind k);

struct FBCameraPose {
  double Eye[3] = {0, 0, 0};
  double Fwd[3] = {0, 0, 0}, Right[3] = {0, 0, 0}, Up[3] = {0, 0, 0};
  double LatDeg = 0.0, LonDeg = 0.0;   /* the streamer refines around the CAMERA, not around the jet */
};

/* THE REGIE. It reads published poses and published damage, decides what is worth looking at, and
 * returns one camera pose per frame. It writes nothing, owns nothing and is stepped by the client:
 * Observe() once per simulation tick (events are found on tick edges), Camera() once per frame. */
class FBCameraDirector {
public:
  void SetHome(int unitId) {
    Home_ = unitId;
    if (Shot_.Subject < 0) Shot_.Subject = unitId;   /* a run that opens HELD never reaches the return rule */
  }

  void Observe(const std::vector<FBStageUnit> &stage, double simT);
  FBCameraPose Camera(double dtS, double alpha);

  /* THE WATCHER'S TWO KEYS. Holding freezes the cut on whatever is on screen; stepping picks the next
   * unit by hand and implies a hold, because a hand-picked subject that the next event overrides is
   * not a hand-picked subject. */
  void ToggleHold();
  void Step(int dir);

  bool Held() const { return Held_; }
  int SubjectId() const { return Shot_.Subject; }
  FBShotKind ShotKind() const { return Shot_.Kind; }

private:
  /* WHAT THIS UNIT DID LAST TICK — the whole event detector. Every field is either a published
   * quantity or a memory of one; there is no channel here that the simulation could be asked. */
  struct Track {
    int Id = -1;
    Units::FBUnitKind Kind = Units::FBUnitKind::Aircraft;
    FBUnitTeam Team = FBUnitTeam::Neutral;
    std::string Name;
    Units::FBUnitPose Prev, Cur;
    uint16_t Hits = 0;
    bool Airborne = false;
    bool BurnSeen = false;    /* the moment it stopped being able to finish the sortie, wherever it was */
    bool WreckSeen = false;   /* ...and the moment it was that AND on the ground: cut to ONCE each */
    bool HavePend = false;    /* an event waiting for the running shot to let it in */
    FBShotKind PendKind = FBShotKind::Home;
    double PendS = 0.0;
    float DimM = 0.0f;
    double LastMoveS = 0.0;   /* a subject that stopped publishing new positions has nothing left to show */
  };

  struct Shot {
    FBShotKind Kind = FBShotKind::Home;
    int Subject = -1;
    double StartS = 0.0, HoldS = 0.0;
    double AzDeg = 0.0;        /* tripod: bearing from the event to the camera, orbited while it runs */
    double EventEcef[3] = {0, 0, 0};
    double EventLatDeg = 0.0, EventLonDeg = 0.0, EventGroundM = 0.0;
    float DimM = 0.0f;
    bool Tripod = false;       /* false = chase the subject, true = stand at a place and watch it */
  };

  Track *Find(int id);
  void Post(Track &t, FBShotKind kind);
  bool Consider(FBShotKind kind, Track &t);
  void CutTo(FBShotKind kind, const Track &t);
  void ChasePose(const Units::FBUnitPose &p, double dtS, FBCameraPose &out);
  void TripodPose(const Units::FBUnitPose &p, double dtS, FBCameraPose &out);

  std::vector<Track> Tracks_;
  Shot Shot_;
  int Home_ = -1;
  bool Held_ = false;
  bool HaveSim_ = false;
  double SimS_ = 0.0;
  double ChaseYawDeg_ = 0.0, ChasePitchDeg_ = 0.0;
  bool ChaseHave_ = false;
  double LastEye_[3] = {0, 0, 0};   /* where the camera stood, so a cut keeps the side it was on */
  bool HaveEye_ = false;
};

} // namespace FlightBox::Clients
#endif
