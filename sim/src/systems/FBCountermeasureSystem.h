/* FlightBox — FBCountermeasureSystem: die AKTIVE Haelfte des Defensiv-Slots — Vorraete, die
 * ALE-47-Programm-Zustandsmaschine, der Modus-Knopf und die geworfenen Wolken. In SEMI/AUTO triggert
 * sie auf den RWR-BLOCK, also auf die WARNUNG und nicht auf die Wahrheit; ob eine Wolke WIRKT,
 * entscheidet allein das gegnerische Radar. Fackeln werden gezaehlt und wirken (noch) nicht — es gibt
 * keinen IR-Sucher. doc/flightbox/sensors.md, Abschnitt 6. */
#ifndef FBCOUNTERMEASURESYSTEM_H
#define FBCOUNTERMEASURESYSTEM_H

#include "FBAvionicsCommand.h"
#include "FBCountermeasure.h"
#include "FBState.h"
#include "FBTelemetry.h"
#include "FBFdm.h"

namespace FlightBox {

class FBCountermeasureSystem : public FBTelemetrySource {
public:
  /* PRGM 1-4 plus Slap-Switch-Programm 5 und Bypass-Programm 6 (doc/f16/defence-rwr-cm.md §2.2). */
  static constexpr int kProgramCount = 6;
  static constexpr int kBypassProgram = 6;

  FBCountermeasureSystem();
  ~FBCountermeasureSystem() override = default;

  void SetMode(FBCmdsMode m);
  FBCmdsMode Mode() const { return Mode_; }
  /* Der PRGM-Knopf. Programme 5 und 6 bleiben unabhaengig davon erreichbar (§2.2), deshalb nimmt
   * Dispense() eine eigene Programmnummer. */
  bool SelectProgram(int n);
  int  SelectedProgram() const { return Selected_; }
  /* Umprogrammieren nur in STBY (§2.2) — eine Mission muss den Jet in den Zustand deklarieren, in dem
   * der echte dafuer sein muss. */
  bool SetProgram(int n, const FBCmProgram &p);
  const FBCmProgram &Program(int n) const;

  void SetLoadout(int chaff, int flare);
  /* Die BINGO-Menge je Typ, 0..99: darunter leuchtet „LO" und AUTOMATISCHES Werfen entfaellt. */
  bool SetBingo(int chaff, int flare);
  int  ChaffRemaining() const { return Chaff_; }
  int  FlareRemaining() const { return Flare_; }

  /* CMS Aft / CMS Right: Zustimmung in SEMI (je Abwurf) und AUTO (je Moduswechsel). */
  void SetConsent(bool on);
  bool Consent() const { return Consent_; }

  /* Nur ueber den Kommandobus erreicht, nie direkt gerufen: ein Abwurf kann abgelehnt werden, die
   * Ablehnung hat einen Grund, beides steht im Protokoll. `program` 0 = der vom PRGM-Knopf gewaehlte. */
  bool Dispense(int program, double nowS, FBCommandOutcome &outcome, FBCommandReason &reason);

  /* Was dieses Flugzeug gerade in der Luft haengen hat — an der Tick-Barriere in der Einheiten-Signatur
   * publiziert, wie der Datalink-Sender und die Radarkeule. */
  const FBChaffCloud *Clouds() const { return Clouds_; }
  int ActiveClouds() const { return ActiveClouds_; }

  /* `simTimeS` ist ABSOLUT — daran werden Salven-/Burst-Intervalle gemessen, damit ein Programm in
   * seiner deklarierten Rate spielt, egal wie oft das Modul den Slot taktet. */
  virtual void Run(FBState &state, const fb_fdm_state &st, double simTimeS);

  const char *TelemetryName() const override { return "cm"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

protected:
  /* WELCHES automatische Programm WELCHE Bedrohung beantwortet: die Quelle sagt, dass das System eines
   * auswaehlt, aber nie welches — die Zuordnung ist Doktrin eines Moduls, also ein Hook. */
  virtual int AutomaticProgram(FBRwrThreatMode worst) const;

  /* Programmieren ohne das STBY-Gate, fuer die eigene Tabelle eines Moduls (dessen Konstruktor IST die
   * Bodencrew). Das oeffentliche SetProgram ist der DED-Pfad und behaelt das Gate. */
  bool InstallProgram(int n, const FBCmProgram &p);

  FBCmProgram Programs_[kProgramCount];

private:
  /* Der Abspielkopf eines Typs: welche Salve, welche Patrone darin, wann die naechste faellig ist. */
  struct Player {
    bool   Running = false;
    int    SalvosLeft = 0;
    int    BurstLeft = 0;
    double NextS = 0.0;
  };

  void LoadGenericPrograms();
  void StartProgram(int n, double nowS, const char *why);
  void StopProgram(const char *why);
  bool PlayType(FBCmType type, const FBCmProgramType &p, Player &pl, const fb_fdm_state &st,
                double simTimeS);
  void Eject(FBCmType type, const fb_fdm_state &st, double simTimeS);
  void ServiceAutomatic(const FBState &state, double simTimeS);
  bool Low(FBCmType type) const;

  FBCmdsMode Mode_ = FBCmdsMode::Off;
  FBCmdsStatus Status_ = FBCmdsStatus::NoGo;   /* was Run() publizierte, fuer die Telemetriezeile */
  int  Selected_ = 1;
  int  Running_ = 0;            /* gerade laufende Programmnummer, 0 = keine */
  bool Consent_ = false;
  bool AwaitingConsent_ = false;   /* SEMI: eine Bedrohung steht, die Abfrage laeuft */

  int  Chaff_ = 0, Flare_ = 0;
  int  BingoChaff_ = 5, BingoFlare_ = 5;
  int  ChaffOut_ = 0, FlareOut_ = 0;   /* in diesem Einsatz geworfen */

  Player ChaffPlayer_, FlarePlayer_;

  FBChaffCloud Clouds_[kMaxChaffClouds];
  int  NextCloud_ = 0;          /* Ring: die frischesten kMaxChaffClouds Patronen */
  int  ActiveClouds_ = 0;
  int  BlockStatus_ = 0;
};

} // namespace FlightBox
#endif
