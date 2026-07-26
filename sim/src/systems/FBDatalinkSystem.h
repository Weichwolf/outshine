/* FlightBox — FBDatalinkSystem: the module's Comms/Datalink slot, and the FIRST system through which
 * one unit ever perceives another. It replaces FBSystemSlots.h's NoOp FBCommsSystem the same way
 * FBDisplaySystem replaced a NoOp Displays slot — interface plus a REAL default big enough for its own
 * file; a module whose terminal genuinely differs subclasses and overrides the hooks below
 * (modules/f16/FBF16Datalink implements the F-16's TNDL contact filter).
 *
 * WHAT IT MODELS (doc/f16/datalink-iff.md — MIDS/Link-16, DCS' TNDL): a COOPERATIVE network, not a
 * sensor. Every terminal periodically broadcasts its OWN navigation fix and its OWN identity; every
 * terminal in range receives it. There is no search, no scan volume and no identification problem —
 * callsign and faction arrive with the message. Consequences this class implements literally:
 *   - faction gate: the net is the own team's. A hostile unit is not on it and can never appear.
 *   - the SENDER must be transmitting. XMT is the sender's switch, published in its snapshot
 *     (units/FBUnit's FBUnitSignature) — a receiver reading it is reading a radiated emission, not the
 *     other module's private state.
 *   - line of sight: the reachable range is min(terminal range, radio horizon of the two altitudes) —
 *     a UHF-band net has no over-the-horizon path of its own.
 *   - a NETWORK CYCLE, not a live feed: positions refresh once per kNetPeriodS. Between cycles the
 *     picture stands still and its AGE counts up; that age IS the latency the pilot must fly with, and
 *     it is never zero because even a fresh message describes a pose published at the last tick barrier
 *     (units/FBUnit's snapshot contract).
 *   - a track survives missed cycles for kDropAfterCycles before it is dropped, exactly as a terminal
 *     holds a contact briefly rather than blinking it out on one lost message.
 *
 * XMT vs. POWER (two switches, because the real terminal has two): SetPowered is the terminal itself —
 * off means it neither transmits nor receives, and this unit's track list is empty. SetTransmit is the
 * HSD's XMT OFF / EMCON case — the terminal still RECEIVES the whole picture, it merely stops
 * radiating, so it is the OTHER units that lose this one from their lists. Modelling XMT as
 * "receive too" would be wrong in the direction that matters for stage 7's training fight: EMCON is
 * about not being seen, not about being blind.
 *
 * WHERE THE REGISTRY MAY GO (CLAUDE.md "Kein Cheaten"): this class and a future radar are the only
 * things in systems/ or modules/ that ever hold a units/FBUnitRegistry. Everything downstream —
 * above all FBPilot — reads the resulting tracks out of FBState like any other instrument, which is
 * what makes an AI pilot's picture of the world exactly as good as its sensors and no better. */
#ifndef FBDATALINKSYSTEM_H
#define FBDATALINKSYSTEM_H

#include "FBDatalinkTrack.h"
#include "FBState.h"
#include "FBTeam.h"
#include "FBTelemetry.h"
#include "FBFdm.h"

namespace FlightBox {

class FBUnit;
class FBUnitRegistry;

class FBDatalinkSystem : public FBTelemetrySource {
public:
  /* Link-16 PPLI (Precise Participant Location and Identification) reporting: network design sets the
   * slot allocation, and a fighter's own-position report sits at roughly one per second. One documented
   * constant, used as the net cycle for every terminal. */
  static constexpr double kNetPeriodS = 1.0;
  /* Three missed cycles before a held track is dropped — the terminal-holds-a-contact behaviour. */
  static constexpr double kDropAfterCycles = 3.0;
  /* Generic placeholder range for "some cooperative net" (the same role FBPilot's generic V-speeds
   * play), deliberately NOT a real terminal's figure — FBF16Datalink supplies MIDS-LVT's. */
  static constexpr double kGenericRangeNm = 150.0;

  ~FBDatalinkSystem() override = default;

  /* Boot wiring, once per unit (units/FBSimUnit's constructor through FBModule::SetUnitIdentity): who
   * this terminal is, so it can skip its own PPLI and know whose net it is on. */
  void SetIdentity(int selfId, FBUnitTeam team) { SelfId_ = selfId; SelfTeam_ = team; }

  void SetPowered(bool on) { Powered_ = on; }
  void SetTransmit(bool on) { Transmit_ = on; }
  bool Powered() const { return Powered_; }
  /* What the outside world can observe (units/FBUnit's FBUnitSignature): a powered-down terminal cannot
   * transmit no matter how the XMT switch stands. */
  bool Transmitting() const { return Powered_ && Transmit_; }

  void SetMaxRangeM(double m) { MaxRangeM_ = m; }
  double MaxRangeM() const { return MaxRangeM_; }

  /* The override point (class banner). `st` is this unit's own navigation fix, `net` the borrowed
   * registry of published unit snapshots (null = no net at all), `simTimeS` the module's own sim clock
   * — absolute time, so the result cannot depend on how often the module happens to cycle this slot.
   * Writes the track list into `state` and nothing else. */
  virtual void Run(FBState &state, const fb_fdm_state &st, const FBUnitRegistry *net, double simTimeS);

  const char *TelemetryName() const override { return "dl"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

protected:
  /* The contact filter — the F-16's HSD FR ON / FL ON / FR OFF is an override of THIS (doc/f16/
   * datalink-iff.md). `flightIndex` is the sender's ordinal among this team's participants in registry
   * order, i.e. the mission's declaration order, so index 0 is the flight lead. The default net shows
   * every friendly participant. */
  virtual bool AcceptContact(const FBUnit &sender, int flightIndex) const {
    (void)sender; (void)flightIndex;
    return true;
  }

  /* Radio horizon (m) between two antenna heights (m ASL) — the 4/3-earth line-of-sight rule
   * d[nm] = 1.23 * sqrt(h[ft]), summed over both ends. Terrain masking is NOT modelled here: that needs
   * the world's DEM along the path and belongs to whichever system first has a reason to pay for it. */
  static double RadioHorizonM(double altAM, double altBM);

private:
  /* The held picture, rebuilt at the net cycle and aged in between. Fixed capacity, no allocation. */
  FBDatalinkTrack Tracks_[kMaxDatalinkTracks]{};
  int TrackCount_ = 0;

  int SelfId_ = 0;
  FBUnitTeam SelfTeam_ = FBUnitTeam::Friendly;
  bool Powered_ = true, Transmit_ = true;
  double MaxRangeM_ = kGenericRangeNm * 1852.0;
  double NextCycleS_ = 0.0;   /* the net's own 1 Hz grid, independent of how often Run() is called */

  /* Telemetry's view of the last Run (the loop needs a number, not a table): nearest track's range/age,
   * -1 when the list is empty. */
  float NearestNm_ = -1.0f, NearestAgeS_ = -1.0f;

  void Cycle(const fb_fdm_state &st, const FBUnitRegistry &net, double simTimeS);
};

} // namespace FlightBox
#endif
