/* FlightBox — FBFlightPicture: what a pilot knows about his own FLIGHT, and the assignment he derives
 * from it. FBBfmTrack's sibling: built ONLY from FBState blocks (Datalink + Radar), so it inherits the
 * perception boundary rather than reopening it — no registry, no world, no other unit's pilot.
 * doc/formation.md, sections 3-5. */
#ifndef FBFLIGHTPICTURE_H
#define FBFLIGHTPICTURE_H

#include "FBFlight.h"
#include "FBState.h"
#include "FBTelemetry.h"
#include "FBFdm.h"

namespace FlightBox::Pilot {

/* WHERE the assignment came from. The order is a hierarchy of information, not of preference: what a
 * mate SAYS beats what this pilot can work out about him, and both beat a rule agreed before takeoff. */
enum class FBSortSource { None, Cooperative, Contract };

/* THE BRIEFED CONTRACT — the sort a flight without a shared picture can still agree on, because it is
 * agreed BEFORE anybody sees anything. Each member applies it to the picture he actually has, which is
 * why two members with different pictures can still collide (doc/formation.md, section 5.3). */
enum class FBSortContract { None, Left, Right, Near, Far };

bool FBSortContractFromString(const char *s, FBSortContract &out);

/* One member of the flight as this pilot sees it — himself from his own state, everybody else from a
 * PPLI that is up to kDropAfterCycles net cycles old. */
struct FBFlightMember {
  int    UnitId = 0;
  int    Position = 0;
  bool   Self = false;
  double LatDeg = 0.0, LonDeg = 0.0, AltM = 0.0;
  double HeadingDeg = 0.0, SpeedMs = 0.0;
  float  AgeS = 0.0f;
  FBFlightReport Report;
};

class FBFlightPicture : public FBTelemetrySource {
public:
  static constexpr int kMaxMembers = kMaxDatalinkTracks + 1;

  /* The pilot's OWN numbers, handed in per update instead of duplicated as hooks here: this class
   * decides WHO takes WHAT, and every quantity it needs to do so already exists one level up. */
  struct FBSortParams {
    double TurnRateDegS = 10.0;   /* how fast a fighter of this class points its nose (FBPilot::CornerTurnRateDegS) */
    double CommitRangeM = 0.0;    /* the range at which this pilot commits — his own lock range */
    double AxisDeg = 0.0;         /* the briefed vector, the reference the CONTRACT sorts against */
    /* What a swap THROWS AWAY: a new single-target track has to settle before it is a firing solution
     * (FBPilot::kInterceptTrackSettleS), so a re-assignment must save at least that much to be worth
     * making. The turn itself is not hysteresis — it is already inside the cost. */
    double SwitchMarginS = 2.0;
  };

  void SetFlight(const FBFlightId &f) { Self_ = f; }
  const FBFlightId &Flight() const { return Self_; }
  void SetContract(FBSortContract c) { Contract_ = c; }
  FBSortContract Contract() const { return Contract_; }

  /* ONE decision tick. `bound` is this jet's own obligation (a launched round that still needs its
   * illumination) — the picture does not know the weapon, the pilot does. */
  void Update(const FBState &state, const Fdm::fb_fdm_state &st, double nowS, double dt, bool bound,
              bool threatened, const FBSortParams &p);

  bool Declared() const { return Self_.Declared(); }
  /* An assignment exists at all only for a declared flight with something to divide. */
  int  AssignedTrack() const { return AssignTrack_; }
  FBSortSource Source() const { return Src_; }
  int  MemberCount() const { return MemberCount_; }
  int  MateCount() const { return MemberCount_ > 0 ? MemberCount_ - 1 : 0; }

  /* THE LEAD as a moving point: the station a wingman holds is defined against it, and it is a REPORT
   * with an age, never a position. Null = nobody in this flight ahead of me is on the net. */
  const FBFlightMember *Lead() const { return LeadIdx_ >= 0 ? &Members_[LeadIdx_] : nullptr; }

  /* COVER: is somebody else in this flight currently unable to defend itself? Only a cooperative
   * picture can answer this — a flight without a channel reads false, and that is the finding, not a
   * default (doc/formation.md, section 6). */
  bool MateBound() const { return MateBound_; }
  bool SelfBound() const { return SelfBound_; }

  /* WHAT THIS JET TELLS THE FLIGHT — the pilot fills the target point, this assembles the message. */
  void SetOwnEngagement(bool engaging, double latDeg, double lonDeg, double altM);
  FBFlightReport Report() const { return Report_; }

  /* Bookkeeping the mission control loop reads back (doc/formation.md, section 8). */
  double StationErrM() const { return StationErrM_; }
  void   NoteStationErr(double m) { StationErrM_ = m; }
  void   NoteDeferred(double dt) { DeferS_ += dt; }

  const char *TelemetryName() const override { return "flt"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  /* The correlation gate between a mate's REPORTED target point and one of this jet's own contacts.
   * A report is up to kDropAfterCycles cycles old and the point it names moves at fighter speed, so
   * the gate must grow with the age: base + age * kCorrelateSpeedMs. */
  static constexpr double kCorrelateBaseM = 1000.0;
  static constexpr double kCorrelateSpeedMs = 300.0;
  /* ...and how much closer the best candidate must be than the next before the match counts as one. */
  static constexpr double kCorrelateUniqueFrac = 0.5;
  /* An assignment is only worth changing if the change saves more time than it costs — the swap's own
   * re-point time is the hysteresis, and it needs no number of its own. */
  static constexpr double kMinSwitchGainS = 0.0;

  void BuildMembers(const FBState &state, const Fdm::fb_fdm_state &st);
  int  Assign(const FBState &state, const Fdm::fb_fdm_state &st, double nowS, const FBSortParams &p);
  int  ContractPick(const FBState &state, const FBSortParams &p) const;
  double CostS(const FBFlightMember &m, double tgtLat, double tgtLon, const FBSortParams &p) const;

  FBFlightId Self_;
  FBSortContract Contract_ = FBSortContract::None;

  FBFlightMember Members_[kMaxMembers]{};
  int MemberCount_ = 0;
  int LeadIdx_ = -1;
  int SelfIdx_ = -1;

  int AssignTrack_ = 0;
  FBSortSource Src_ = FBSortSource::None;
  bool SelfBound_ = false, MateBound_ = false;
  bool Duplicate_ = false;
  int  FreeContacts_ = 0;
  int  Switches_ = 0;
  double BothBoundS_ = 0.0, CoverS_ = 0.0, ExposedS_ = 0.0, DeferS_ = 0.0;
  double StationErrM_ = -1.0;

  FBFlightReport Report_;
};

} // namespace FlightBox::Pilot
#endif
