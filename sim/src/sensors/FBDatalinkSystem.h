/* FlightBox — FBDatalinkSystem: der Comms/Datalink-Slot, das KOOPERATIVE Netz (MIDS/Link-16) und
 * damit kein Sensor im Suchsinn: jeder Teilnehmer sendet seine EIGENE Loesung samt Identitaet.
 * doc/sensors.md, Abschnitt 3. */
#ifndef FBDATALINKSYSTEM_H
#define FBDATALINKSYSTEM_H

#include <string>

#include "FBDatalinkTrack.h"
#include "FBState.h"
#include "FBTeam.h"
#include "FBTelemetry.h"
#include "FBFdm.h"

namespace FlightBox::Units { class FBUnit; class FBUnitRegistry; }

namespace FlightBox::Sensors {

class FBDatalinkSystem : public FBTelemetrySource {
public:
  /* Link-16-PPLI: die Eigenpositionsmeldung eines Jaegers liegt bei rund einer pro Sekunde. */
  static constexpr double kNetPeriodS = 1.0;
  static constexpr double kDropAfterCycles = 3.0;   /* [SET] Terminal haelt einen Kontakt kurz */
  /* Ausdruecklicher PLATZHALTER „irgendein kooperatives Netz", keine echte Terminalzahl. */
  static constexpr double kGenericRangeNm = 150.0;

  /* A BURIED CABLE is not subject to a radio horizon and cannot be jammed; a radio link is both. It is
   * the mission's choice and a real doctrinal trade, not a waiver — doc/air-defence-network.md §2. */
  enum class LinkMode { Radio = 0, Wire };

  ~FBDatalinkSystem() override = default;

  /* Boot-Wiring: wer dieses Terminal IST — eigene PPLI ueberspringen, eigenes Netz kennen. */
  void SetIdentity(int selfId, FBUnitTeam team) { SelfId_ = selfId; SelfTeam_ = team; }

  /* ---- THE FOUR HOOKS AN AIR-DEFENCE NET NEEDS, each defaulting to the built behaviour exactly, so a
   * mission that declares no net is byte-identical. doc/air-defence-network.md §2. ---- */

  /* WHICH KINDS CARRY A TERMINAL. The default is Aircraft only — a dropped store carries none — and it
   * is also why the class as built cannot hear another GROUND unit at all. */
  void SetCarriesTerminal(bool aircraft, bool ground) { CarryAir_ = aircraft; CarryGround_ = ground; }

  void SetLinkMode(LinkMode m, double mastM = 0.0) { Mode_ = m; MastM_ = mastM > 0.0 ? mastM : 0.0; }
  void SetNetPeriodS(double s) { if (s > 0.0) NetPeriodS_ = s; }
  void SetHoldCycles(double n) { if (n > 0.0) HoldCycles_ = n; }

  /* THE SURFACE UNDER THIS ANTENNA, pushed by the unit's owner on the same tick as the terrain sample.
   * The radio horizon is a height ABOVE THE REFLECTING SURFACE; without this the reach of a ground net
   * would be a function of the absolute map elevation, which is wrong in both directions. */
  void SetOwnGroundAslM(double m) { OwnGroundAslM_ = m; }

  /* WHOSE SILENCE IS WORTH A LINE IN THE LOG. Purely an events.log affordance: the member cannot tell
   * the four Silent causes apart — its block is Invalid either way — but the analyst may. The reason
   * never enters FBState and never reaches a decision. doc/air-defence-network.md §5. */
  void SetControlNode(const std::string &callsign);

  void SetPowered(bool on) { Powered_ = on; }
  void SetTransmit(bool on) { Transmit_ = on; }
  bool Powered() const { return Powered_; }
  /* Was die Aussenwelt sieht: ein stromloses Terminal sendet nicht, wie auch immer XMT steht. */
  bool Transmitting() const { return Powered_ && Transmit_; }

  void SetMaxRangeM(double m) { MaxRangeM_ = m; }
  double MaxRangeM() const { return MaxRangeM_; }

  /* `simTimeS` ist ABSOLUTE Zeit, damit das Ergebnis nicht davon abhaengt, wie oft das Modul den Slot
   * taktet. `net` null = gar kein Netz. */
  virtual void Run(FBState &state, const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry *net, double simTimeS);

  const char *TelemetryName() const override { return "dl"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

protected:
  /* Der Kontaktfilter. `flightIndex` = Ordinal des Absenders unter den Teilnehmern dieser Fraktion in
   * Registry- = Missionsreihenfolge, Index 0 also der Flight Lead. */
  virtual bool AcceptContact(const Units::FBUnit &sender, int flightIndex) const {
    (void)sender; (void)flightIndex;
    return true;
  }

  /* Funkhorizont (m): 4/3-Erd-Sichtlinienregel d[nm] = 1,23·sqrt(h[ft]), ueber beide Enden summiert.
   * BEIDE ARGUMENTE SIND ANTENNENHOEHEN UEBER GRUND, nicht ueber NN: der Horizont misst gegen die
   * reflektierende Oberflaeche, und zwei Stellungen auf 936 m ASL sehen einander nicht 252 km weit.
   * Terrain-Maskierung entlang des Pfades ist NICHT modelliert (braeuchte das DEM entlang der Strecke). */
  static double RadioHorizonM(double aglAM, double aglBM);

private:
  /* Das gehaltene Bild, feste Kapazitaet, keine Allokation. */
  FBDatalinkTrack Tracks_[kMaxDatalinkTracks]{};
  int TrackCount_ = 0;

  int SelfId_ = 0;
  FBUnitTeam SelfTeam_ = FBUnitTeam::Friendly;
  bool Powered_ = true, Transmit_ = true;
  double MaxRangeM_ = kGenericRangeNm * 1852.0;
  double NextCycleS_ = 0.0;   /* das eigene 1-Hz-Raster des Netzes, unabhaengig vom Run()-Takt */

  bool CarryAir_ = true, CarryGround_ = false;
  LinkMode Mode_ = LinkMode::Radio;
  double MastM_ = 0.0;
  double NetPeriodS_ = kNetPeriodS;
  double HoldCycles_ = kDropAfterCycles;
  double OwnGroundAslM_ = 0.0;
  char   ControlNode_[kDatalinkCallsignLen] = {};
  bool   NodeHeard_ = false;
  /* Barrage jamming swamps a RECEIVER's front end, so this bit is about the ear and not the mouth. One
   * distance test against another team's published radius — no die, no ramp, no probability. */
  bool   Jammed_ = false;

  /* Telemetrie braucht eine Zahl, keine Tabelle: naechster Track, -1 bei leerer Liste. */
  float NearestNm_ = -1.0f, NearestAgeS_ = -1.0f;

  void Cycle(const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry &net, double simTimeS);
};

} // namespace FlightBox::Sensors
#endif
