# FlightBox — Wahrnehmung: Datalink, Radar, RWR, Gegenmaßnahmen

> Body still in German — translation pass pending (see [roadmap](../roadmap.md)).

**Gegenstand.** Wie eine Einheit von anderen Einheiten erfährt — und wie sie es *nicht* darf. Das ist
die schärfste Grenze der Architektur: sie ist nicht per Konvention, sondern per Include-Baum und
Typwahl gezogen und per Grep prüfbar.

**Primärquellen (Code, dieses Repo):**

| Datei | Rolle |
|---|---|
| `sim/src/systems/FBDatalinkSystem.{h,cpp}` | kooperatives Netz (Comms/Datalink-Slot) |
| `sim/src/systems/FBRadarSystem.{h,cpp}` | aktives Luft-Luft-Radar (Sensors-Slot) |
| `sim/src/systems/FBRwrSystem.{h,cpp}` | passiver Warnempfänger (Defensiv, passive Hälfte) |
| `sim/src/systems/FBCountermeasureSystem.{h,cpp}` | Täuschkörperanlage (Defensiv, aktive Hälfte) |
| `sim/src/modules/f16/FBF16Datalink.h`, `FBF16Fcr.{h,cpp}`, `FBF16Rwr.h`, `FBF16Cmds.{h,cpp}` | die F-16-Ableitungen |
| `sim/src/modules/missile/FBMissileSeeker.{h,cpp}`, `FBMissileUplink.{h,cpp}` | die zwei Ableitungen des Flugkörpers |
| `sim/src/units/FBUnit.h`, `FBSimUnit.cpp` (`PublishPose`) | die publizierte Emissions-Signatur |
| `sim/src/modules/f16/FBF16Module.cpp` (`Run`, `ApplySetup`) | Taktung, Health-Gate, Missionsschalter |

**Wertetypen** (`FBDatalinkTrack`, `FBRadarContact`/`FBIffReply`, `FBEmitterSignature`, `FBRwrThreat`,
`FBCmProgram`/`FBChaffCloud`) sind in `core.md` als TYPEN dokumentiert. Diese Datei dokumentiert ihr
VERHALTEN — wer sie erzeugt, unter welchen Bedingungen, mit welcher Alterung und welchem Preis.
Missions-Schalter und Telemetriespalten stehen vollständig in `doc/mission-format.md`; hier nur die
Verweise. Die reale Vorlage (ALR-56M, ALE-47, APG-68, MIDS/TNDL) steht in `doc/f16/radar-sensors.md`,
`doc/f16/defence-rwr-cm.md`, `doc/f16/datalink-iff.md`.

**Kennzeichnung.** `[SET]` = FlightBox-Setzung ohne Quelle (der Code markiert sie so).
`[DERIVED]` = aus einer genannten Formel/Quelle hergeleitet. `[DOC]` = aus `doc/f16/` belegt.

---

## Spec

How a unit learns about other units — and how it may **not**. This is the sharpest boundary in the
architecture: drawn by include graph and type choice, checkable by grep.

| Contract | Acceptance / measurement anchor |
|---|---|
| The unit registry reaches the SENSOR slots and nothing else | `#include "FBUnitRegistry.h"` / `.Units()` appear in exactly four files under `sim/src/systems` + `sim/src/modules` (the three sensor slots + the missile's uplink receiver) |
| A radar contact is anonymous | `core/FBRadarContact` carries range/bearing/az/el/closure and a radar-owned track number — no unit id, no callsign, no team |
| The only identity source is IFF Mode 4, and it is two-valued | `FBIffReply` has no value "hostile" |
| Perception costs time | a track firms after `kHitsToFirm` consecutive looks and coasts after leaving the volume; the block carries age, never "live" |
| Cooperative ≠ active | the datalink gives identity away and needs a transmitting sender; the radar gets an echo and pays with an emission |
| What a set radiates is derived from what it is doing | `Emission()` from the pattern actually flown — antenna state and radiated signature cannot diverge |
| The RWR sees only published emissions, never truth, and has a real blind zone | elevation coverage limit at the own antenna; no range, ever (an RWR measures power) |
| Deception is a model, not a die | chaff works through the Doppler notch, measured from own quantities over a dwell |

## State

Built: datalink, radar with mode set, RWR, countermeasures — plus the two missile derivations.

| Piece | Status | Anchor |
|---|---|---|
| `FBDatalinkSystem` + `FBF16Datalink` (MIDS/Link-16, 1 Hz net cycle, 3-cycle hold) | built | `9190e7c` |
| `FBRadarSystem` + `FBF16Fcr` (CRM, four ACM sub-modes, STT as its own volume) | built | `4049a7b` |
| `FBRwrSystem` + `FBF16Rwr` (ALR-56M geometry, PRIORITY/OPEN display cap) | built | `439f53a` |
| `FBCountermeasureSystem` + `FBF16Cmds` (ALE-47 programs, OFF…BYP state machine, chaff clouds) | built | `439f53a` |
| `FBMissileSeeker` / `FBMissileUplink` | built | `5c68fc5` |

## Gaps

### Contradictions between claim and code (from the retired `TODO.md` §1)

| Place | Contradiction |
|---|---|
| `core/FBRadarContact.h` | banner claims track numbers are reused after a drop; `FBRadarSystem::NextTrackNum_` counts monotonically and never does. Consumers rely on uniqueness **undocumented**. |
| `systems/FBRwrSystem` | `SelfTeam_` is stored and deliberately never read — dead state with cheat potential the moment somebody touches it |

### Deliberately not modelled (from the retired `TODO.md` §3)

| Thing | Consequence |
|---|---|
| Terrain masking for radar, datalink and radio path | air-to-air line of sight is always clear; would need a DEM raymarch per contact per look |
| No IR seeker | flares are counted and do nothing |
| No ECM/jammer, no MWS, IFF Mode 4 only, no measurement noise on sensor data | the whole CMDS/CMS/ECM interaction of the source material is absent |
| No formation concept — `fl` is simply the first unit | the datalink filter "flight leads only" is not real |

### Inventory (German, from the previous `Offene Punkte` section)

**Bewusste Lücken (dokumentiert, mit Begründung im Code):**

1. **Terrain-Maskierung fehlt vollständig** — weder das Radar (`FBRadarSystem`, kein `FBWorld`, kein
   DEM-Sample) noch der Funkpfad des Datalinks (`RadioHorizonM` kennt nur die geometrische
   Sichtlinie). Preis: ein DEM-Raymarch je Kontakt und je Look. Bis dahin ist jede Luft-Luft-Sichtlinie
   frei; Bodenziele und tieffliegende Einheiten sind dadurch systematisch zu leicht zu sehen.
2. **Kein IR-Sucher** → Fackeln werden geworfen, gezählt und wirken nicht. Es gibt heute keine Waffe,
   die sie täuschen könnten.
3. **Kein Luft-Boden-Radarmodus.** `FBRadarSystem` filtert auf `FBUnitKind::Aircraft`: Stores in freiem
   Flug und Bodenziele sind für jedes Radar unsichtbar. Ein Bodenziel ist damit ausschließlich über den
   Steerpoint/die Feuerleitrechnung anfliegbar, nie über einen Sensor.
4. **Keine Messfehler.** Geometrie ist exakt (Posen sind Wahrheit); simuliert werden ausschließlich
   Verfügbarkeit, Volumen, Zeit und Alterung. Es gibt kein Rauschen auf Entfernung, Peilung oder
   Annäherungsrate, und deshalb auch keine Track-Verwechslung.
5. **Keine Messungs-Assoziation.** Die interne Korrelation läuft über `UnitId` — ein Track kann nie auf
   das falsche Ziel überspringen. Das ist die ehrliche Grenze des Modells (im Header benannt), nicht
   Realität.
6. **Bedrohungsbibliothek einen Eintrag tief.** `Classify()` reicht die Emitter-Klasse durch; die
   Schätzung ist heute immer richtig. Die ALIC-/Symbolcode-Tabelle aus `doc/f16/defence-rwr-cm.md`
   Appendix B ist nicht übernommen (die Quelle transkribiert sie nicht).
7. **Kein ECM/Jammer.** `doc/f16/defence-rwr-cm.md` §2.2/§2.3 beschreibt die Wechselwirkung von
   CMDS-Modus, CMS und ECM-XMIT; FlightBox hat keinen Störsender, also fehlt diese ganze Kopplung.
8. **Kein MWS/Raketenanflugwarner.** Eine Startwarnung entsteht ausschließlich aus einem stützenden
   (Guidance-)Radar oder einem Raketensucher in der Keule — ein Flugkörper mit stillem Sucher im
   Anflug bleibt unbemerkt. (Deckt sich mit der Quellenlage: MWS auf Block 50 nicht funktional.)
9. **IFF kennt nur Mode 4.** Kein Mode 1/2/3, keine eigene Abfrage-Reichweitengrenze (abgefragt wird
   jeder feste Track im Volumen, alle 5 s).
10. **Kein Verbandskonzept** → `datalink_filter fl` behält den ERSTEN Teilnehmer der Fraktion. Ein
    dokumentierter Platzhalter für eine Lead-Zuweisung.
11. **Keine HUD-Symbologie für den Lock.** `doc/f16/hud-symbology.md` kennt weder TD-Box noch
    Locked-Target-Symbol; der Lock bleibt in FBState/Telemetrie/Events, bis die Symbologiequelle ihn
    abdeckt.

**Widersprüche / Ungenauigkeiten im Bestand:**

12. **Track-Nummern-Wiederverwendung.** Der Banner von `core/FBRadarContact.h` sagt, `TrackNum` werde
    „in Erfassungsreihenfolge vergeben und nach einem Drop WIEDERVERWENDET". `FBRadarSystem` zählt
    `NextTrackNum_` monoton hoch und verwendet nie eine Nummer erneut (nur der Array-SLOT wird
    wiederverwendet). Entweder ist der Kommentar zu korrigieren oder die Vergabe. Verhaltensrelevant:
    ein Konsument, der Nummern über einen Drop hinweg vergleicht, darf sich heute auf Eindeutigkeit
    verlassen — das ist ein undokumentierter Verlass.
13. **`FBF16Rwr::kOpenThreats = 16` übersteigt `kMaxRwrThreats = 8`.** Bewusst als dokumentierte Zahl
    stehen gelassen; heute ist der Deckel damit im OPEN-Modus nie bindend und PRIORITY (5) der einzige
    wirksame. Wächst die Erkennungstabelle, ändert sich das Verhalten schlagartig.
14. **Chaff hängt an der werfenden Einheit.** Eine Wolke kann nur ein Radar täuschen, das auf genau
    dieses Flugzeug schaut — nie eines, das einen Rottenkameraden 500 m daneben verfolgt. Bewusste
    Scope-Entscheidung mit ausgesprochener Folge; die Alternative wäre eine Wolke als eigene Einheit.
15. **Kein Windfeld** → Wolken stehen absolut still statt in der Luftmasse zu treiben. Bei starkem
    Höhenwind wäre das ein messbarer Unterschied.
16. **`kBeamRangeFactor` ist EINE Konstante für jeden Emitter.** Die Empfängerempfindlichkeit ist damit
    implizit für alle gleich; eine Rakete mit kleinem Sucher wird im selben Verhältnis „zu früh"
    gehört wie ein großes Feuerleitradar.
17. **Der RWR blendet nur in Elevation ab.** Rumpfabschattung in Azimut (z. B. ein Sender exakt hinter
    der eigenen Zelle) ist nicht modelliert — 360° Azimut sind wirklich 360°.
18. **`Emission()` publiziert das Suchvolumen als Keule**, obwohl der Strahl es nur einmal je Frame
    überstreicht. Ein Ziel im Volumen wird also durchgehend „bestrahlt" statt gepulst; die Dauer der
    tatsächlichen Bestrahlung (und damit ein realistischeres „neu"-Fenster) ist nicht modelliert.
19. **`FBRwrSystem` speichert `SelfTeam_` und liest es nie.** Absicht (Kommentar), aber ein toter
    Zustand, der bei einer künftigen Bibliothek nach Emitter-Typ leicht versehentlich zur
    Fraktionserkennung mutiert. Beim Anfassen prüfen.


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### 1. Die Grenze

#### 1.1 Die Regel

Ein Pilot — Mensch wie KI — **sieht andere Einheiten ausschließlich über simulierte Sensoren**. Die
Welt-Wahrheit „wer existiert wo" ist `units/FBUnitRegistry`: geborgte `const FBUnit*` in
Registrierungs- = Missions-Deklarationsreihenfolge. Sie reicht bis zu den SENSOR-Slots eines Moduls
und keinen Schritt weiter. Was ein Pilot weiß, steht in `FBState` — mit der Reichweite, dem
Scanvolumen, dem Netzzyklus und dem ALTER, mit dem der Sensor es dort hineingeschrieben hat.

`FBPilot::Run` trägt weder `FBUnitRegistry` noch `FBWorld` in der Signatur und hält keines von beidem
als Member.

#### 1.2 Die vier Dateien

```
$ cd sim && grep -rn "FBUnitRegistry.h\|\.Units()\|->Units()" src/systems src/modules
src/systems/FBDatalinkSystem.cpp:5:  #include "FBUnitRegistry.h"
src/systems/FBDatalinkSystem.cpp:36:   for (const FBUnit *u : net.Units()) {
src/systems/FBRadarSystem.cpp:5:     #include "FBUnitRegistry.h"
src/systems/FBRadarSystem.cpp:89:     for (const FBUnit *u : net.Units()) {
src/systems/FBRwrSystem.cpp:5:       #include "FBUnitRegistry.h"
src/systems/FBRwrSystem.cpp:71:       for (const FBUnit *u : net->Units()) {
src/modules/missile/FBMissileUplink.cpp:3:  #include "FBUnitRegistry.h"
src/modules/missile/FBMissileUplink.cpp:20:  for (const FBUnit *u : net->Units()) {
```

Vier Treffer, mehr darf es nicht geben. Sonst erscheint `FBUnitRegistry` in `systems/`/`modules/` nur
als Vorwärtsdeklaration oder als durchgereichter Parameter (`FBModule::Run` → `FBF16Module::Run` →
Sensor-Slot). **Wer diese Prüfung durch einen fünften Treffer verletzt, reißt die Architektur ein —
nicht eine Konvention.**

| Datei | Warum sie die Registry sehen darf |
|---|---|
| `FBDatalinkSystem.cpp` | liest publizierte PPLI-Aussendungen (Sende-Bit + Pose) — kooperativ |
| `FBRadarSystem.cpp` | testet Posen gegen ein Scanvolumen — aktives Echo |
| `FBRwrSystem.cpp` | liest ausschließlich publizierte Emissions-Signaturen — passiv |
| `FBMissileUplink.cpp` | hört die publizierte Lenkfunk-Aussendung SEINES Schützen ab |

**Warum die vierte dazugehört.** Ein Flugkörper ist strukturell eine Einheit wie jede andere; seine
Comms-Slot-Ableitung empfängt den Mittelphasen-Uplink. Ein Uplink ist eine EMISSION
(`FBUnitSignature::Uplink`), kein Zugriff auf den Privatzustand des Schützen: der Empfänger sucht die
EINE Einheit mit `GetId() == LauncherId_`, nimmt `GetSignature().Uplink` per Wert und liest von dieser
Einheit sonst nichts — und über das ZIEL gar nichts. Der Inhalt ist die Radarschätzung des Schützen
**mit dessen Fehler und dessen Alter** (`ReportTimeS` = der Look des SCHÜTZEN, nicht der
Empfangszeitpunkt). Fällt der Lock des Schützen, wird `Uplink.Active` falsch, der Empfänger publiziert
nichts mehr und der Block wird `Invalid` — die Lenkung sieht das Alter wachsen und fällt auf inertial
zurück. Kein Fehlerpfad, kein Sonderfall.

#### 1.3 Die zweite Hälfte der Grenze: der Kontakt selbst

Die Registry-Beschränkung allein genügt nicht — ein Sensor könnte weiterreichen, was er weiß. Deshalb
ist der Kontakttyp selbst die zweite Schranke:

| Typ | trägt Identität? | Begründung |
|---|---|---|
| `FBDatalinkTrack` | **ja** — `UnitId`, `Callsign`, `Team` | Eine Nachricht. Der Absender sendet seine eigene Kennung; Identität ist geschenkt. |
| `FBRadarContact` | **nein** — nur `TrackNum` + Geometrie | Ein Echo. Kein Feld für Id, Callsign, Team. Die Abwesenheit IST das Modell. |
| `FBRwrThreat` | **nein** — nur `Id` + Richtung | Eine gehörte Wellenform. `Kind` ist eine SCHÄTZUNG des Empfängers. |

`FBRadarSystem::Track` (privat) hält `UnitId` als Korrelationsschlüssel von Look zu Look — dieser
Schlüssel **verlässt das Objekt nie**. Dasselbe gilt für `FBRwrSystem::Threat::UnitId`. Die
veröffentlichten Nummern (`TrackNum`, `FBRwrThreat::Id`) sind sensoreigene Aktenzeichen ab 1, in
Erfassungsreihenfolge vergeben.

**IFF Mode 4 ist die einzige Identitätsquelle** und sie ist ZWEIWERTIG:

```
FBIffReply { NotInterrogated, NoReply, Friendly }     // es gibt keinen Wert "Hostile"
```

`FBRadarSystem::Interrogate` ist die einzige Zeile der Klasse, die `GetTeam()` liest, und sie wandelt
das Ergebnis sofort in eine Antwort um, die keinen Feind benennen kann:

```
validReply = Ziel-Transponder AN  UND  Ziel-Fraktion == eigene Fraktion (Krypto)
→ Friendly, sonst NoReply
```

Ein Feind mit eingeschaltetem Transponder und ein Freund mit ausgeschaltetem liefern **denselben**
`NoReply`. Wer schießen will, lebt damit — genau wie der Pilot des echten Jets. Ohne eigenen Abfrager
(`SetIffInterrogator(false)`) ist die Antwort `NotInterrogated`; Abfragerhythmus `kIffPeriodS = 5,0 s`
je fester Track (eine Abfrage ist eine Aussendung; jede Abfrage bei jedem Look wäre mehr Abstrahlung
als die echte Box hat).

#### 1.4 Was NICHT diese Grenze ist

- **`const FBWorld*`** steht getrennt neben der Registry: das ist die TERRAIN-Seite (Maskierung), nicht
  die Einheiten-Seite. Heute liest kein Sensor sie.
- **Der Snapshot-Vertrag** (`FBUnit::GetPose`/`GetSignature`) liefert immer den Stand des zuletzt
  ABGESCHLOSSENEN Ticks (Barriere `FBSimUnit::PublishPose`). Kein Sensor sieht je eine halb
  integrierte Pose oder einen Schalter, der mitten im Tick umgelegt wurde. Damit kann die
  Tick-Reihenfolge kein Sensorergebnis beeinflussen — die Voraussetzung für `fb-gym --threads`.
- **Die Registry-Reihenfolge ist Determinismus, keine Information.** Alle vier Systeme laufen die
  Registry IN ORDNUNG durch, also hängen Track-/Symbolnummern an der Deklarationsreihenfolge der
  Mission und nie daran, wer zuerst gehört/gesehen wurde.

---

### 2. Gemeinsame Struktur aller vier Systeme

| Eigenschaft | Ausprägung |
|---|---|
| Bauform | Interface + REALER Default in EINER Klasse; ein Modul überschreibt per Ableitung (kein leerer Ableitungs-Zwang für Zahlen) |
| Schreibrichtung | Sensoren SCHREIBEN genau EINEN `FBState`-Block, sonst nichts. Displays/Pilot LESEN |
| Gültigkeit | `FBBlockHeader` dreiwertig: `Invalid` (Box aus/ausgefallen → nicht „leeres Bild", sondern KEIN Bild), `Valid` (frisch publiziert), `Held` (eingefroren, Stempel nennt die letzte echte Aktualisierung) |
| Zeitbasis | **absolute** Sim-Zeit (`simTimeS` des Moduls). Netzzyklus, Antennen-Frame und Salven-Fahrplan laufen auf eigenen Rastern — das Ergebnis hängt NICHT daran, wie oft das Modul den Slot taktet |
| Kapazität | feste Arrays, keine Allokation im Tick (8 Tracks / 8 Kontakte / 8 Bedrohungen / 8 Chaff-Wolken) |
| Bedienung | ausschließlich über den Kommandobus (`core/FBAvionicsCommand.h`), also ablehnbar, mit Latenzklasse und Quittung |
| Schadenskopplung | ausgefallenes System wird gar nicht getaktet, sein Block wird `Invalid`; degradiert = eine ABLEITBARE Leistungsminderung |
| Beobachtbarkeit | jede Klasse ist `FBTelemetrySource` (`dl_*`, `fcr_*`, `rwr_*`, `cm_*`) + diskrete `FBLog`-Ereignisse |

**Taktung im F-16-Modul** (`FBF16Module::Run`):

| Slot | Rate | Begründung |
|---|---|---|
| Sensors (FCR) | 10 Hz | mit dem übrigen Anzeigen-/Feuerleitblock, dessen Leser er ist |
| Defensiv (RWR → Kommandos → CMDS) | 10 Hz | ein Programm-Burst-Intervall ist 0,1 s [DOC §2.2]; langsamer würde eine Salve quantisieren. Reihenfolge = Datenfluss: Empfänger schreibt, Kommandos werden beantwortet, Werfer liest |
| Comms (Datalink) | 5 Hz | der Netzzyklus ist 1 Hz; feiner ist ohne Nutzen |

**Health-Gate** (`core/FBSystemHealth`, nur lesend im Modul):

```
Fcr_->SetRangeFactor(SystemDegraded(Radar) ? kRadarRangeDegraded : 1.0);
if (SystemWorking(Radar)) Fcr_->Run(...); else SharedState.Radar.H.Invalidate();
```
identisch für `Rwr`, `Countermeasures`, `Datalink`. `kRadarRangeDegraded = 0,70710678` [DERIVED]:
halbe Antennenapertur, Radargleichung R⁴ ~ Pt·G² mit G ~ A, also R ~ √A → 1/√2.

---

### 3. Kooperativ — `FBDatalinkSystem`

#### 3.1 Was es ist

**Kein Sensor im Suchsinn.** Jedes Terminal sendet periodisch seine EIGENE Navigationslösung und seine
EIGENE Identität; jedes Terminal in Reichweite empfängt sie. Es gibt keine Suche, kein Scanvolumen und
kein Identifikationsproblem — Callsign und Fraktion reisen mit der Nachricht. Die Genauigkeit ist die
Navigationsgenauigkeit des ABSENDERS; was der Empfänger beisteuert, ist allein der Zeitpunkt, zu dem
er sie gehört hat.

#### 3.2 Konstanten

| Konstante | Wert | Herkunft |
|---|---|---|
| `kNetPeriodS` | 1,0 s | Link-16-PPLI: die Eigenpositionsmeldung eines Jägers liegt bei rund einer pro Sekunde [DOC] |
| `kDropAfterCycles` | 3,0 | Terminal hält einen Kontakt kurz, statt ihn bei einer verlorenen Nachricht auszublenden [SET, Verhaltensregel aus DOC] |
| `kGenericRangeNm` | 150 nm | ausdrücklicher PLATZHALTER („irgendein kooperatives Netz"), keine echte Terminalzahl |
| `FBF16Datalink::kMidsRangeNm` | 300 nm | MIDS-LVT/Link-16 Luft-Luft-LOS-Reichweite [DOC datalink-iff.md] |

#### 3.3 Der Netzzyklus (`Cycle`)

Pro Zyklus wird das GANZE Bild neu gebaut, Registry in Reihenfolge:

1. **Fraktionsfilter** — `u->GetTeam() != SelfTeam_` → übersprungen. Ein kooperatives Netz ist das
   eigene; ein Gegner kann dort niemals erscheinen.
2. **Nur `FBUnitKind::Aircraft`** — ein abgeworfener Store gehört zur selben Fraktion, trägt aber kein
   Terminal. Der Test steht VOR dem Ordinal-Zähler, weil `flightIndex` das Auswahlkriterium des
   FR/FL-Filters ist und eine Bombe sonst die Flight-Lead-Nummerierung verschieben würde.
3. `flightIndex++` — Ordinal innerhalb der Rotte, **einschließlich der eigenen Einheit**; Index 0 ist
   der Flight Lead.
4. **eigene PPLI überspringen** (`GetId() == SelfId_`) — nach dem Ordinal, nicht davor.
5. `AcceptContact(sender, flightIndex)` — der Override-Punkt (§3.6).
6. Kapazitätsgrenze 8, dann `break`.
7. **Gehört?** `sig.DatalinkXmt && rangeM <= min(MaxRangeM_, RadioHorizonM(eigene, fremde Höhe))`.
8. Gehört → frischer Track mit `ReportTimeS = simTimeS`. Nicht gehört, aber ein alter Track existiert
   und ist jünger als `3 × 1 s` → **unverändert übernommen** (Position und Zeitstempel bleiben stehen;
   das Alter läuft weiter hoch). Sonst fällt er.
9. Differenz alt/neu erzeugt `datalink TRACK_GAINED` / `TRACK_LOST`.

**Funkhorizont** (`RadioHorizonM`, statisch, protected):
`d[nm] = 1,23 · (√h₁[ft] + √h₂[ft])`, die 4/3-Erd-Sichtlinienregel, über beide Antennenhöhen summiert
[DERIVED]. Ein UHF-Netz hat keinen eigenen Weg über den Horizont. **Terrain-Maskierung entlang des
Pfades ist NICHT modelliert** — sie bräuchte das DEM entlang der Strecke.

#### 3.4 Zwischen den Zyklen

Nur das ALTER bewegt sich. Entfernung und Peilung werden gegen die EIGENE neue Position neu gerechnet
(das ist die Geometrie des eigenen Displays), **nie gegen eine neuere Absenderposition**. `AgeS` wird
auf ≥ 0 geklemmt (`ReportTimeS` ist `float`, `simTimeS` `double` — eine im selben Zyklus gestempelte
Nachricht kann sonst um Nanosekunden „in der Zukunft" liegen).

Blockkopf: `Publish` in einem Tick, in dem ein Zyklus lief, sonst `Hold`. Ein Track ist **nie „live"**:
selbst eine frische Nachricht beschreibt eine Pose, die an der letzten Tick-Barriere publiziert wurde.

#### 3.5 Die zwei Schalter

| Schalter | Methode | Wirkung |
|---|---|---|
| POWER | `SetPowered` | aus = **blind UND stumm**. Trackliste wird geleert, `TRACK_LOST` mit `reason=terminal off`, Block `Invalid` |
| XMT | `SetTransmit` | aus = **EMCON**: empfängt weiter das ganze Bild, wird nur von niemandem mehr geführt |

`Transmitting() == Powered_ && Transmit_` — was die Außenwelt sieht (`FBUnitSignature::DatalinkXmt`).
Ein stromloses Terminal kann nicht senden, wie auch immer XMT steht. **XMT als „empfängt auch nicht"
zu modellieren wäre in der Richtung falsch, auf die es ankommt:** EMCON heißt nicht gesehen werden,
nicht blind sein.

#### 3.6 `AcceptContact` — der Override-Punkt

```
virtual bool AcceptContact(const FBUnit &sender, int flightIndex) const;   // Default: alles
```
F-16 (`FBF16Datalink`, HSD-Kontaktfilter [DOC Part 13]): `fr` = alle Freundlichen (Default), `fl` =
nur `flightIndex == 0`, `off` = keine.

**Ehrlich vermerkt:** der Simulator hat kein Verbandskonzept — es gibt keine Element-/Rotten-Zuordnung,
aus der ein Lead ableitbar wäre. `fl` behält deshalb den ERSTEN Teilnehmer dieser Fraktion in
Missionsreihenfolge. Ein dokumentierter Platzhalter für eine Lead-Zuweisung, kein Modell davon.

#### 3.7 Die zweite Ableitung: `FBMissileUplink`

Dieselbe Basisklasse, `Run` vollständig überschrieben. Empfängt die EINE Aussendung des programmierten
Schützen und publiziert sie als `Tracks[0]` mit Callsign `"UPLINK"` und **ohne Unit-Id** — der Schütze
weiß selbst nicht, wen sein Radar sieht (§1.3), also kann die Waffe keine Identität lernen, die es nie
gab. Kein neuer Busblock, kein Rückkanal: die Lenkung liest das Ding wie jedes andere Instrument, und
der Blockkopf beantwortet die einzige Frage, die sie hat — sagt mir noch jemand etwas?

---

### 4. Aktiv — `FBRadarSystem`

#### 4.1 Das Scanvolumen IST der Modus

`FBRadarScanVolume` ist ein vollständiges Antennenmuster:

| Feld | Bedeutung |
|---|---|
| `AzCenterDeg` / `AzHalfDeg` | Mitte + Halbbreite **relativ zur NASE** (+ = rechts) |
| `ElCenterDeg` / `ElHalfDeg` | Mitte + Halbhöhe über/unter der Rumpfreferenzebene |
| `RangeM` | Entfernungstor |
| `FrameS` | Zeit für EINEN vollständigen Sweep |
| `AutoAcquire` | lockt den nächsten festen Track ohne Bedienung (die ACM-Eigenschaft) |
| `Active` | false = das Set strahlt nicht (ein OFF-Modus) |
| `SingleTarget` | alle Leistung auf EINEN Track: jedes andere Trackfile wird nicht mehr aufgefrischt und läuft aus |

Elevation als Mitte+Halb statt symmetrischem Halbwinkel, weil ein echtes Vertical-Scan-Muster **nicht**
um die Rumpfreferenzlinie zentriert ist (weit darüber, kaum darunter) und dasselbe Feld die
Cursorposition eines schwenkbaren Musters trägt.

Das Volumen ist KÖRPERFEST: die volle Roll/Nick/Gier-Rotation wird angewandt (`FBEnuToBodyLos` aus
`core/FBGeodesy.h` — dieselbe eine Definition, die auch die BFM-Steuerung invertiert). Das Volumen
kippt mit dem Jet; genau das lässt die HUD-bezogenen ACM-Boxen in der Kurve so wirken, wie der Pilot
es erwartet.

`RelativeLos` liefert je Ziel FÜNF Größen: Schrägentfernung, **welt**bezogene Peilung + Elevationswinkel
und **körper**bezogenes Az/El. Das Weltpaar ist die gemeldete Position (der Konsument müsste sonst
einen look-alten Körpervektor durch eine jetzt-aktuelle Lage zurückdrehen und würde die eigene
Rollbewegung in die Zielgeometrie schmieren); das Körperpaar ist die Größe der Antenne.

#### 4.2 `ActiveVolume()` — DER Override-Punkt

```
virtual const FBRadarScanVolume &ActiveVolume() const { return Search_; }
```

Ein ganzer Modus-Satz ist nichts als eine Auswahl unter Volumina, und **ein Lock ist nichts als ein
anderes Volumen**. Deshalb trägt EIN virtueller Getter eine komplette Feuerleitanlage. Daneben nur zwei
weitere Hooks: `ModeOrdinal()` (nur Telemetrie-Label) und `EmitterKind()` (was für eine Box hinter der
Antenne sitzt).

**F-16 — `FBF16Fcr`, AN/APG-68** (Taxonomie [DOC radar-sensors.md], **Winkel/Frames sind ein deklarierter
MODELLPARAMETERSATZ [SET]** — die Quelle zeigt MFD-Screenshots und nennt keine Zahlen):

| `fcr_mode` | Azimut | Elevation | Reichweite | Frame | Auto-Lock |
|---|---|---|---|---|---|
| `off` | — | — | — | (1,0 s, nie gesweept) | strahlt nicht |
| `crm` (Power-up) | ±60° | ±10,5° um `fcr_slew_el` | 40 nm | 4,0 s | **nein** |
| `acm_hud` | ±15° | ±10° | 10 nm | 1,0 s | ja |
| `acm_bore` | ±5° | ±5° | 10 nm | 0,3 s | ja |
| `acm_vert` | ±5° | +17° ± 30° = −13°…+47° | 10 nm | 1,2 s | ja |
| `acm_slew` | ±10° um Cursor | ±10° um Cursor | 10 nm | 0,8 s | ja |
| **STT (gelockt)** | ±60° (Gimbal) | ±60° (Gimbal) | 40 nm | 0,1 s | Single-Target |

Die Frame-Zeiten folgen den Volumina: ein mechanisch abtastendes Radar braucht für ein breiteres Muster
länger — der Boresight-Kegel erfasst in einem Bruchteil der HUD-Box-Zeit, CRM ist das langsamste.
**Diese Relation, nicht die absoluten Sekunden, ist das Modell.**

`OFF` schlägt den Lock: das Set abschalten muss die Antenne stoppen, nicht sie durch einen OFF-Modus
starren lassen. Jeder andere Modus übergibt einen Lock an STT.
`fcr_slew_el` ist die **Antennenhöhen-Bedienung**, nicht nur der ACM-Cursor: auch CRMs Elevationsmitte
folgt ihr. Bei BVR-Entfernung deckt ±10,5° nur ein paar tausend Fuß ab — die Antenne auf das falsche
Höhenband zu stellen ist die klassische Art, an einem Ziel vorbeizufliegen, das man problemlos hätte
sehen können.

**Flugkörper — `FBMissileSeeker`**: dieselbe Klasse, andere Zahlen. `kFovHalfDeg = 10°` [SET] (kein
öffentlicher Wert für die AMRAAM), `kGimbalHalfDeg = 45°` [SET] (mechanische Grenze ≠ momentanes
Sichtfeld — ohne die Unterscheidung verlöre ein gelockter Sucher sein Ziel bei 10° Ablage; im ersten
geflogenen Lauf gemessen: Drop + Reacquire bei 25° Ablage, 3 s vor Einschlag), `kFrameS = 0,05 s` [SET]
(ein Starren mit 20 Looks/s), `AutoAcquire + SingleTarget`, **IFF-Abfrager AUS** (eine Waffe trägt
keinen), Volumenmitte = die vom Uplink gemeldete Richtung („SLAVE"; beide Winkel 0 = BORE).

#### 4.3 Kontaktaufbau und -verlust

| Regel | Wert | Konsequenz |
|---|---|---|
| `kHitsToFirm` | 2 aufeinanderfolgende Looks | Erfassung kostet **Zeit** (`kHitsToFirm × FrameS`); ein Ziel, das durch die Keule blitzt, wird nie gemeldet |
| Trefferserie gebrochen, aber noch nicht fest | Eintrag wird **gelöscht** | nur ein FESTER Track verdient Coast |
| `Hits` bei ausgelassenem Look | auf 0 zurück | Re-Firming braucht wieder 2 frische Looks |
| Coast-Dauer | `max(kMinCoastS 1,0 s, kCoastFrames 3,0 × FrameS)` | Frames für einen Suchsweep, SEKUNDEN für ein Starren |
| Coast-Zustand | Geometrie **eingefroren**, `LookAgeS` läuft hoch, `Coasting = LookAgeS > FrameS` | der Verlust ist ein PROZESS, kein Ereignis |

Die Sekunden-Untergrenze ist die eigentliche Setzung: ein STT mit 0,1-s-Frame würde sonst 0,3 s nach
der Gimbal-Grenze fallen, während ein echtes Trackfilter ein verlorenes Ziel etwa eine Sekunde
extrapoliert — unabhängig davon, wie oft die Antenne es getroffen hat.

`ClosureMs` wird aus zwei aufeinanderfolgenden Looks differenziert (`PrevRangeM`/`PrevLookS`). Ein
Puls-Doppler-Set misst sie direkt; die Differenz über das Beobachtbare ist dieselbe Größe und braucht
keine Annahme über eine Vertikalrate, die der Snapshot nicht publiziert.

Weitere Regeln des Sweeps (`ScanFrame`):
- eigenes Echo übersprungen; **nur `FBUnitKind::Aircraft`** — ein Luft-Luft-Set sucht keine Bomben, und
  eine als Kontakt zu malen wäre Erfindung statt Simulation;
- Trackdatei voll (8) → nichts wird verdrängt;
- `SingleTarget && Locked` → jede andere Einheit wird in diesem Frame **gar nicht erst angeschaut**;
- nicht strahlend oder keine Registry → `DropAllTracks`, Block `Invalid` („nicht schauen" ≠ „nichts
  gefunden").

#### 4.4 Das Frame-Raster

Eigenes absolutes Raster (`NextScanS_`), unabhängig davon, wie oft das Modul den Slot taktet.
`ActiveVolume()` wird **in jeder Schleifeniteration neu gelesen**, weil ein erworbener Lock das Muster
und damit die Frame-Zeit ändert. Wächter `guard < 64` begrenzt das Nachholen eines pathologischen
`FrameS` und **resynchronisiert** danach, statt weiter zurückzufallen. `SetPowered(false)` setzt das
Raster zurück — ein wieder hochgefahrenes Set startet einen frischen Frame, kein abgestandenes.

#### 4.5 `Designate()` — der Piloten-Lock

Die ACM-Modi locken selbst, weil in einem Kurvenkampf niemand ein Radar bedient. Ein BVR-Suchmodus tut
das Gegenteil: er findet alles und lockt nichts, und WELCHER Rückstrahler zum Single-Target-Track wird,
ist eine Entscheidung **mit Preis** — die Keule warnt genau den, auf den sie zeigt. Deshalb braucht sie
ein Verb.

```
bool Designate(int trackNum, double simTimeS);   // Wert = die VERÖFFENTLICHTE Track-Nummer
                                                 // 0 = lösen (TMS rückwärts)
```
- `trackNum` ist das anonyme Handle vom Bus, **nie eine Unit-Id**.
- keine passende feste Trackakte → `false` (das Modul quittiert `out_of_context`: der Rückstrahler war
  weg, bis die Hand fertig war).
- Ereignisse getrennt: `RADAR_DESIGNATE`/`RADAR_BREAK` (eine Entscheidung) vs. `RADAR_LOCK` (ein
  Automatismus). Auf dem Scope sehen sie gleich aus, im Debriefing sind sie Gegensätze.

**Frame-Raster-Neusetzung, und warum sie nötig ist.** `Designate` setzt `NextScanS_ = simTimeS`. Das
Locken ERSETZT das Muster (ein 4-s-Suchsweep wird ein 0,1-s-Starren), aber das Raster trüge noch die
nächste Sweepzeit des Suchmusters. **Gemessen ohne diese Zeile: vier Sekunden eingefrorener Lock, in
denen die fusionierte Zielgeschwindigkeit auf null stand — ein in diesem Fenster abgefeuerter Schuss
wurde mit einem stehenden Ziel programmiert.**

**Rückfall bei Lock-Verlust.** `UpdateLock` löscht einen verlorenen Lock; war er DESIGNIERT
(`Designated_`), kehrt das Set in die SUCHE zurück und greift sich **nicht** den nächsten Kontakt. Das
Auto-Reacquire gehört den ACM-Modi, die es verlangt haben. Auto-Lock wählt den **nächstgelegenen**
festen Track („der erste" wird als „der nächste" gelesen, weil die momentane Keulenposition innerhalb
des Sweeps nicht modelliert ist und die Alternative ein willkürlicher Registry-Tiebreak wäre). Ein
Moduswechsel auf ein Muster ohne `AutoAcquire` verwirft den Lock (`reason=mode change`).

#### 4.6 `Emission()` — was das Set abstrahlt

Ein Radar ist kein passiver Beobachter; es kündigt sich an, und die Ankündigung ist genau das, was ein
fremder Warnempfänger hört. `Emission()` wird **aus dem gerade geflogenen Muster abgeleitet** — deshalb
**können Abstrahlung und Antennenzustand nicht auseinanderlaufen**, und kein Modul muss daran denken,
beides synchron zu halten.

| Antennenzustand | publizierte Keule | Bedeutung |
|---|---|---|
| nicht bestromt ODER `!Active` | `Mode::None` | strahlt nicht |
| suchend | das GANZE Scanvolumen (Mitte/Halbwinkel des Musters) | der Strahl streicht einmal je Frame über alles darin — „jemand sucht" |
| `SingleTarget && Locked` | **Bleistift** `kTrackBeamHalfDeg = 3,0°` [SET] auf Az/El des gelockten Tracks | nur dieses eine Flugzeug hört es — „er hat MICH" |

`RangeM` der Emission = `GateRangeM(v)` = Musterreichweite × `RangeFactor_`. Ein beschädigtes Set kann
also nicht in einem Codepfad weiter sehen als in einem anderen, und was es SIEHT und was es ANKÜNDIGT
bleibt eine Sache. Die Entfernungsgrenze ist zugleich der einzige Leistungsindikator, den die Signatur
trägt (statt Sendeleistung/Frequenz).

**Der Lenkungsfall wird NICHT hier gebildet.** `FBSimUnit::PublishPose` hebt `Track` auf `Guidance`,
wenn gleichzeitig `Stores().Uplink().Active` gilt: ob dieser Jet gerade einen Schuss stützt, ist Wissen
des SMS und nicht des Radars. Beide Emissionen werden an der Barriere kombiniert, die ohnehin beide
publiziert — **keines der zwei Systeme lernt vom anderen**. `FBSimUnit::Retire` leert die Signatur: ein
detonierter Flugkörper strahlt nicht ewig weiter.

#### 4.7 Täuschbarkeit — `SelectDecoy` + Doppler-Notch

Das GANZE Gegenmaßnahmen-Modell auf der Radarseite, und es beruht auf einer einzigen Bedingung.

Eine Chaff-Wolke ist ein großer Rückstrahler **ohne Eigengeschwindigkeit**. Ein Puls-Doppler-Set trennt
Ziele vom Clutter über die RADIALGESCHWINDIGKEIT: alles, was sich mit der Luftmasse bewegt
(Bodenrückstreuung, und eine Wolke, die binnen einer Sekunde die Flugzeuggeschwindigkeit verliert),
liegt in EINEM Filterbin, und dieses Bin verwirft der Prozessor. Ein Ziel ist von Chaff also **genau
solange unterscheidbar, wie seine eigene Radialgeschwindigkeit außerhalb des Bins liegt** — und drin
liegt sie, wenn es quer zur Sichtlinie fliegt. Daher: **Chaff ohne Beam-Manöver bewirkt nichts, ein
Beam-Manöver ohne Chaff auch fast nichts.**

| Konstante | Wert | Herleitung |
|---|---|---|
| `kDopplerNotchMs` | 40 m/s (~78 kt) [SET] | Größenordnung der Halbbreite eines Hauptkeulen-Clutterfilters: breit genug, dass gewöhnliche Kreuzgeometrie nicht versehentlich notcht, schmal genug, dass es ein bewusstes Beam-Manöver braucht |
| `kDopplerDwellS` | 0,2 s [SET] | eine Doppler-Geschwindigkeit ist eine INTEGRATION über ein kohärentes Verarbeitungsintervall, keine Differenz zweier Momentanpositionen — und hier muss sie es sein (s.u.) |

**Warum das Dwell zwingend ist.** Die Posen anderer Einheiten werden einmal je Tick-Barriere publiziert
(10 Hz), während ein Sucher mit 20 Hz schaut. Die Differenz zweier aufeinanderfolgender Looks
alterniert deshalb zwischen „nur ich habe mich bewegt" und „wir beide" und liest ein sich näherndes
Ziel jeden zweiten Look als stehend — **gemessen: 446 m/s gegen wahre 654 m/s, also ein head-on Ziel
INNERHALB des Notch**. Zwei Zehntelsekunden überspannen mindestens zwei Barrieren; die Messung ist dann
die echte Radialgeschwindigkeit, egal wie die beiden Raten stehen. Die Dwell-Messung wird **getrennt**
von `FBRadarContact::ClosureMs` geführt (das bleibt die look-zu-look-Zahl, unverändert).

**Gemessen wird an EIGENEN Größen, nie an der Wahrheit des Ziels:**

```
tgtRadialMs = OwnClosureOn(Sichtlinie)            // eigene Geschwindigkeit auf die Sichtlinie projiziert
            − measuredClosureMs                    // aus den eigenen Look-Entfernungen differenziert
Notch getroffen  ⟺  |tgtRadialMs| < kDopplerNotchMs
```
`OwnClosureOn` ist die CLUTTER-Doppler: was ein STEHENDER Punkt in dieser Richtung schließen würde.

Ablauf je Look (`ScanFrame`):
1. Die Doppler-Entscheidung fällt **VOR** dem Volumentest — ein verführter Look misst die WOLKE, und
   dann muss die WOLKE in der Keule liegen.
2. Nur ein Track mit vorhandenem Look-Paar kann getäuscht werden; eine Erstentdeckung hat keine
   Annäherungsrate zum Testen.
3. **Klebrigkeit:** war der Track im letzten Look verführt, wird weiter nach einer Wolke gesucht, ohne
   den Notch-Test zu wiederholen. Grund: hat sich das Verfolgungstor einmal in den Clutterfilter
   gesetzt, bleibt es dort, solange dort ein Echo ist (eine Wolke ist per Konstruktion im Notch). Ohne
   die Klebrigkeit kippte der Test bei jedem Look — die ersetzte Messung macht die NÄCHSTE
   Annäherungsrate zu einem Sprung, der als weit außerhalb des Notch liest (**gemessen:
   Seduce/Resolve im 20-Hz-Takt des Suchers alternierend**). Die Verführung endet, wenn die Wolken
   enden.
4. `SelectDecoy` testet **jede** Wolke der publizierten Signatur gegen **dasselbe** Volumen wie das
   Flugzeug (Entfernungstor + Az/El-Fenster) und nimmt die stärkste: Leistung = `RCS / r⁴`
   (Zweiweg-Radargleichung), RCS aus der Alterskurve `FBChaffRcsNorm`. **Deterministisch — kein Wurf,
   keine Wahrscheinlichkeit.**
5. Bei Übergang: `radar CHAFF_SEDUCED` / `CHAFF_RESOLVED` mit BEIDEN Messgrößen, dem Notch, dem
   Wolkenalter und dem Versatz Wolke↔Flugzeug — die Entscheidung ist im Log rekonstruierbar.

Die Wolken kommen aus der Signatur der EINHEIT, die gerade angeschaut wird (`sig.Chaff`). Folge, die im
Code ausdrücklich steht: **eine Wolke kann nur ein Radar täuschen, das auf das Flugzeug schaut, das sie
geworfen hat** — nie eines, das jemand anderen in der Nähe verfolgt.

#### 4.8 Bewusste Nicht-Modellierung: Terrain-Maskierung

Ein echtes Look-down-Bild wird vom Horizont und vom Gelände entlang der Sichtlinie beschnitten. Diese
Klasse tut **keines von beidem**: sie nimmt kein `FBWorld` entgegen und tastet kein DEM ab.

Begründung, wie sie im Header steht: für Luft-Luft zwischen zwei fliegenden Einheiten — der Trainings-
kampf, für den dieser Sensor existiert — ist die Sichtlinie frei, und Scanvolumen plus Entfernungstor
entscheiden bereits alles. Maskierung bräuchte einen DEM-Raymarch **je Kontakt und je Look** und gehört
demjenigen System, das zuerst einen Grund hat, diesen Preis zu zahlen. **Hier wird nichts still
angenähert**, damit niemand das Bild für eines halten kann, das Maskierung hat. (Dieselbe Aussage macht
`FBDatalinkSystem::RadioHorizonM` für den Funkpfad.)

---

### 5. Passiv — `FBRwrSystem`

#### 5.1 Das Spiegelbild

Das Radar fragt „was ist da draußen"; der RWR fragt „wer schaut mich an" — und beantwortet das auf die
einzig ehrliche Weise: er hört, was andere Einheiten ABSTRAHLEN (`FBUnitSignature::Radar`), und prüft
zwei Geometrien. Er ist **kein Bedrohungsorakel**: er meldet nie, ob das fremde Radar dieses Flugzeug
tatsächlich sieht, und nie, ob es dieses oder ein anderes Flugzeug auf derselben Peilung verfolgt
[DOC defence-rwr-cm.md §2.1].

Die eigene Fraktion wird gespeichert und **absichtlich nie gelesen**: ein Warnempfänger hört eine
Wellenform, keine Zugehörigkeit. Ein befreundetes Radar, das einen verfolgt, erzeugt exakt dasselbe
Symbol wie ein feindliches.

#### 5.2 Die zwei geprüften Geometrien

**(1) Trifft die Keule des Senders?** — `BeamCovers`, gerechnet AM SENDER:

```
FBEnuToBodyLos(Sender-Roll/Pitch/Yaw,  Vektor Sender→hier)  →  az, el
innerhalb  ⟺  |wrap180(az − sig.AzCenterDeg)| ≤ sig.AzHalfDeg  ∧  |el − sig.ElCenterDeg| ≤ sig.ElHalfDeg
```

Dieselbe Transformation, dieselbe Konvention, dieselbe Datei, die das Radar für seine EIGENE Erfassung
benutzt — **die zwei Seiten einer Bestrahlung können sich über die Geometrie nie uneinig sein.** Ein
suchendes Radar bestrahlt daher alles in seinem Volumen; ein verfolgendes GENAU EIN Flugzeug (deshalb
ist eine Verfolgungswarnung eine persönliche).

**(2) Kann die eigene Antenne aus dieser Richtung hören?** — `ElevCoverageDeg()`:

```
FBEnuToBodyLos(eigene Lage, Vektor hier→Sender)  →  rxAz, rxEl
gehört  ⟺  |rxEl| ≤ ElevCoverageDeg()
```

360° Azimut, aber begrenzte Elevation. Generischer Default 60°; **F-16 `FBF16Rwr::kElevCoverageDeg =
45°** [DOC §2.1: vier Hochband-Quadrantenantennen + Tiefband-Doppelblatt geben 360° Azimut, aber nur
±45° Elevation]. Das ist eine echte **BLINDZONE über und unter der Rumpfachse, die das eigene
Manövrieren aufreißt** — eine bereits bestehende Lock- oder Startwarnung verschwindet dabei STILL,
ohne dass der Sender irgendetwas getan hätte. Geloggt wird nur der ÜBERGANG (`rwr THREAT_BLIND`), nicht
jeder Tick: was eine Zeile wert ist, ist der Moment, in dem eine bestehende Warnung unhörbar wurde.

#### 5.3 Keine Entfernung — und warum

Ein RWR misst Peilung und EMPFANGSLEISTUNG. Er kann keine Entfernung messen, weil er nie etwas gesendet
hat, dessen Rücklauf er stoppen könnte. `FBRwrThreat` trägt deshalb **keine Meter**, sondern:

| Feld | Bedeutung |
|---|---|
| `BearingDeg` | RELATIV zur eigenen Nase, −180…180 (die TWA ist eine Relativpeilungs-Anzeige) |
| `ElDeg` | Einfallselevation, körperfest — auf dem echten azimutalen Scope nicht dargestellt, aber die Größe, an der die Antennenabdeckung entschieden wird, also publiziert statt versteckt |
| `SignalNorm` | Empfangsleistung 0..1 — die EINE Näherungsandeutung, die die Box hat |
| `LethalityNorm` | 0..1, Radialposition auf dem Scope (1 = Mitte) [DOC §2.1: der Abstand vom Zentrum ist relative LETHALITÄT, nicht Entfernung] |

Damit kann nichts stromabwärts versehentlich eine Entfernungslösung von einem Warnempfänger fliegen.

**Ringlage** [SET, Schema aus §2.1 verbatim]: `Search 0,20` / `Track 0,55` / `Missile 0,85`, plus
`kLethalitySignalWeight 0,15 × SignalNorm`, auf 1 geklemmt — der Modus wählt den Ring, die Leistung
bewegt das Symbol innerhalb seines Rings.

#### 5.4 Hörweite: Einweg gegen Zweiweg

```
hearM = sig.Radar.RangeM × kBeamRangeFactor           // kBeamRangeFactor = 2,0  [SET, DERIVED]
SignalNorm = 1 − (rangeM / hearM)²                     // Einwegausbreitung: Leistung ~ 1/r²
```

Herleitung (Header): der Sender braucht Hin- UND Rückweg, sein Echo fällt mit 1/r⁴ ab; der Empfänger
sitzt nur in der Hinhälfte, hört also mit 1/r². Bei gleicher Empfängerempfindlichkeit ist die
Warnreichweite damit ein VIELFACHES der Erfassungsreichweite des Senders, kein Bruchteil. Die
öffentliche Literatur nennt je nach Empfänger 1,5 bis 3; 2,0 ist die Mitte und hat die Eigenschaft, die
die Mission messen kann: **man wird gewarnt, bevor man erfasst wird.** Die Zahl `hearM` verlässt die
Klasse nie — publiziert wird die Leistung, nie die Entfernung dahinter.

#### 5.5 Modus, Halten, Rangfolge, Deckel

| Regel | Wert / Verhalten |
|---|---|
| `ModeOf` | `Kind == MissileSeeker` → **Missile** (unabhängig davon, wie er scannt); `EmitterMode::Guidance` → Missile; `Track` → Track; sonst Search |
| `kHoldS` | 2,0 s [SET] — eine wegstreichende Keule und ein abgeschalteter Sender sehen im ersten Moment gleich aus; das Symbol wird nicht beim ersten Ausbleiben ausgeblendet. In SEKUNDEN, weil dieser Empfänger kein eigenes Frame hat (er hört durchgehend) |
| `kNewThreatS` | 1,0 s [SET] — der Ersatz für den Erkennungston: „neu" ist ein publizierter Zustand mit Lebensdauer, lang genug für einen 10-Hz-Konsumenten, kurz genug, um nicht ins Standbild zu verschwimmen |
| Rangfolge | erst MODUS (Enum-Ordnung `Search < Track < Missile`), dann Empfangsleistung, Tiebreak = Tabellenreihenfolge (stabile Insertion) → das Prioritätssymbol flackert nicht zwischen zwei gleichwertigen Bedrohungen |
| `SearchShown` | SEARCH-Filter; versteckte Suchemitter setzen `HiddenSearch` — **Unterdrückung muss von Abwesenheit unterscheidbar bleiben** [DOC §2.1] |
| `MaxDisplayed()` | Anzeige-Deckel ÜBER einer weiterlaufenden Erkennung. F-16: PRIORITY 5 / OPEN 16 [DOC §2.1] |
| Erkennungstabelle | `kMaxRwrThreats = 8`, voll = nichts wird verdrängt |
| stromlos | Block `Invalid`, Tabelle geleert — „nicht zuhören" ist nicht „nichts da draußen" |

`FBRwrBlock` trägt zusätzlich `MissileLaunch` (irgendeine Bedrohung im Missile-Modus → das LAUNCH-
Licht) und `Activity` (irgendeine nicht-suchende → die ACT-Hälfte der ACT/PWR-Anzeige).

Ereignisse: `rwr THREAT_NEW` / `THREAT_MODE` / `THREAT_BLIND` / `THREAT_DROP`.

**Nicht hier: die Bedrohungsbibliothek.** `Classify()` gibt heute die Emitter-Klasse durch (die
Bibliothek ist einen Eintrag tief und deshalb immer richtig). Das Feld existiert, damit an dem Tag, an
dem sie es nicht mehr ist, kein Konsument seine Form ändern muss. Die ALIC-/Symbolcode-Tabelle aus
Appendix B ist bewusst nicht übernommen: die Quelle beschreibt ihre Struktur, transkribiert sie nicht,
und Symbolcodes zu erfinden hieße genau das zu raten, was ein RWR nicht raten darf.

---

### 6. Aktiv-defensiv — `FBCountermeasureSystem`

#### 6.1 Ein Programm ist DATEN

Das Parameterschema ist das des AN/ALE-47, Feld für Feld und Bereich für Bereich [DOC §2.2], je
Typ (Chaff/Fackel):

| Feld | Bereich | Bedeutung |
|---|---|---|
| `BurstQty` (BQ) | 0..99 | Patronen in EINER Salve |
| `BurstIntervalS` (BI) | 0,020..10,000 s | Zeit zwischen Patronen innerhalb einer Salve |
| `SalvoQty` (SQ) | 0..99 | Salven im Programm |
| `SalvoIntervalS` (SI) | 0,50..150,00 s | Zeit zwischen Salven |

`BQ` **oder** `SQ` auf 0 nimmt den Typ aus dem Programm — so wird ein reines Chaff- oder Fackelprogramm
ausgedrückt (eine Regel der echten DED-Seite, reproduziert statt durch ein „Typ"-Flag ersetzt). Ein
Programm ist damit Missions-/Zuladungsdaten, kein Verhalten.

`FBCountermeasureSystem` ist die Maschine, die eines ABSPIELT: Patrone auswerfen, `BI` warten, nächste,
`SI` warten, aufhören wenn Salven oder Magazin enden. Der Fahrplan ist **absolutzeitgesteuert** mit
Nachhol-Wächter (`guard < 128`) — ein Burst-Intervall unter der Slot-Taktung wirft trotzdem die
richtige Zahl Patronen zu den richtigen Zeiten, statt auf die Tickrate gedehnt zu werden.

**F-16 — `FBF16Cmds`** (Schema [DOC], **Werte [SET]**, Aufgabe je Programm im Header benannt):

| PRGM | Chaff | Fackeln | Aufgabe |
|---|---|---|---|
| 1 BREAK LOCK | 2 × 0,10 s, 2 Salven à 1,00 s (= 4 Patronen in ~1,1 s) | — | die dichte Reflexantwort; was AUTO gegen eine RAKETE wirft |
| 2 MIXED | 2 × 0,10 s, 2 Salven à 2,00 s | 1, 2 Salven | unbekannte Bedrohung — auf Kosten zweier Magazine |
| 3 FLARE | — | 2 × 0,10 s, 4 Salven à 1,00 s | nur IR (s. §6.5) |
| 4 SUSTAINED | 2 × 0,10 s, 4 Salven à 4,00 s (= 8 über ~12 s) | — | gegen einen bloßen TRACK; was AUTO wiederholt. Langsam genug, ein 60er-Magazin nicht vor der Entscheidung zu leeren; dicht genug, dass immer eine Wolke innerhalb `kChaffLifeS` steht |
| 5 SLAP | 1 | 1 | der Wandknopf (immer erreichbar) |
| 6 BYPASS | 1 | 1 | die dokumentierte Notausgabe [DOC §2.2] |

Magazin 60/60, kombiniertes Maximum 120 [DOC §1]; BINGO 10/10 [SET].

#### 6.2 Der Modus-Knopf als Zustandsmaschine

| Modus | Wer darf werfen | Zustimmung | Sonstiges |
|---|---|---|---|
| `off` | niemand | — | Block `Invalid`, Status `NoGo`; laufendes Programm gestoppt |
| `stby` | niemand | — | **der einzige Modus, in dem umprogrammiert werden darf** |
| `man` | CMS vorwärts wirft das PRGM-Programm | — | nichts automatisch |
| `semi` | das System WÄHLT das Programm | **je Abwurf** (CMS aft); Prompt kehrt nach jedem Programm zurück | Status `DispenseReady`, solange der Prompt steht |
| `auto` | das System wählt UND wiederholt | **einmal je Moduswechsel** (Drehen auf AUTO erteilt sie); CMS rechts widerruft und **unterbricht das laufende Programm** | — |
| `byp` | genau 1 Chaff + 1 Fackel | — | überschreibt die PRGM-Wahl vollständig |

SEMI und AUTO sind bewusst zwei Zustände und kein Flag: Zustimmung pro Abwurf gegen Zustimmung pro
Moduseintritt sind verschiedene Maschinen, und die Quelle sagt das ausdrücklich.

`SetMode` setzt `Consent_ = (m == Auto)` [DOC §2.2: „Zustimmung gilt als erteilt, sobald der Knopf auf
AUTO steht"] und stoppt bei OFF/STBY ein laufendes Programm. `SetConsent(false)` = CMS rechts →
`StopProgram("consent revoked")`.

**Zwei Programmierpfade, ein Gate:** `SetProgram` (der DED-Pfad) verlangt STBY; `InstallProgram`
(protected) ist der Modul-Konstruktor — die Bodencrew — und ist ungegated.

#### 6.3 Auslösung nur über den Kommandobus

`Dispense(program, nowS, outcome, reason)` wird **nie direkt gerufen** (dieselbe Regel wie beim Pickle).
Ablehnungskatalog:

| Bedingung | Outcome / Reason |
|---|---|
| Modus OFF/STBY | `Rejected` / `HardwarePrecedence` (der Knopf sperrt den Werfer physisch aus) |
| Programmnummer außerhalb 1..6 | `Rejected` / `OutOfRange` |
| Programm mit beiden Typen genullt | `Rejected` / `OutOfContext` |
| kein Vorrat für die im Programm enthaltenen Typen | `Rejected` / `Depleted` |
| ausgefallene Box | `Rejected` / `SystemFailed` (Modulebene, gilt für jedes System) |

Bus-Ziele: `CmDispense` (CMS vorwärts, HOTAS-Klasse — die eine Bedienhandlung, die IN einem Manöver
stattfindet; Wert 0 = PRGM-Programm, 1..6 = direkt), `CmConsent` (CMS aft/rechts, HOTAS),
`CmdsMode` (Knopf am Linkskonsol → **DED-Klasse**: eine Hand vom Gashebel, Kopf nach unten).

#### 6.4 In SEMI/AUTO triggert sie auf die WARNUNG, nicht auf die Wahrheit

```
threatened = state.Rwr.H.Readable() && ThreatCount > 0 && Threats[0].Mode != Search
```

Das ist die Kernaussage der Klasse. Ausgelöst wird auf den **RWR-Block** — also auf das, was der Jet
WEISS. Was in der Blindzone des Empfängers steht (§5.2), wird nicht beantwortet: genau die
zusammenwirkende Verwundbarkeit, die die Quelle beschreibt. Kein Pfad von hier erreicht die Welt, die
Registry oder eine andere Einheit.

Weitere Regeln von `ServiceAutomatic`:
- läuft bereits ein Programm → kein Neustart;
- **Chaff-BINGO** (`Chaff_ <= BingoChaff_`) unterdrückt **automatische** Abwürfe (und nur die) — was
  übrig ist, gehört dem Piloten für einen Schuss, den er selbst beantworten will [DOC §2.2];
- SEMI ohne Zustimmung → `AwaitingConsent_` (Status `DispenseReady`), sonst Start; nach dem Start wird
  die Zustimmung in SEMI wieder entzogen;
- Programmwahl: `AutomaticProgram(worst)` — der EINE protected Hook. Default-Doktrin [SET]: gegen eine
  Rakete das dichte Programm (1), gegen einen bloßen Track das sparsame wiederholende (4). Die Quelle
  sagt, dass das System „das passende Automatikprogramm je Bedrohung" wählt, und nie welches — die
  Zuordnung ist deshalb Doktrin eines Moduls, ausgedrückt als Hook statt in der generischen Schicht
  erfunden. `FBF16Cmds` überschreibt ihn NICHT: die Programmtabelle ist um genau diese Doktrin gebaut,
  und ein Override, der dieselben zwei Zahlen zurückgibt, wäre die verbotene leere Ableitung.

#### 6.5 Was ein Abwurf tatsächlich tut

**Chaff** → eine `FBChaffCloud` an der Position, an der das Flugzeug JETZT ist, mit `BloomS = simTimeS`.
Ring aus 8 Einträgen: die frischesten Patronen bleiben, ältere sind die zerstreuten und sind das
Richtige zum Verlieren. Die Wolke bewegt sich **nicht** (FlightBox hat kein Windfeld — „steht in der
Luftmasse" ist „steht"). Alterskurve `FBChaffRcsNorm` [SET, Begründung im Header]:

```
ageS < kChaffBloomS 0,3 s      → 0     (gepacktes Bündel, noch kein Reflektor)
0,3 s ≤ ageS < kChaffLifeS 8 s → linearer Abfall von 1 auf 0
ageS ≥ 8 s                     → 0     (zu dünn, um gegen ein Flugzeugecho zu bestehen)
```
**Kein Zufall, keine Ablösewahrscheinlichkeit.** Ob die Wolke WIRKT, entscheidet allein das gegnerische
Radar (§4.7) — diese Klasse erfährt es nicht, genau wie der Pilot.

Publiziert wird sie in `FBUnitSignature::Chaff` (Barriere), also unter demselben Vertrag wie Pose und
Sendeschalter: kein Radar sieht je eine halbe Salve.

**Fackeln** werden geworfen, gezählt und **wirken nicht** — und das steht so da, statt versteckt zu
werden: ein Infrarot-Täuschkörper braucht einen Infrarot-Sucher, den dieser Simulator nicht hat. Die
Bücher werden ehrlich geführt, damit an dem Tag, an dem eine AIM-9 existiert, hier nichts geändert
werden muss.

Ereignisse: `cmds PROGRAM_START` / `SALVO` / `PROGRAM_END` / `MAGAZINE_EMPTY`.

---

### 7. Die vier Systeme gegenübergestellt

| | **Datalink** (kooperativ) | **Radar/FCR** (aktiv) | **RWR** (passiv) | **CMDS** (aktiv-defensiv) |
|---|---|---|---|---|
| Klasse | `systems/FBDatalinkSystem` | `systems/FBRadarSystem` | `systems/FBRwrSystem` | `systems/FBCountermeasureSystem` |
| F-16 | `FBF16Datalink` (MIDS-LVT) | `FBF16Fcr` (APG-68) | `FBF16Rwr` (ALR-56M) | `FBF16Cmds` (ALE-47) |
| Frage | „wo sind meine Leute" | „was ist da draußen" | „wer schaut MICH an" | „was werfe ich" |
| **Sieht** | eigene Fraktion, nur Aircraft, nur sendende Absender, in min(Terminal, Funkhorizont) | Aircraft im Scanvolumen (Az×El körperfest) innerhalb des Entfernungstors | jede Einheit, deren Keule diesen Jet trifft UND deren Einfallswinkel die eigene Antenne abdeckt | nichts — liest den RWR-BLOCK des eigenen Busses |
| **Bekommt** | Id, Callsign, Team, Position, Kurs, Speed, Alter | **anonyme Geometrie**: Track-Nr., Range, Peilung, Elevationswinkel, Az/El, Closure, Look-Alter, IFF | Relativpeilung, Elevation, Signal, Lethality, Modus, geschätzte Emitter-Art, „neu" | — |
| **Gibt preis** | die eigene PPLI (Position + Identität), solange XMT an ist | die eigene Keule: Suchvolumen bzw. ±3°-Bleistift auf genau ein Ziel; dazu die IFF-Transponderantwort | **nichts** (rein passiv) | Chaff-Wolken hinter dem Flugzeug (in der Signatur publiziert) |
| **Alterung** | 1-Hz-Netzzyklus; Halten über 3 Zyklen; `AgeS` läuft hoch; Block `Held` | Antennen-Frame (0,1–4,0 s je Modus); Aufbau 2 Looks; Coast `max(1 s, 3 Frames)`; `LookAgeS`; Block `Held` | kontinuierlich; Halten 2 s nach letztem Hören; `AgeS`; „neu" 1 s | Programmfahrplan in Absolutzeit; Wolke: Blüte 0,3 s, Leben 8 s |
| **Latenz der Bedienung** | POWER/XMT/Filter/Range über Bus (DED-Klasse) | Modus/Range/Slew/IFF (DED), `Designate` (HOTAS) | POWER/Display/Search (DED) | `CmDispense`/`CmConsent` (HOTAS), `CmdsMode` (DED) |
| **Kann NICHT** | Gegner sehen; über den Funkhorizont; Terrain-Maskierung; Stores/Bodenziele führen | Identität liefern (außer IFF friendly/unbekannt); Bomben oder Bodenziele sehen; Terrain-Maskierung; mehr als 8 Trackfiles; in STT etwas anderes sehen | Entfernung messen; einen Sender außerhalb seiner Keule hören; außerhalb ±45° Elevation hören (F-16); benennen, WER da ist; Fraktion unterscheiden | wissen, ob es gewirkt hat; auf eine Bedrohung reagieren, die der RWR nicht hört; IR-Sucher täuschen (kein IR im Sim) |
| **Ausgefallen** | Block `Invalid` | Block `Invalid`, alle Tracks gedroppt | Block `Invalid`, Tabelle leer | Block `Invalid`, Status `NoGo` |
| **Degradiert** | — | Reichweite × 0,7071 [DERIVED] | — | — |
| Override-Punkt | `AcceptContact` | **`ActiveVolume()`** (+ `ModeOrdinal`, `EmitterKind`) | `Run` (+ `ElevCoverageDeg`, `MaxDisplayed`, `Classify`) | `Run` (+ `AutomaticProgram`) |
| Telemetrie | `dl_*` (5) | `fcr_*`, `iff_xpdr` (11) | `blk_rwr` + `rwr_*` (10) | `blk_cmds` + `cm_*` (11) |

---

### 8. Determinismus — was garantiert ist

- **Kein Zufall in der Wahrnehmungskette.** Kein Würfel, keine Ablösewahrscheinlichkeit, keine
  Detektionswahrscheinlichkeit. Chaff-Wirkung ist eine Ungleichung auf gemessenen Größen; die stärkere
  Wolke gewinnt über `RCS/r⁴`.
- **Keine Abhängigkeit von der Tick-Reihenfolge:** alle Cross-Unit-Lesezugriffe gehen über den
  Barriere-Snapshot.
- **Keine Abhängigkeit von der Slot-Taktung:** Netzzyklus, Antennen-Frame und Salvenfahrplan laufen auf
  absoluten Rastern mit Nachhol-Wächtern.
- **Keine Abhängigkeit von der Erfassungsreihenfolge:** Registry wird in Missions-Deklarationsreihenfolge
  gelaufen; Track- und Symbolnummern folgen daraus.
- **Keine Allokation im Tick:** alle vier Systeme arbeiten auf festen Arrays.
- Folge: `fb-gym --threads 1..N` liefert denselben Fingerabdruck.
