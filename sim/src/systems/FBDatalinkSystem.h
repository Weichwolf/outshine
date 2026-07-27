/* FlightBox — FBDatalinkSystem: der Comms/Datalink-Slot, das KOOPERATIVE Netz (MIDS/Link-16) und
 * damit kein Sensor im Suchsinn: jeder Teilnehmer sendet seine EIGENE Loesung samt Identitaet.
 * doc/flightbox/sensors.md, Abschnitt 3. */
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
  /* Link-16-PPLI: die Eigenpositionsmeldung eines Jaegers liegt bei rund einer pro Sekunde. */
  static constexpr double kNetPeriodS = 1.0;
  static constexpr double kDropAfterCycles = 3.0;   /* [SET] Terminal haelt einen Kontakt kurz */
  /* Ausdruecklicher PLATZHALTER „irgendein kooperatives Netz", keine echte Terminalzahl. */
  static constexpr double kGenericRangeNm = 150.0;

  ~FBDatalinkSystem() override = default;

  /* Boot-Wiring: wer dieses Terminal IST — eigene PPLI ueberspringen, eigenes Netz kennen. */
  void SetIdentity(int selfId, FBUnitTeam team) { SelfId_ = selfId; SelfTeam_ = team; }

  void SetPowered(bool on) { Powered_ = on; }
  void SetTransmit(bool on) { Transmit_ = on; }
  bool Powered() const { return Powered_; }
  /* Was die Aussenwelt sieht: ein stromloses Terminal sendet nicht, wie auch immer XMT steht. */
  bool Transmitting() const { return Powered_ && Transmit_; }

  void SetMaxRangeM(double m) { MaxRangeM_ = m; }
  double MaxRangeM() const { return MaxRangeM_; }

  /* `simTimeS` ist ABSOLUTE Zeit, damit das Ergebnis nicht davon abhaengt, wie oft das Modul den Slot
   * taktet. `net` null = gar kein Netz. */
  virtual void Run(FBState &state, const fb_fdm_state &st, const FBUnitRegistry *net, double simTimeS);

  const char *TelemetryName() const override { return "dl"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

protected:
  /* Der Kontaktfilter. `flightIndex` = Ordinal des Absenders unter den Teilnehmern dieser Fraktion in
   * Registry- = Missionsreihenfolge, Index 0 also der Flight Lead. */
  virtual bool AcceptContact(const FBUnit &sender, int flightIndex) const {
    (void)sender; (void)flightIndex;
    return true;
  }

  /* Funkhorizont (m): 4/3-Erd-Sichtlinienregel d[nm] = 1,23·sqrt(h[ft]), ueber beide Enden summiert.
   * Terrain-Maskierung entlang des Pfades ist NICHT modelliert (braeuchte das DEM entlang der Strecke). */
  static double RadioHorizonM(double altAM, double altBM);

private:
  /* Das gehaltene Bild, feste Kapazitaet, keine Allokation. */
  FBDatalinkTrack Tracks_[kMaxDatalinkTracks]{};
  int TrackCount_ = 0;

  int SelfId_ = 0;
  FBUnitTeam SelfTeam_ = FBUnitTeam::Friendly;
  bool Powered_ = true, Transmit_ = true;
  double MaxRangeM_ = kGenericRangeNm * 1852.0;
  double NextCycleS_ = 0.0;   /* das eigene 1-Hz-Raster des Netzes, unabhaengig vom Run()-Takt */

  /* Telemetrie braucht eine Zahl, keine Tabelle: naechster Track, -1 bei leerer Liste. */
  float NearestNm_ = -1.0f, NearestAgeS_ = -1.0f;

  void Cycle(const fb_fdm_state &st, const FBUnitRegistry &net, double simTimeS);
};

} // namespace FlightBox
#endif
