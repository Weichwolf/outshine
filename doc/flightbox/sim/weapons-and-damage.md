# FlightBox — Waffeneinsatz & Schaden

**Was diese Datei beschreibt:** die vollständige Kette von der Auslösung bis zur Systemfolge — wer eine
Waffe freigibt, wie sie fliegt, wer entscheidet, dass sie getroffen hat, was ein Treffer im
Gesundheitsregister anrichtet und wie sich das durch Avionik-Bus, HUD, Warnsatz, Kommandobus und
Flugphysik fortpflanzt.

**Quellen** (alles Primärquelle im Baum, nichts extern):

| Bereich | Dateien |
|---|---|
| Freigabepfade | `sim/src/systems/FBStoresSystem.{h,cpp}`, `sim/src/systems/FBGunSystem.{h,cpp}` |
| Ballistik | `sim/src/core/FBGunBallistics.{h,cpp}`, `sim/src/core/FBBallistics.{h,cpp}`, `sim/src/core/FBGunProjectiles.{h,cpp}` |
| Auflösung | `sim/src/app/FBMissionRunner.cpp` (`ClosestApproach`, `ResolveBurst`, `ResolveGunHit`, `ResolveGroundBurst`, `GroundCrossing`) |
| Schaden | `sim/src/core/FBDamageModel.{h,cpp}`, `sim/src/core/FBSystemHealth.{h,cpp}`, `sim/src/units/FBSimUnit.{h,cpp}` |
| Moduldaten | `sim/src/modules/f16/FBF16Damage.{h,cpp}`, `sim/src/modules/f16/FBF16Sms.h`, `sim/src/modules/f16/FBF16Gun.h`, `sim/src/modules/ground/FBGroundTarget.h` |
| Waffenmodule | `sim/src/modules/stores/`, `sim/src/modules/missile/`, `sim/src/modules/ground/` |
| Waffenmodell | `sim/assets/aircraft/aim120/` (`aim120.xml`, `engine/WPU-6.xml`) |
| Kataloge (TYPEN) | `sim/src/core/FBStore.h`, `sim/src/core/FBGun.h` — **als Typen dokumentiert in `core.md`**; hier steht ihr VERHALTEN |
| Missionsseite | `doc/mission-format.md` (Abschnitte „Waffen", „Die Bordkanone", „Luft-Boden-Angriff", „Bodenziele") |

**Kennzeichnung von Zahlen** (aus dem Quellcode übernommen): `[SET]` = FlightBox-Setzung ohne
zitierbare Quelle, Begründung genannt; `[DERIVED]` = aus einer genannten Zahl per genannter Formel
gerechnet; `[T3]`/`[T4]` = Konfidenzstufen aus `doc/f16/weapons.md`; ohne Marke = aus dem gepinnten
JSBSim-Modell selbst gelesen.

---

## 1. Die Grundentscheidung: eine abgefeuerte Waffe IST eine Einheit

Es gibt keinen Waffen-Sonderpfad. Ein Store, der die Station verlässt, wird ein
`units/FBSimUnit` — mit derselben Mechanik wie ein Jet:

| Eigenschaft | Jet | abgeworfener Store / Flugkörper | statisches Bodenziel |
|---|---|---|---|
| eigene `FBFdm`-Instanz | ja | ja (eigenes gepinntes Modell) | **nein** (kein Modell) |
| `FBModule` aus `FBModuleRegistry` | ja (`f16`) | ja (`mk82` / `aim120`) | ja (`target_soft`/`target_hard`) |
| eigene Telemetriedatei | ja | ja (`telemetry_<callsign>.csv`) | ja |
| `FBFlightMonitor` (Physik-Richter) | ja | ja | **nein** (Kind Ground) |
| `FBMissionMonitor` | wenn Ziele deklariert | nein | wenn Ziele deklariert |
| `FBSystemHealth`-Register | ja | ja | ja |
| Schadensmodell anwendbar | ja | ja (Layout leer → kein Schaden) | ja |
| in `FBUnitRegistry` | ja | ja | ja |
| Snapshot-Pose (`PublishPose`) | ja | ja | ja |

### Was `FBUnitKind` bewirkt — und was NICHT

`FBUnitKind` (`sim/src/units/FBUnit.h`) unterscheidet `Aircraft` / `Weapon` / `Ground`. Die
Unterscheidung existiert für genau die Dinge, die dem BESITZER der Simulation gehören:

| Regel | Aircraft | Weapon | Ground |
|---|---|---|---|
| physikalisches K.O. beendet den Lauf (`FirstFlightKo`) | **ja** | nein — es ist die Detonation | nein (fliegt nicht) |
| von Luft-Luft-Sensoren gesucht | ja | nein | nein |
| Kanonen-Bündel dagegen aufgelöst | ja | nein | **nein** (kein Strafing) |
| Näherungszünder dagegen aufgelöst | ja | nein | nein |
| Bodenburst dagegen aufgelöst | **nein** (s. §5.4) | nein | ja |
| im Roster für `objective kill …` | ja | **nein** | ja |
| von JSBSim getaktet | ja | ja | nein (`Airframe` optional) |
| Bodenhöhe für das FDM | echte Elevation | `kWeaponNoGroundElevM` = −100000 m | — |

Was die Unterscheidung NICHT bewirkt: sie ist kein zweiter Codepfad, kein Verhaltens-Flag und keine
Ausnahme im Tick. Ein Flugkörper wird von derselben Schleife gerechnet, von denselben Richtern
beurteilt und mit denselben Logzeilen belegt wie ein Jet; `UNIT_RESULT` nennt ihn `IMPACT` statt
`CRASH`, und das ist der ganze Unterschied im Urteil.

Der letzte Punkt der Tabelle ist eine bewusste Entscheidung mit Begründung
(`units/FBSimUnit.cpp`, `kWeaponNoGroundElevM`): JSBSims Ground-Reactions beschreiben ein RUHENDES
Objekt. Die Feder-/Dämpferwerte des mk82-Modells (10.000 lbf/ft, 200.000 lbf/ft/s) divergieren bei
150 m/s Aufschlag innerhalb eines Schritts — es bliebe kein Aufschlagzustand zu melden. Eine Bombe
federt nicht, sie detoniert. Der Store fliegt daher ballistisch durch den Boden hindurch, und WO der
Aufschlag war, rekonstruiert der Runner sub-Tick (§5.3).

---

## 2. `FBStoresSystem` — der SMS

`sim/src/systems/FBStoresSystem.{h,cpp}`. Der airframe-agnostische DEFAULT (Interface + Implementierung
in einer Klasse, `FBSystemSlots.h`-Muster); ein Modul liefert nur seine PYLON-GEOMETRIE
(`modules/f16/FBF16Sms`) und kann `Run()` überschreiben.

### 2.1 Vertrag

| Phase | Aufruf | Bedingung |
|---|---|---|
| Setup | `DeclareStation(number, xIn, yIn, zIn)` | vor `AttachFdm`, Stationsnummer 1-basiert wie der Jet zählt, Position im STRUKTURRAHMEN des Modells (Zoll) |
| Setup | `AttachFdm(FBFdm&)` | legt je deklarierter Station EINE JSBSim-Punktmasse `station<N>` an (Gewicht 0) |
| Missionsdaten | `Load(station, FBStoreSpec)` | false bei unbekannter oder belegter Station |
| Bedienung | `SetMasterArm`, `SelectStation` | Master Arm steht beim Hochfahren auf `Sim` (SAFE) |
| Bedienung | `Release(nowS, outcome, reason)` | **nur über den Kommandobus** erreichbar |
| Besitzer | `TakeRelease(FBStoreRelease&)` | FIFO-Drain, false wenn leer |
| Tick | `Run(FBState&, dt)` | publiziert den `FBStoresBlock`, cached die Feuerleitungs-Antwort |

### 2.2 Trägereffekt — Physik der Engine, nicht Rechnung dieser Klasse

`PublishLoadout()` (nur bei Load und bei Release, nie pro Frame — zwischen diesen zwei Ereignissen
ändert sich an einer Zuladung nichts):

- **Masse**: je Station eine JSBSim-Punktmasse (`FBFdm::SetStorePointMassLbs`). Masse, Schwerpunkt UND
  Trägheitstensor kommen damit aus `FGMassBalance`, nicht aus einer eigenen Rechnung.
- **Widerstand**: die SUMME der `DragAreaFt2` als JSBSim-`<external_reactions>`-Kraft `fb-stores`
  (`FBFdm::SetStoresDrag`), `CdA·qbar` entgegen der Körper-x-Achse, angreifend am
  **gewichtsgewichteten Schwerpunkt der belegten Stationen**. Das ist kein Detail: eine asymmetrische
  Zuladung erzeugt damit ihr Gier-/Rollmoment aus JSBSims eigenem Kraftmodell und nicht aus einem hier
  erfundenen Term.
- **Beim Abwurf** geht die Punktmasse im SELBEN Tick auf 0 — das Flugzeug wird sofort leichter, besser
  ausgetrimmt und sauberer, als Physik.

Gemessen (`missions/mk82-carriage-loaded.fbm` vs. `mk82-carriage-clean.fbm`, identisch bis auf vier
`set store`-Zeilen, beide level bei vollem Schub):

| Größe | 4× Mk-82 | sauber |
|---|---|---|
| Startmasse | +2.000 lb | — |
| Zeit auf Mach 1,0 | 25,8 s | 22,6 s |
| Zeit auf Mach 1,2 | 51,5 s | 41,1 s |
| Spitzen-Mach | 1,364 | 1,416 |

Die Loggzeile `sms RELEASE` nennt `gwLbs` bewusst als Gewicht **vor** dem Wirksamwerden: `FGMassBalance`
summiert die Punktmassen in seinem eigenen `Run`, das neue Gewicht existiert also einen Schritt später
und wird aus der Telemetriespalte `sms_gw_lbs` gelesen. Ein „Vorher", das der nächste Schritt
widerspricht, wäre eine Messung von nichts.

### 2.3 Die Klasse SPAWNT nichts — und das ist die Anti-Cheat-Struktur

`Release()` erzeugt einen `FBStoreRelease`-Datensatz (`core/FBStore.h`) in einer Warteschlange fester
Kapazität (`kMaxPendingReleases == kMaxStations` — eine volle Ripple aller Stationen passt hinein, die
Queue kann also nie der Grund sein, dass ein Store verlorengeht). Der BESITZER der Simulation leert sie.

Der Grund ist strukturell: eine `FBFdm` zu erzeugen verlangt `fdm/FBFdmBoot.h`, und dieser Header darf
von keiner Datei unter `systems/` oder `modules/` inkludiert werden. Ein Modul, das sich selbst eine
Einheit in die Welt setzen könnte, hätte einen Weltschreibpfad — genau das, was CLAUDE.mds „Kein
Cheaten" ausschließt. Der Freigabepfad endet daher in einem WERT, nicht in einer neuen Zelle.

Der Runner (`app/FBMissionRunner.cpp`) leert die Queues **am Ende des Ticks**, in Akteursreihenfolge,
je Akteur FIFO; der neue Store wird erst im NÄCHSTEN Tick gerechnet. Das ist die
Determinismus-Bedingung: die Step-Phase verteilt Akteursindizes über Threads, ein mitten in der Phase
auftauchender Akteur würde das Ergebnis von der Reihenfolge abhängig machen.

### 2.4 Die Ablehnungsgründe

`Release()` prüft in genau dieser Reihenfolge (`doc/f16/controls-commands.md` §6.2/§6.5):

| # | Bedingung | Outcome | Reason | Detail |
|---|---|---|---|---|
| 1 | Master Arm ≠ ARM | Rejected | `hardware_precedence` | „master arm not in ARM" — die Sicherheit des Piloten |
| 2 | Gewicht auf dem Fahrwerk | Rejected | `hardware_precedence` | „weight on wheels" — die Sicherheit der Zelle |
| 3 | keine belegte Station gewählt | Rejected | `out_of_context` | gültiges Kommando, falscher Kontext |
| 4 | Freigabe-Queue voll | Rejected | `channel_busy` | „release queue not drained" |
| 5 | `RequiresLock` und kein Lock/Ziel | Rejected | `out_of_context` | „no fire-control lock" |
| 6 | `RequiresLock` und keine DLZ | Rejected | `out_of_context` | „no launch-zone solution" |
| 7 | `RequiresLock` und außerhalb der DLZ | Rejected | `out_of_context` | „target beyond Raero" / „target inside Rmin", zusätzlich `sms LAUNCH_OUT_OF_ZONE` |
| — | sonst | Accepted | `none` | + Station-Step auf die nächste belegte Station |

Die beiden Hardware-Verriegelungen kommen ZUERST, weil ein physischer Schalter den Softwarepfad sperrt
und kein Software-Zustand sich daran vorbeireden darf. Die waffenspezifischen Prüfungen (5–7) kommen
NACH ihnen und VOR dem Verlassen der Schiene; sie sind die Antwort der FEUERLEITUNG (in `Run()` aus dem
`FBFireControlBlock` gecached), nicht die Meinung des SMS — `Release()` wird vom Kommandobus zwischen
zwei Ticks gerufen und darf nicht selbst nach dem Bus greifen.

### 2.5 Was eine Runde beim Start mitbekommt

Zwei Übergaben, ein Prinzip: **die Vorhersage muss den Jet MIT der Waffe verlassen.**

| Runde | Feld in `FBStoreRelease` | Inhalt | Quelle |
|---|---|---|---|
| gelenkt (`Guided`) | `LauncherId` + `Target` (`FBWeaponTargetState`) | wo die Feuerleitung des Schützen das Ziel zuletzt sah, und wessen Uplink zu hören ist | `SetTargetState()` je Tick vom Modul (`FBF16FireControl`) |
| ungelenkt | `Solution` (`FBReleaseSolution`) | vorhergesagter Aufschlagpunkt, Rechenebene, Flugzeit, Zielpunkt, Fehlabstand, Schärfreserve, Zeitstempel | `SetReleaseSolution()` je Tick vom Modul |

Warum: der Fehler zwischen dem, was der Rechner sagte, und dem, was geflogen wurde, ist eine ECHTE
Eigenschaft jedes Abwurfs (der FCC führt eine grobe gespeicherte Tabelle, die Waffe fliegt ihr volles
Aero-Modell). Bliebe die Vorhersage im Jet, könnte der Besitzer der Simulation sie nicht neben den
gemessenen Aufschlag legen. Deshalb reist sie auf der Runde mit und wird beim Aufschlag als
`stores DELIVERY` gegen die Wirklichkeit gestellt (§5.3).

`FBReleaseSolution::StampS` ist Teil der Ehrlichkeit: die Stores-Kommandogruppe wird im
`FBF16Module`-Ratenplan VOR dem Feuerleitungs-Tick bedient, die Lösung einer Runde ist also
notwendigerweise die des vorigen Sweeps. Diese Verzögerung ist bei Jägergeschwindigkeit zig Meter wert
— sie wird protokolliert (`solAgeS`), nicht versteckt.

### 2.6 Der Lenkfunk (Uplink)

`Uplink()` liefert, was dieses Flugzeug an eine von ihm gestartete Runde ABSTRAHLT — publiziert in der
`FBUnitSignature` der Einheit, wie XMT und der IFF-Transponder:

```
Uplink_.Active = GuidedInFlight_ > 0 && Target_.Valid
```

Aktiv also nur, solange (a) eine lock-fordernde Runde gestartet wurde UND (b) die Feuerleitung noch ein
Ziel hat. Verliert der Schütze den Lock, hört die Aussendung mitten im Flug auf — das ist der ganze
taktische Sinn der Midcourse-Phase. Sie hört ausdrücklich NICHT auf, wenn der Flug der Runde endet:
niemand meldet das dem Schützen, und im echten Jet auch nicht — der Sender verstummt, wenn der Pilot
den Schuss nicht mehr stützt.

### 2.7 Telemetrie & Ereignisse

`sms_arm`, `sms_station`, `sms_loaded`, `sms_lbs`, `sms_released`, `sms_gw_lbs` (die letzte Spalte ist
das Bruttogewicht der ZELLE in derselben Zeile wie die Bücher des SMS — nur so ist der Trägereffekt
mess- statt behauptbar).

Ereignisse: `sms RELEASE`, `sms RELEASE_REJECTED`, `sms LAUNCH_OUT_OF_ZONE`, `sms LAUNCH_SOLUTION`
(gelenkt: die komplette DLZ im Moment des Starts), `sms RELEASE_SOLUTION` (ungelenkt: die
Abwurflösung).

---

## 3. `FBGunSystem` — die Bordkanone

`sim/src/systems/FBGunSystem.{h,cpp}`. Der SMS-Geschwisterslot. Gleich, wo die Struktur gleich ist;
anders, wo die Waffen sich unterscheiden.

### 3.1 Worin es sich vom SMS unterscheidet

| | SMS | Kanone |
|---|---|---|
| Produkt | EIN Store je Auslösung | ein STROM von Geschossen |
| Grenzobjekt | `FBStoreRelease` → wird eine EINHEIT | `FBGunBurst` → wird ein BÜNDEL im Pool des Klienten |
| Kapazität der Queue | eine je Station | `kMaxPendingBursts` = 4 (der Besitzer leert jeden Tick) |
| Kommandowert | 1.0 (Pickle) | **Dauer** des Abzugsdrucks in Sekunden |
| Bus-Latenz | HOTAS 0,5 s | 0,1 s (`FBCommandBus::kTriggerLatencyS`) |
| Auslöserate | eine Handlung je Store | eine Handlung, N Ticks Feuer |

Die Latenz-Ausnahme ist begründet: die 0,5 s der anderen HOTAS-Kommandos sind eine Tastendauer; die
Verzögerung zwischen Fingerdruck und erstem Schuss ist der Rohr-Hochlauf, und der steckt bereits im
Waffenmodell (`SpoolUpS` 0,3 s). Beides zu zählen wäre doppelt gerechnet. Der ABSTAND zweier Drücke
bleibt `kHotasLatencyS`.

Warum ein Bündel keine Einheit ist (`core/FBGunProjectiles.h`): 6.000 Schuss/min gegen einen 0,1-s-Tick
sind zehn Schuss pro Tick und Flugzeug; ein andauernder Kampf produzierte Tausende, jedes mit
JSBSim-Instanz, Telemetriedatei und Monitor. Was die Geschosse physikalisch SIND, rechtfertigt das
nicht: ungelenkte Klumpen ohne Systeme und ohne Entscheidungen, an die nur eine Frage gestellt wird —
wo sind sie und was haben sie getroffen. Also leben sie als Arithmetik.

### 3.2 Die integrierte Rate — und warum sie taktunabhängig sein MUSS

`RoundsBetween(a, b, ratePerS, spool)` ist das Integral der Feuerrate zwischen zwei Momenten `a`,`b`
nach dem Abzugsdruck, mit linearem Hochlauf über `spool`:

```
I(x) = 0                              x <= 0
I(x) = rate · x²/(2·spool)            0 < x < spool     (Rampe)
I(x) = rate · (spool/2 + (x−spool))   x >= spool        (voll)
n    = I(b) − I(a)
```

`Run()` addiert `n` auf `Fraction_`, nimmt `floor` als ganze Schüsse ab und trägt den Rest mit. Damit
ist die Trommel in exakt `Capacity/rate` Sekunden leer, egal mit welchem `dt` der Slot getaktet wird —
und ein 0,1-s-Tick bei 6.000 rd/min sind zehn Schuss, nicht „ein Feuerstoß".

Genau deshalb wird die Kanone im F-16-Modul **nicht gedrosselt**, sondern einmal pro `Run()` mit dem
VOLLEN `dt` betreten (`FBF16Module.cpp`): eine andere Eintrittsrate würde Schüsse erfinden oder
verschlucken. Eine nicht feuernde Kanone kostet einen Vergleich.

### 3.3 Verriegelungen

| Bedingung | Outcome | Reason |
|---|---|---|
| kein Gun installiert | Rejected | `not_implemented` |
| Master Arm ≠ ARM | Rejected | `hardware_precedence` |
| Gewicht auf dem Fahrwerk | Rejected | `hardware_precedence` |
| Trommel leer | Rejected | `depleted` — die eine Ablehnung, die eine Tatsache über das FLUGZEUG ist statt über die Anfrage |
| `seconds <= 0` | Rejected | `out_of_range` |
| `seconds > MaxBurstS` | **Clamped** | `value_clamped` (gemeldet, nicht still gekürzt) |
| Kanone zerschossen | Rejected | `system_failed` (vom Modul-Router, §8.3) |

Ein zweiter Druck während eines laufenden Feuerstoßes VERLÄNGERT ihn auf das spätere Ende; der Hochlauf
wird nicht neu gestartet, weil die Rohre nie stehengeblieben sind.

### 3.4 Was ein Bündel trägt

Beim Erreichen eines ganzen Schusses legt `Run()` einen `FBGunBurst` an:

- **Ort**: die MÜNDUNG, nicht der CG. `FBBodyVecToEnu(roll,pitch,yaw, fwd,right,down)` auf den
  Installationsversatz, dann geodätisch aufaddiert. Bei Kanonenentfernung ist dieser Versatz der
  Unterschied zwischen Treffer und Vorbeischuss.
- **Geschwindigkeit**: Eigengeschwindigkeit des Jets PLUS Mündungsgeschwindigkeit entlang der
  Bore-Richtung (`FBBodyLosToEnu` auf `BoreRightDeg`/`−BoreDownDeg`). Beide Hälften sind Physik und
  diese Klasse kennt beide — was die Grenze überquert, ist deshalb ein schlichter Geschosszustand.
- **Anzahl** und `LauncherId`, `Kind`, `SimTimeS`.

### 3.5 Sie wertet NIE einen eigenen Treffer

Dieselbe Grenze wie beim SMS: der Besitzer der Simulation leert die Queue, fliegt die Runden
(`core/FBGunProjectiles`) und entscheidet auf den VERÖFFENTLICHTEN Posen, was sie getroffen haben. Eine
Kanone, die sich selbst punktet, wäre derselbe Betrug wie eine Rakete, die sich selbst punktet.

### 3.6 F-16-Installation

`modules/f16/FBF16Gun.h`: M61A1, Mündung im linken Rumpfwurzel-Strake.

| Achse | Wert | Herkunft |
|---|---|---|
| vorwärts | +4,6 m | aus dem gepinnten `f16.xml`: CG bei FS −193 in, Port ~180 in davor [SET innerhalb der Modellgeometrie] |
| rechts | −0,9 m | Backbord-Installation, halbe Rumpfbreite |
| unten | −0,3 m | Oberkante des Strake-Fillets |
| Bore | 0°/0° | doc/f16/ nennt KEINEN Einbauwinkel — Null ist die ehrliche Wahl und steht als solche im Header |

### 3.7 Telemetrie & Ereignisse

`blk_gun` (zuerst — der Gun-Block kam lange nach `FBStateBusTelemetry`s Liste dazu, ein Name dort hätte
jede Spalte rechts davon verschoben), `gun_rounds`, `gun_fired`, `gun_firing`, `gun_triggers`,
`gun_refused`, `gun_burst`, plus die GELESENE Ziellösung `gun_sol_rng`, `gun_sol_err`, `gun_sol_span`,
`gun_in_funnel` (ein Lesen fremder Blöcke, das jedem System erlaubt ist; sie sitzen auf der Gun-Source,
weil „wohin zeigte die Kanone" und „was kam heraus" EINE Messung sind).

Ereignisse: `gun TRIGGER`, `gun BURST`, `gun BURST_DROPPED`, `gun DRY`, `gun HIT`, `gun MISS`.

---

## 4. Ballistik

### 4.1 `FBGunBallistics` — die GETEILTE Arithmetik

`sim/src/core/FBGunBallistics.{h,cpp}`. Reine Funktionen auf Werten, kein Zustand, keine Allokation.
Drei Konsumenten benutzen buchstäblich denselben Code:

| Konsument | Wann | Frage |
|---|---|---|
| `modules/f16/FBF16FireControl` | VOR dem Schuss | wohin muss das Rohr zeigen (EEGS-Lösung) |
| `core/FBGunProjectiles` | NACH dem Schuss | wo sind die Geschosse |
| `app/FBTestGun` (`make -C sim test-gun`) | Prüfung | stimmt beides gegen `doc/f16/weapons.md` |

Dass Feuerleitung und geflogene Bahn identischen Code teilen, wäre normalerweise ein Cheat (deshalb ist
`FBWeaponPerf` eine bewusst GRÖBERE separate Kopie der Raketen-Aerodynamik). Hier ist es keiner, und
der Unterschied ist der Punkt: eine AMRAAM fliegt als gelenkte Zelle mit eigenem Autopiloten und
eigenem Energiemanagement — keine Tabelle sagt das exakt vorher. Ein 20-mm-Geschoss ist ein ungelenkter
Klumpen auf einem ballistischen Bogen, und genau den löst der FCC. Eine Abweichung dazwischen zu
modellieren hieße, einen Fehler zu ERFINDEN statt einen zu messen.

**Das Bahnmodell** — Punktmasse unter Schwerkraft und quadratischem Widerstand, `dv/dt = −k·v²` entlang
der Geschwindigkeit, mit

```
k = 0.5 · rho · Cd · A / m         [1/m]     (rho = ISA-Dichte auf Feuerhöhe)
A = pi/4 · RoundDiaM²
```

Die eine benannte VEREINFACHUNG: Widerstand wirkt auf den BETRAG, Schwerkraft auf die Vertikale — nicht
auf die Vektorsumme. Über die ganze nutzbare Lebensdauer einer Runde (unter 2 s, unter 2 km) ist der
Fall Meter gegen einen Weg von Kilometern; der Winkel zwischen beiden ist klein, die Entkopplung kostet
Zentimeter. Was sie einbringt, ist eine GESCHLOSSENE FORM:

```
v(t) = v0 / (1 + k·v0·t)
s(t) = ln(1 + k·v0·t) / k
t(s) = (exp(k·s) − 1) / (k·v0)          — der exakte, iterationsfreie Inverse
```

Der Inverse ist der Grund, warum die Vorhaltelösung unten in festen sechs Durchläufen ohne Suche und
ohne Allokation konvergiert. `t(s)` liefert −1, wenn `k·s > 20` — ein Wächter gegen Unsinnseingaben,
damit keine Unendlichkeit in eine Pose propagiert.

**Die Vorhaltelösung** (`FBGunSolveLead`) — Fixpunkt einer Gleichung: die Bahnlänge `s(t)` muss dem
Abstand zu dem Ort gleichen, an dem das Ziel zur Zeit `t` sein wird:

```
D(t) = rel + v_target·t + up·(0.5·g·t²)
```

(aktueller Versatz, Zielbewegung während der Flugzeit, und der Fall, ÜBER den gezielt werden muss).
Richtung → `t` exakt aus `FBGunTimeToPath`; `t` → Richtung aus `D(t)`. Sechs Durchläufe setzen das
deutlich unter einen Meter fest, auf jeder Entfernung, auf der die Kanone benutzt wird.

**Die Eigengeschwindigkeits-Korrektur** ist der Schritt, den eine naive Vorhaltrechnung falsch macht:
die Runde verlässt das Rohr mit der EIGENGESCHWINDIGKEIT plus der Mündungsgeschwindigkeit, die
Flugrichtung der Runde ist also NICHT die Rohrrichtung. Geschlossen lösbar — zerlege die
Eigengeschwindigkeit in die Anteile längs und quer zur gewünschten Flugrichtung; das Rohr muss quer
genau so weit vorhalten, dass die Mündungsgeschwindigkeit den Queranteil aufhebt:

```
v_along  = v_own · flightdir
v_across = v_own − v_along·flightdir
mu       = sqrt(v_muzzle² − |v_across|²)
bore     = (mu·flightdir − v_across) / v_muzzle          (dann normiert)
v0       = v_along + mu
```

Dieser Vorhaltwinkel ist der physikalische Ursprung der EEGS-Trichterform: darum gehen die Geschosse
eines hart kurvenden Jägers dorthin, wo seine Nase nicht ist. `Valid=false` heißt: es gibt keine
Lösung — das Ziel läuft schneller weg, als die Runde schließt, oder der Jet quert schneller, als die
Mündungsgeschwindigkeit kompensieren kann (bei 1.030 m/s gegen ein Flugzeug unmöglich, wird aber
geprüft statt angenommen).

Ausgabe `FBGunAim`: `TofS`, `RangeM`, Bore-Einheitsvektor (ENU), `SpreadM = DispersionSigmaRad · dist`,
`ImpactSpeedMs` = Restgeschwindigkeit minus Zielanteil längs (also die Geschwindigkeit, die das ZIEL
sieht — ein Frontalschuss kommt härter an als ein Verfolgungsschuss, ohne dass das irgendwo extra
gesagt werden müsste; auf ≥0 geklemmt).

**Das Trefferdichte-/Energiemodell.** Kein Zufall, nirgends. Die Runden sind ein zirkularnormales
Muster der Breite `sigma` um die Bündelachse, das ZIEL ist eine Scheibe der Fläche, die es präsentiert,
und die erwarteten Treffer sind die Überlappung beider. Schreibt man die Zielscheibe als ihre eigene
Äquivalentnormale (`sigma_t² = A/(2π)` — die Breite, deren Zentraldichte der einer Scheibe der Fläche A
entspricht), wird die Überlappung geschlossen:

```
hits = N · A/(A + 2π·sigma²) · exp( −d² / (2·(sigma² + A/(2π))) )
```

mit `d` = Fehlabstand zum Zielmittelpunkt. Energie je Runde `E = ½·m·v_rel²`; die Flussdichte ist
`hits·E` verteilt auf die KLEINERE der beiden Flächen (Muster `2π·sigma²` oder Zielfläche):

```
FBGunFluxJm2 = hits · 0.5·m·v_rel² / min(2π·sigma², A)        [J/m²]
```

Jeder Grenzfall fällt aus derselben Formel:

| Fall | Ergebnis | Konsequenz |
|---|---|---|
| Muster ≫ Ziel | Ziel fängt `N·A/(2π·sigma²)` ab | Fluss ∝ 1/Entfernung² (sigma wächst linear) — **darum** ist eine Kanone eine Kurzstreckenwaffe, hergeleitet statt per Reichweitengrenze verordnet |
| Muster ≪ Ziel | jede Runde trifft, auf `2π·sigma²` | Fluss sättigt bei dem, was ein Nullabstandsstoß tut |
| Bündel ein paar Meter daneben | die AUSDEHNUNG des Ziels fängt einen Teil | ein Punktziel-Modell hat genau das falsch — ein Flugzeug ist Meter breit |

**Zwei Maßstäbe** (`FBGunExpectedHits`), weil ein Jäger zwei hat: `targetAreaM2` = wieviel MATERIAL
präsentiert wird, `extentM` = wie weit dieses Material vom Zentrum REICHT. Eine einzige Scheibe kann
beides nicht: richtig für einen Stoß auf den Rumpf, „gar nichts" für einen vier Meter daneben, wo eine
echte F-16 noch Flügel hat. Also das GRÖSSERE zweier Lesungen desselben Musters:

- **KOMPAKT**: das Material als eine Scheibe der Fläche A (exakt für einen Stoß auf die Mitte — dort,
  wo ein tödlicher Stoß liegt);
- **AUSDEHNUNG**: dasselbe Material dünn über die Silhouettenscheibe des Radius `extentM` verteilt,
  eine Runde darin trifft mit Wahrscheinlichkeit `A/(π·extent²)`. Nur ausgewertet, wenn die Silhouette
  größer als A ist; `extentM = 0` schaltet die zweite Lesung ab (Ziel gilt als kompakt).

Geklemmt auf `hits <= rounds`: ein Bündel kann nicht mehr Runden landen, als es hält.

**M61A1-Zahlen** (`core/FBGun.h`, Herleitungen dort im Original):

| Größe | Wert | Marke/Quelle |
|---|---|---|
| Mündungsgeschwindigkeit | 1.030 m/s | [T4] 3.380 ft/s, quellenkonsistent, kein T1/T2 |
| Feuerrate | 6.000 rd/min | ED-Zahl |
| Trommel | 510 | ED §3 (§2.5 desselben Guides sagt 512 — die Spezifikationstabelle gewinnt, die Differenz ist notiert statt gemittelt) |
| Hochlauf | 0,3 s | [T4] — modelliert, weil das Weglassen den größeren Fehler machte (~15 Schuss je Druck) |
| Geschossmasse | 0,100 kg | **[SET]** — §4.1 verweigert diese Zahl ausdrücklich; JEDE kinetische Schadenszahl ist linear in ihr, darum hier einmal benannt |
| Kaliber | 0,020 m | 20×102 mm |
| Cd | 0,30 | **[SET]**, prüfbar: ergibt ~1,3 s Flugzeit auf 1.000 m (`make test-gun`) |
| Streuung σ | 2,2295e−3 rad | **[DERIVED]** aus MIL-DTL-45500/1A: „80 % eines 75-Schuss-Stoßes in 8,0 in bei 1.000 in" = 80 % in 4 mil Radius. Für ein zirkularnormales Muster ist `P(r<R) = 1 − exp(−R²/2s²)`, also `s = 4 mil / sqrt(2·ln5) = 2,2295 mil`. **Gegenprobe mit der NICHT verwendeten zweiten Zahl derselben Quelle**: sagt 97,3 % im 12-mil-Kreis voraus, den die Quelle „100 %" nennt. Eine Gleichverteilungsscheibe hätte nur 44 % im 8-mil-Kreis — ausgeschlossen durch die Quelle, nicht durch Geschmack |
| max. Feuerstoß | 1,0 s | [SET] = 100 Runden; ein Trigger-Kommando ist EINE Handlung und braucht eine Dauer |

Ausdrücklich NICHT modelliert: Rohrverschleiß, Streuung der Mündungsgeschwindigkeit von Schuss zu
Schuss, Munitionsmischung (Leuchtspur/HEI/API — die Trommel ist homogen) und die MASSE der Munition
(510 Runden ≈ 110 lb; die Empty-Weight-Aufteilung des vanilla `f16.xml` ist nicht zerlegbar,
Prinzip 1, also würde eine Trommel-Punktmasse ebenso wahrscheinlich doppelt zählen wie korrigieren —
unter 0,5 % des Bruttogewichts, benannt statt versteckt).

### 4.2 `FBBallistics` — das geteilte Primitiv beider Luft-Boden-Verfahren

`sim/src/core/FBBallistics.{h,cpp}`. EINE Vorwärtsintegration, zwei Fragen:

| Modus | Frage | Funktion |
|---|---|---|
| CCIP | „wenn ich JETZT auslöse, wo trifft sie?" | `FBSolveImpactPoint` |
| CCRP | „gegeben dieser Punkt — WANN muss ich auslösen?" | dieselbe Vorhersage, auf den aktuellen Bodenkurs projiziert (`FBSolveAim`) |

Damit können Pipper und Freigabe-Countdown nicht auseinanderlaufen.

**Was integriert wird** — und warum es bewusst NICHT die Aerodynamik ist, die die Bombe dann fliegt:

```
a = −g·u_up − (0.5·rho(h)·v²·Cd·S/m) · v_hat
```

Gelesen werden nur vier Felder aus `FBWeaponPerf`: `LaunchMassKg`, `DragCoefA`, `RefAreaM2`, `ArmingS`.
Ein Store ohne Motor und Sucher hat nicht mehr. Die Dichte ist ISA auf der AKTUELLEN Höhe der fallenden
Runde und wird jeden Schritt neu ausgewertet — eine Bombe aus 4 km fällt durch ein Drittel der
Atmosphäre, eine Einzeldichte wäre hier um mehr falsch als der gemessene Effekt.

Nicht modelliert (Auslassungen des RECHNERS, nicht der Simulation): Auftrieb (die Runde ist eine
Punktmasse ohne Anstellwinkel), Wind (es gibt keinen), Corioliskraft, Mach-Abhängigkeit von Cd.

**Warum die Differenz eine echte Eigenschaft jedes Abwurfs ist:** die Runde wird beim Verlassen des
Pylons ihre eigene JSBSim-Instanz mit dem vollen Aero des gepinnten Modells — Mach-abhängiger
Widerstand, Auftrieb beim Trimm-Alpha, Nickdämpfung. Ein echter Feuerleitrechner hat nichts davon; er
führt eine gespeicherte Tabelle und integriert eine Punktmasse. Genau das tut diese Datei. Der Fehler
zwischen Vorhersage und geflogenem Ergebnis ist eine reale Eigenschaft JEDER je geflogenen Lieferung —
fütterte man die Rechnung mit der Aerodynamik der Waffe, versteckte man ihn. Die CCIP/CCRP-Missionen
messen exakt diesen Fehler.

**Numerik:** Heun (Prädiktor + Korrektor auf demselben Beschleunigungsgesetz), `kStepS = 0.05 s`,
`kMaxTofS = 120 s` (Leck-Wächter). Begründung im Header: eine Mk-82 aus 4 km bei 450 kt fällt ~30 s,
also 600 Schritte — billig genug für den 10-Hz-Feuerleitungsslot und fein genug, dass der Schrittfehler
weit unter dem MODELLFEHLER liegt, den die ganze Vorhersage offenlegen soll (gemessen: Halbierung
verschiebt den Aufschlagpunkt um deutlich unter einen Meter). Plain Euler wäre bei quadratischem
Widerstand um Meter systematisch daneben — dieselbe Größenordnung wie der gemessene Effekt, also nicht
gratis hinnehmbar. Der Durchstoß der Ebene wird INNERHALB des kreuzenden Schritts linear interpoliert
(Quantisierung auf 0,05 s wären ~15 m Wurfweite bei Abwurfgeschwindigkeit).

**Die Aufschlagfläche wird ÜBERGEBEN, nie nachgeschlagen** — diese Datei kennt kein Gelände. Der
F-16-Aufrufer reicht die Steerpoint-Elevation, also denselben `FBElevationProvider`-Wert, den auch der
Radarhöhenmesser liest und gegen den der Monitor den Aufschlag beurteilt. Eine ebene Fläche auf dieser
Höhe ist genau das, was ein Jet mit barometrischer/Steerpoint-Entfernungslösung hat (Provider-Buchstabe
`B`).

**Ausgaben:**

`FBImpactPrediction`: `LatDeg`/`LonDeg` (double, weil 1e−5° ≈ 1 m die gemessene Größe IST), `ElevM`,
`TofS`, `RangeM`, `BearingDeg`, `ImpactSpeedMs` (die Annäherung, mit der ein Bodenburst aufgelöst wird),
`ArmMarginS = TofS − ArmingS` — der Pull-Up-Anticipation-Cue als das, was die Rechnung wirklich liefert:
wieviel Fall nach der Schärfzeit übrigbleibt. Negativ = Abwurf kommt unscharf an (der Blindgänger-Fall
der Quelle). Der echte Jet zeichnet das als Bildschirmposition Richtung FPM; eine Reserve in Sekunden
ist dieselbe Tatsache in der Form, in der die Entscheidung fällt, und braucht keine zweite Integration.

`FBAimSolution`: beide Punkte (vorhergesagter Aufschlag und designierter Zielpunkt) auf den AKTUELLEN
Bodenkurs projiziert (`FBTrackProjectM`) — die Achse, auf der ein Freigabe-Cue lebt: längs bewegt man
die Runde durch WARTEN, quer durch DREHEN. Eine Projektion, beide Modi: CCIP liest `MissM`, CCRP liest
`AlongErrM`/`TimeToGoS`. Ungültig ohne Bodenkurs (>1 m/s) — ohne Bewegungsrichtung gibt es keinen
Freigabepunkt, vor dem man zu kurz sein könnte; „keine Antwort" ist nicht „jetzt auslösen".

**Gemessen** (`missions/attack-ccrp.fbm` / `attack-ccip.fbm`, `--elev const`, 19 km Anflug, 900 m,
450 KCAS, Wurfweite 2.880 m):

| Größe | CCRP | CCIP | 2 s zu spät (`attack-late.fbm`) |
|---|---|---|---|
| `predErrM` (Rechner gegen Modell) | 57,1 m | 57,1 m | 57,1 m |
| `aimErrM` (Bombe gegen Ziel) | 22,2 m | 22,2 m | 481,5 m |
| davon lang / seitlich | 19,5 / 10,6 m | 19,5 / 10,6 m | 481,5 / 3,5 m |
| `tofErrS` | −0,097 s | −0,097 s | −0,097 s |
| Urteil | SUCCESS (0) | SUCCESS (0) | TIMEOUT (3), Ziel steht |

Der Rechnerfehler ist ein systematischer Vorhalt-Fehler nach KURZ: die echte Bombe fliegt weiter als die
Tabelle sagt, weil die Tabelle EIN Cd für alle Machzahlen führt und den Auftrieb der weathercockenden
Runde gar nicht kennt. Der Lieferfehler ist KLEINER als der Rechnerfehler, weil dessen Längsanteil dem
Auslösemoment entgegenläuft. Fehlerbudget:

| Posten | Betrag | Gehört |
|---|---|---|
| Rechner (Tabelle gegen Modell-Aero) | 57,1 m kurz | `core/FBBallistics` — erklärte Auslassung |
| Auslösemoment (Cue + Vorhalt) | 19,5 m lang (netto) | Pilot/Feuerleitung |
| Querbahn (Spurfehler der Führung) | 10,6 m | `systems/FBAutopilot` |

**Wogegen diese Zahlen gemessen sind.** Die AUFTEILUNG oben bleibt gültig — sie misst FlightBox' Führung
und Feuerleitung gegen FlightBox' eigene Ballistiktabelle, und beide Seiten sind unsere. Die ABSOLUTE
Zahl (22 m, davon 10,6 m quer) sagt dagegen nichts über einen echten Abwurf: Referenz ist die
Aerodynamik des Mk-82-Modells, dessen eigener `<fileheader>`-`<note>` einräumt, es könne „a gross
approximation, with the only similarity to an actual object being the name" sein und sei „for
educational and entertainment purposes only". Prinzip 5 gilt hier also besonders eng — die Zahl ist die
Treue zum MODELL, nicht die Treue zur Wirklichkeit, und darf nicht als Fidelity-Beleg zitiert werden.

### 4.3 `FBGunProjectiles` — der Pool

`sim/src/core/FBGunProjectiles.{h,cpp}`. Feste Kapazität `kMaxBundles = 64` (genug für vier
dauerfeuernde Flugzeuge über die volle Lebensdauer eines Bündels, mit Reserve). Eigentum des KLIENTEN,
von ihm getaktet, von ihm gelesen. `core/FBDamageModel`s strukturelles Geschwister:

- kein Modul kann ihn erreichen oder eines konstruieren — kein Flugzeug fliegt seine eigenen Geschosse
  und keines entscheidet, was sie taten;
- nichts darin ist zufällig, zeitabhängig oder versteckt: dasselbe Bündel aus derselben Geometrie
  fliegt dieselbe Bahn — das macht ein Kanonengefecht über Threadzahlen reproduzierbar;
- **es allokiert nichts.** Ein Bündel, das nicht aufgenommen werden kann, wird GEZÄHLT (`DroppedCount`)
  statt still verloren — ein Pool, der leise einen Feuerstoß frisst, brächte die Trommelarithmetik zum
  Nicht-mehr-Aufgehen.

`Bundle` trägt VORIGE und AKTUELLE Position, weil ein Treffer eine Closest-Approach-Rechnung über das
SEGMENT des Ticks ist (~100 m Weg je 0,1 s — ein Abstandstest je Tick verfehlte fast alles).

`Step(dt)`: Widerstand auf den Betrag (geschlossene Form aus §4.1), Schwerkraft auf die Vertikale,
Positionsupdate trapezförmig auf dem Mittel der zwei Geschwindigkeiten (zweiter Ordnung in dt — nötig
bei 0,1 s und ~1.000 m/s). `PathM` wächst mit (der Hebelarm des Streumusters), `AgeS` ebenso.

Lebensdauer: `kMaxAgeS = 3.0 s` ODER `kMaxPathM = 3000 m`, was zuerst kommt — beides weit jenseits der
Entfernungen, auf denen die Kanone benutzt wird (der Trichter selbst endet laut `weapons.md` §2.5 bei
3.000 ft). **Bewusst nicht modelliert:** Geschosse werden NICHT bis zum Boden verfolgt, es gibt keinen
ballistischen Aufschlag auf Gelände. Der Pool ist für Luft-Luft-Kanone; einen Strafing-Fußabdruck zu
behaupten, den hier nichts rechnet, wäre schlimmer als die benannte Abwesenheit.

`Retire(index)` ist das Urteil des Aufrufers: dieses Bündel ist gegen ein Ziel aufgelöst und verbraucht.
Ein Bündel trifft EINMAL — die Runden, für die es stand, sind ins Ziel gegangen.

---

## 5. Die drei Auflösungsgrenzen

Alle drei laufen über den BESITZER der Simulation (`app/FBMissionRunner.cpp`), nie über ein Modul, und
alle drei messen auf den VERÖFFENTLICHTEN Posen — also auf der Wahrheit, wie `FBFlightMonitor`s
Bodenkontakt. Der Grund ist immer derselbe: der Sucher der Waffe sagt, wo sie das Ziel VERMUTET; ließe
man die Waffe sich auf ihrer eigenen Schätzung punkten, wäre das die reinste Form des Betrugs.

### 5.0 Das gemeinsame Primitiv: `ClosestApproach`

Warum keine Abstandsprüfung: der Tick ist 0,1 s, eine frontale Annäherung kann 1.500 m/s überschreiten
— aufeinanderfolgende Abtastungen liegen 150 m auseinander, ein Abstandstest gegen einen 10-m-Zünder
verfehlte fast jeden echten Treffer. Also das Minimum über das SEGMENT zwischen der relativen Position
des letzten Ticks und der dieses Ticks, die Standard-CPA-Formel auf `p(t) = p0 + t·(p1−p0)`, `t∈[0,1]`:

```
t*      = −(p0·d) / (d·d),  d = p1 − p0,  auf [0,1] geklemmt
MissM   = |p0 + t*·d|
Closure = |d| / dt
FracT   = t*                    (der SUB-TICK-Zeitpunkt des Ereignisses — protokolliert)
RelE/N/U= der Vektor selbst     (die Richtung braucht die Schadensauflösung)
```

Die Geradlinigkeitsannahme innerhalb eines Ticks ist bei 20 g etwa einen Meter Krümmung wert — im
Header genannt, nicht versteckt.

### 5.1 Näherungszünder neben einem Jet

Bedingungen (alle im Runner, in dieser Reihenfolge):

1. der Store HAT einen Näherungszünder (`FuzeRadiusM > 0`; eine Bombe hat keinen),
2. `simT − SpawnS >= Perf.ArmingS` — **die Schärfverzögerung ist, was einen Start davon abhält, auf dem
   eigenen Träger zu detonieren**: eine Runde, die 3 m neben dem Jet die Schiene verlässt, ist deswegen
   kein Treffer auf ihn,
3. Ziel ist ein `Aircraft`, aktiv, nicht die Runde selbst,
4. `MissM <= FuzeRadiusM`.

Dann: `stores DETONATION` (Ziel, Fehlabstand, Zünderradius, Annäherung, sub-Tick-Flugzeit, Aspekt,
Höhen und Geschwindigkeiten beider) → `ResolveBurst` → `store.Retire()`.

`ResolveBurst` dreht den CPA-Vektor mit `FBEnuToBodyVec` in den KÖRPERRAHMEN des Ziels (aus dessen
publizierter Lage — demselben Snapshot, gegen den in diesem Tick alles andere gemessen wurde), setzt
`ClosureMs` und `WarheadKg` aus dem Katalog und ruft `FBSimUnit::TakeBurst`. Die Waffe liefert EINE
Zahl (ihre Sprengmasse), das Modul des ZIELS liefert EINE Tabelle (wo seine Systeme sitzen), und keiner
von beiden entscheidet etwas.

Die geringste Annäherung an ein anderes Flugzeug als den eigenen Schützen wird als `MinMissM`
mitgeführt und beim Ende der Runde als `stores MISS` gemeldet. Der Schütze ist vom BERICHT
ausgenommen, nicht vom Zünder: eine Runde, die von einem Pylon separiert, passiert ihren eigenen Träger
in zig Metern — eine Tatsache über Geometrie, nicht über Zielgenauigkeit.

### 5.2 Kanonenstrom

Reihenfolge im Tick, fest: Bündel fliegen (`Bullets.Step(dt)`) → gegen jedes passierte Flugzeug
auflösen → ERST DANN die in diesem Tick abgefeuerten Bündel aufnehmen. Ein Bündel wird also nie in dem
Tick aufgelöst, in dem es entstand — dieselbe Snapshot-Disziplin wie beim Akteurswachstum.

Je Bündel × je Flugzeug (nicht der Schütze selbst):

| Schritt | Wert |
|---|---|
| Streuung an dieser Stelle | `sigmaM = DispersionSigmaRad · PathM`, Untergrenze 0,05 m |
| Vorprüfung | `MissM > 3·sigma + kGunHitReachM (8 m)` → überspringen (jenseits der eigenen Reichweite der Zelle; die Dichterechnung könnte dort nur eine Zahl liefern, die kein Bericht tragen sollte) |
| präsentierte Fläche | `FBPresentedAreaM2(layout, fwd,right,down)` im Körperrahmen des Ziels; `<= 0` → gar kein Ziel (ein Store, eine Einheit ohne deklarierte Zelle) |
| präsentierte Ausdehnung | `FBPresentedExtentM(…)`, gleiche Interpolation |
| erwartete Treffer | `FBGunExpectedHits(...)`; `< kMinReportedHits (0,1)` → **Vorbeischuss**, nichts wird aufgelöst |
| Energiedichte | `FBGunFluxJm2(...)` |
| Anwendung | `FBSimUnit::TakeKineticBurst` → `FBDamageModel::ApplyKinetic` |
| danach | `Bullets.Retire(bi)` — die Runden sind in ihn gegangen |

Ein Bündel, dessen Leben endet, ohne getroffen zu haben, erzeugt `gun MISS` mit der DICHTESTEN je
erreichten Annäherung (nicht dem ersten Tick, in dem es irgendwo in die Nähe kam) — genau diese Zahl
sagt, ob das Zielen oder das Timing falsch war. Gemeldet nur unter `kGunNearMissM` (200 m): weit genug,
dass „der ist an ihm vorbeigegangen" gemessen wird, eng genug, dass ein Bündel im selben Himmel nicht
zählt.

### 5.3 Bodenaufschlag eines Stores

Der Store hat keine Bodenkontaktkräfte (§1), fliegt also ballistisch weiter, bis der Physik-Richter
Penetration meldet. Beim 0,1-s-Tick ist er dann bereits bis zu einen Tick UNTER der Oberfläche —
gemessen an einer Mk-82 mit 216 m/s Ankunft: 14 m Tiefe, also ~20 m Horizontalweg jenseits des echten
Aufschlagpunkts. Das ist ein Fünftel des GESAMTEN Lieferfehlers, den dieser Missionssatz misst, und
es ist ein Artefakt der Abtastrate, nicht etwas, das Flugzeug oder Rechner taten.

`GroundCrossing(store, backS)` rekonstruiert den Durchstoß aus der beobachteten Probe:

```
depth = GroundAslM − elev            (nur wenn > 0 und Sinkrate < −0,1 m/s)
backS = depth / (−vy)
lat  −= (−vz)·backS / kMPerDeg
lon  −= vx·backS / (kMPerDeg·cos lat)
elev  = GroundAslM
```

Über ~0,1 s ist die Krümmung des Bogens Zentimeter wert — darum eine Gerade und keine zweite
Integration. Bewusst NICHT zwischen den letzten zwei publizierten POSEN interpoliert: bis der Richter
schließt, liegt auch die vorige Pose schon unter der Oberfläche (es braucht ein paar Meter Penetration,
damit das ein Urteil und kein Rundungsfehler ist) — es gibt kein einschließendes Paar.

Alles Weitere benutzt DIESEN Punkt: `stores IMPACT` (`crossLat`/`crossLon`/`crossBackS`/`crossTofS`),
`stores DELIVERY` (die Vorhersage aus §2.5 gegen die gemessene Wirklichkeit: `predErrM`, `aimErrM`,
`aimLongM`/`aimAcrossM` in der ANKUNFTSRICHTUNG der Runde — eine Bombe weathercockt in ihre
Geschwindigkeit, ihr Kurs beim Aufschlag ist also der Anflugkurs —, `tofErrS`, `planeM` gegen
`groundAslM`, `armMarginS`, `solAgeS`), und der Gefechtskopf.

`ResolveGroundBurst` gegen jedes aktive `Ground`-Ziel. Das Nähe-Tor ist ABGELEITET, kein gewählter
Radius: die NIEDRIGSTE Schwelle, die das Layout dieses Ziels deklariert, ist die geringste Energie, die
ihm überhaupt etwas antun kann — ein Burst, dessen Fluss auf dieser Entfernung darunter liegt, könnte
nur eine Null-Effekt-Zeile und einen falschen Eintrag in seiner Trefferzahl erzeugen. `ClosureMs` ist
hier die reine Ankunftsgeschwindigkeit der Runde (das Ziel bewegt sich nicht).

### 5.4 Warum ein Bodenburst NICHT gegen Flugzeuge aufgelöst wird

Ein Jet tief über der eigenen Detonation ist real in einer Splitterhülle. Modelliert würde das eine
Frag-gegen-Zelle-Geometrie brauchen (und ein Ausweichmanöver, gegen das man sie messen kann), die hier
nichts hat; einen Burst gegen alles innerhalb eines ERFUNDENEN Cutoffs aufzulösen wäre eine Zahl, die
sich als Physik ausgibt. Also: ein Bodenburst verletzt BODEN-Einheiten, und die Grenze steht im
Klartext im Code statt hinter einer Radiuskonstanten.

Dieselbe Regel andersherum bei `target_soft`/`target_hard`: deren präsentierte Fläche/Ausdehnung sind
NULL, also legt das Trefferdichtemodell der Kanone keine Runde auf sie — **kein Strafing**. Auch das ist
keine später per Schätzung zu füllende Lücke: Strafing bräuchte Geschosse, die bis zum Boden verfolgt
werden, was der Kanonen-Pool ausdrücklich nicht tut (§4.3); eine präsentierte Fläche hier würde eine
Fähigkeit behaupten, die die Geschossseite nicht hat.

---

## 6. `FBDamageModel` — was ein Treffer ANRICHTET

`sim/src/core/FBDamageModel.{h,cpp}`. Der EINE Schreiber von `core/FBSystemHealth` (dessen einziger
`friend`), Eigentum des Klienten. Ein Modul löst seinen eigenen Schaden nie auf, so wenig wie es seinen
eigenen Absturz beurteilt.

**Es ist ein MODELL und sagt das.** Beobachtet und prüfbar ist der EINGANG: die Burst-Geometrie (die
Closest-Approach-Rechnung des Runners auf den publizierten Posen), die Annäherung und die Sprengmasse
aus dem Store-Katalog. MODELLIERT ist der Schritt von diesen drei Zahlen zu einem Systemzustand, und er
ist aus den zwei Dingen gebaut, die wirklich Physik sind — isotrope Splitterausbreitung und kinetische
Energie — plus einer Schwelle je System, die eine Setzung ist.

### 6.1 Die Energie eines Gefechtskopfs, in drei Schritten

| # | Schritt | Formel | Annahme |
|---|---|---|---|
| 1 | Splittermasse | `m_frag = WarheadKg · kCaseFraction` | `kCaseFraction = 0.5` **[SET]** — übliche Größenordnung für eine Splitterhülle; `doc/f16/weapons.md` §4.7 führt Gefechtskopf-Innenleben als echte Lücke, also Setzung statt Zitat |
| 2 | Flächendichte | `rho_A = m_frag / (4π·r²)` [kg/m²] | ISOTROPIE — die eine geometrische Annahme. Ein echter Gefechtskopf sprüht in ein fokussiertes Band; das machte das Ergebnis vom Winkel zur Raketenachse abhängig, und nichts hier behauptet, dieses Band zu kennen |
| 3 | spezifische Energie | `flux = ½·rho_A·(v_frag² + v_closure²)` [J/m²] | `kFragSpeedMs = 1800` **[SET]**. Für eine radialsymmetrische Wolke ist der MITTLERE Betrag der Vektorsumme aus Auswurfgeschwindigkeit (radial) und Annäherung `sqrt(v_eject² + v_closure²)` — bewusst NICHT `v_eject + v_closure`, was nur für die geradeaus geworfenen Splitter gälte |

Entfernungs-Untergrenze `r >= 0,5 m`: keine physikalische Aussage, sondern ein Schutz — das 1/r²-Gesetz
divergiert bei null, und ein Burst INNERHALB der Zelle ist nicht lehrreicher als einer an ihrer Haut.
0,5 m ist etwa die halbe Rumpfbreite eines Jägers, also das Näheste, was außerhalb liegen kann.

Ergebnis ist ein **1/r²-Gesetz in der Energie**: doppelter Fehlabstand = ein Viertel der ankommenden
Energie. Das — und nicht irgendeine einzelne Schwelle — ist, was das Modell auf Entfernungen sinnvoll
verhalten lässt, auf denen es niemand kalibriert hat.

`FBFragmentFluxJm2(warheadKg, rangeM, closureMs)` ist ÖFFENTLICH, damit ein Bericht, ein Harness oder
eine Logzeile die genaue Zahl hinter einem Schadensurteil reproduzieren kann, statt ihr zu vertrauen.

### 6.2 Zonen: 1/r²-Abfall statt Partition

Ein Flugzeug ist kein Punkt. Das Layout (Moduldaten) zerschneidet die Zelle entlang ihrer LÄNGSACHSE in
Zonen und benennt, welche Systeme in welcher sitzen. `FBDamageModel::Apply` rechnet PRO ZONE einen
eigenen Abstand:

```
fwd_clamped = clamp(burst.FwdM, zone.AftM, zone.FwdM)
r           = |(burst.FwdM − fwd_clamped, burst.RightM, burst.DownM)|
```

Ein Burst quer zur Mitte einer Zone ist also so nah wie sein Querabstand; einer vor der Nase muss auch
noch die Achse entlang zurückgreifen.

**Jede Zone wird ausgewertet, nicht nur die nächste.** Splitter gehen überall hin, sie kommen weiter
weg nur dünner an — das 1/r²-Gesetz das sagen zu lassen ist ehrlicher, als die Zelle zu partitionieren
und einer Partition alles zu geben. Die Zelle ist als dieses Achsensegment modelliert und als sonst
nichts: kein Querschnitt, keine Abschirmung, keine Splitterzählung.

Je Zone und System: `flux >= FailJm2` → `Failed`, sonst `flux >= DegradeJm2` → `Degraded`. Ein System
ohne ABLEITBARES degradiertes Verhalten setzt `Degrade == Fail` und hat damit nie eines.

`FBDamageResult` meldet: Zone mit dem höchsten Fluss, deren Abstand, den Spitzenfluss, die Bitmasken
`NewlyFailed`/`NewlyDegraded` (was DIESER Burst geändert hat) und `WasEffective`/`NowEffective`.

### 6.3 `ApplyKinetic` — der zweite Eingang

Bewusst ein ANDERER Eingabetyp statt eines Flags auf demselben, weil die beiden Waffenwirkungen durch
verschiedene Dinge BEKANNT sind:

| | Gefechtskopf | Kanonenstoß |
|---|---|---|
| bekannt durch | eine MASSE, aus der das Modell die Energie ableitet | eine **FLÄCHENENERGIEDICHTE**, die der Besitzer der Simulation bereits aus Trefferzahl, Aufschlaggeschwindigkeit und Streuung gerechnet hat (`FBGunFluxJm2`) |
| Geometrie | isotroper Splitterregen → jede Zone sieht etwas | schmales Muster → nur die Zonen, die der Fußabdruck berührt |
| Summierung | **nein** — eine Detonation ist EIN Ereignis | **ja**, je Zone (`FBSystemHealth::AddKinetic`) |

Warum diese Datei die Energie eines Feuerstoßes nicht selbst ableitet: sie sieht nie eine Runde und
hätte kein Recht dazu.

Was beide TEILEN, und warum das legitim ist: das ZIEL. Beide drücken das Ankommende als J/m² an einer
Stelle der Zelle aus, beide werden gegen dieselben Schwellen beurteilt, also antwortet EIN
Schadensregister für beide ohne einen zweiten, unkalibrierten Satz Zahlen. Das ist eine erklärte
Modellentscheidung und keine physikalische Behauptung: 20-mm-Einschläge und Gefechtskopfsplitter
beschädigen Struktur nicht über denselben Mechanismus; die gemeinsame Währung ist die Wahl dieses
Simulators, keine Äquivalenzaussage.

**Warum kinetische Energie je Zone SUMMIERT wird** (`FBSystemHealth::AddKinetic`, das einzige Stück
Schadenszustand, das keinem System gehört): eine Kanone ist ein durchgehender Strom, den dieser
Simulator notwendig in Tick-Bündel schneidet. Jedes Bündel für sich zu beurteilen machte den Schaden
zu einer Funktion der TICKRATE — genau das, was CLAUDE.mds Prinzip 4 verbietet. Fünfzig Runden als fünf
Bündel richten den Schaden von fünfzig Runden an. Ein Gefechtskopf hat kein Äquivalent und benutzt das
nicht.

**Fußabdruck**: `half = max(SpreadM, 0,5 m)`; nur Zonen, die `[FwdM−half, FwdM+half]` überlappen, sehen
etwas. Der Boden von 0,5 m sorgt dafür, dass ein Nullabstands-Stoß — dessen Muster Zentimeter breit ist
— auf der Zone landet, durch die er ging, statt auf einem mathematischen Punkt zwischen zweien.
`res.RangeM = 0` — ein Treffer, kein Abstands-Burst; es gibt keine Entfernung.

### 6.4 `FBDamageLayout` — zwei Maßstäbe

```
FrontalAreaM2  seen head-on/from astern        FrontalExtentM  halbe Spannweite
LateralAreaM2  seen from the side/above        LateralExtentM  halbe Länge
```

Interpolation für einen Strom aus Richtung `(fwd,right,down)` im Körperrahmen des Ziels:

```
along  = |fwd| / |v|
across = sqrt(1 − along²)
Fläche     = Frontal·along + Lateral·across
Ausdehnung = FrontalExtent·along + LateralExtent·across
```

„Die einfachste Interpolation, die an beiden Enden exakt ist, und überhaupt keine Behauptung über die
Form dazwischen." Zwei Zahlen statt einer, weil der Unterschied bei jedem Jäger ein Faktor drei ist und
das Interpolieren gratis ist. Ein Modul, das keine davon deklariert (der Default, und jeder abgeworfene
Store), präsentiert NICHTS und nimmt keinen Kanonenschaden — was korrekt ist: auf eine Bombe im freien
Fall schießt niemand.

### 6.5 Determinismus — und was daraus folgt

Kein Zufallsgenerator irgendwo in dieser Datei, keine Zeitabhängigkeit, kein interner Zustand. Gleiche
Geometrie + gleicher Gefechtskopf + gleiche Annäherung → gleiche Masken, immer.

Folgen: ein Gefecht ist über Threadzahlen bit-identisch reproduzierbar (nachgemessen, s. CLAUDE.md
„Etappe 4"); ein Regressionslauf kann Schadensbilder als Fingerabdruck verwenden; und ein Debriefing
kann jede Zahl hinter einem Urteil nachrechnen, weil `FBFragmentFluxJm2` und `FBGunFluxJm2` öffentlich
sind.

### 6.6 Die physikalischen Folgekonstanten

Alle in `FBDamageModel.h` beisammen, damit das ganze „wie sich Schaden anfühlt"-Modell auf einmal
lesbar ist. Angewandt ausschließlich über JSBSim (`FBSimUnit::ApplyDamageToAirframe` → `fdm/FBFdm`),
nie über ein zweites Parallel-Flugmodell.

| Konstante | Wert | Herleitung |
|---|---|---|
| `kAuthorityDegraded` | 0,5 | **[SET, aber mit strukturellem Grund]**: die F-16 hat ZWEI unabhängige Hydrauliksysteme an ihren Aktuatoren — eines zu verlieren ist die natürliche Bedeutung von „degradiert". Skaliert die kommandierten Ausschläge IN `FBFdm::SetControls`: die FLCS kommandiert unverändert weiter, das Flugzeug antwortet nur nicht mehr |
| `kAuthorityFailed` | 0,0 | keine Autorität; die Ruder antworten nicht mehr, das Flugzeug fliegt auf Trimm und Eigenstabilität — genau das Departure, das JSBSim dann selbst integriert |
| `kThrottleLimitDegraded` | 0,6 | **[DERIVED]** aus der `throttle-cmd-norm`-Konvention des F-16-Modells: dort sitzt das Nachbrenner-Tor. Degradiert = kein Nachbrenner |
| (Triebwerk failed) | — | JSBSims eigener Cutoff, kein hier erfundener Schubterm |
| `kDamageDragFt2Degraded` | 1,5 ft² | **[SET]**; Maßstab: die Nullauftriebs-Widerstandsfläche einer sauberen F-16 liegt in der Größenordnung 4 ft² — degradiert = „spürbar dreckig" |
| `kDamageDragFt2Failed` | 6,0 ft² | **[SET]**; = „mit einem Loch fliegen". Angesetzt über DIESELBE `<external_reactions>`-Mechanik wie der Zuladungswiderstand (`FBFdm::SetDamageDrag`), durch den CG — es wird KEIN Nickmoment behauptet, das niemand belegen kann |
| `kRadarRangeDegraded` | 0,70710678… | **[DERIVED]** aus der Radargleichung: `R⁴ ~ Pt·G²` mit `G ~ A`, also `R ~ sqrt(A)`; halbe Apertur = `1/sqrt(2)` der Reichweite |

---

## 7. `FBSystemHealth` — das Register

`sim/src/core/FBSystemHealth.{h,cpp}`. EIN Register je `FBSimUnit`, strukturelles Geschwister von
`FBFlightMonitor`/`FBMissionMonitor`: Eigentum des KLIENTEN, gespeist nur von einem core-eigenen
Urteil, GELESEN — nie geschrieben — vom Modul, das das Flugzeug fliegt.

### 7.1 Das Schreibtor ist der TYP, nicht eine Konvention

Jeder Mutator (`Worsen`, `NoteHit`, `AddKinetic`) ist **privat**, und es gibt genau einen `friend`:
`FBDamageModel`. Es existiert also nirgends eine API — nicht auf einem const-Handle, nicht auf einem
nicht-const — mit der ein System, ein Pilot oder ein Modul sich selbst (oder jemand anderen) als
beschädigt oder repariert markieren könnte. `grep -rn FBSystemHealth src/systems src/modules` findet
nur Lesezugriffe, und es KANN nichts anderes finden: alles andere kompiliert nicht.

### 7.2 Monoton

Ein Zustand wird nie besser. Es gibt keine Reparatur im Flug, und ein monotones Register ist, was das
Schadensbild eines Laufs zu einer Funktion der genommenen Bursts und von nichts sonst macht — keine
Reihenfolgefrage, kein Heilungs-Race zwischen zwei Beobachtern.

### 7.3 Das Inventar

`FBSystemId` (Append only — das Ordinal ist telemetriesichtbar in den `dmg_*`-Bitmasken):

| Ordinal | Id | bedeutet |
|---|---|---|
| 0 | `Engine` | Antrieb: Schub |
| 1 | `FlightControls` | FLCS/Hydraulik: Ruderautorität |
| 2 | `Structure` | Zelle: Widerstand |
| 3 | `AirData` | ADC + Sonden |
| 4 | `RadarAlt` | Radarhöhenmesser (CARA) |
| 5 | `Nav` | INS/Navigation |
| 6 | `Radar` | aktives Luft-Luft-Set |
| 7 | `FireControl` | Startbereichsrechner |
| 8 | `Stores` | SMS: Racks und Verkabelung |
| 9 | `Datalink` | Netzterminal |
| 10 | `Rwr` | Warnempfänger |
| 11 | `Countermeasures` | Werfer |
| 12 | `Gun` | Bordkanone: Trommel, Zuführung, Rohre (**angehängt**, nach der Regel des Enums) |

Die Liste ist bewusst der Modul-SLOT-Satz plus die drei physischen Dinge, deren Folge JSBSim selbst
tragen kann.

### 7.4 Die drei Zustände für einen Konsumenten

| Zustand | Verhalten |
|---|---|
| `Intact` | System läuft, publiziert seinen Ausgabeblock normal |
| `Degraded` | läuft und publiziert weiter, mit reduzierter Leistung DORT, wo eine ableitbar ist (Radarreichweite, Triebwerksdecke, FLCS-Autorität). Wo nicht, hat ein System kein degradiertes Verhalten und sein Layout-Eintrag erzeugt nie eines |
| `Failed` | System läuft NICHT und publiziert NICHT. Sein Block wird `Invalid`, und alles Weitere ergibt sich von selbst (§8) |

### 7.5 `CombatEffective` — ein MISSIONS-Urteil

```
CombatEffective() = !Failed(Engine) && !Failed(FlightControls) && !Failed(Structure)
```

Erklärte Modellentscheidung: eine Einheit ist kampfunfähig, sobald die ZELLE ihren Einsatz nicht mehr
zu Ende fliegen kann. Avionikverluste sind ausdrücklich NICHT Teil davon — ein Jet mit totem Radar und
toten Racks ist aus dem Kampf, fliegt aber; und was dieses Prädikat speist (`core/FBMissionMonitor`)
beurteilt den EINSATZ, nicht das Gefecht.

**Die Einheit ist nicht „tot", wenn das falsch wird.** Kein Freeze, keine Markierung, kein Fall für den
Physik-Monitor: sie fliegt genau so lange weiter, wie die Physik es hergibt, und stürzt ab, weil ihr
Triebwerk aus und ihre Steuerung weg ist. In der `UNIT_RESULT`-Zeile hat für eine abgeschossene Einheit
das MISSIONS-Urteil Vorrang vor dem späteren CRASH: der Abschuss erklärt den Aufschlag, der Aufschlag
erklärt nichts. Referenzlauf `missions/damage-amraam.fbm` — das Ziel deklariert bewusst KEINE Ziele,
trägt also gar keinen `FBMissionMonitor` und kann den Lauf nicht beenden; die Mission handelt von den
~340 s Nachspiel und endet mit Exit 2 (CRASH), verursacht von nichts als dem eigenen Schaden.

### 7.6 Telemetrie

Eigene Source `dmg`, vom Unit als LETZTE registriert (Anhängeregel): `dmg_hits`, `dmg_failed`,
`dmg_degraded` (Bitmasken über `FBSystemId`), `dmg_effective`.

---

## 8. DIE KOPPLUNG — der Kern des Ganzen

Ein ausgefallenes System wird vom Modul **nicht mehr getaktet**, und sein Block wird `Invalid`. Alles
Weitere ergibt sich aus dem Avionik-Bus, der das längst kann. **Dafür wurde nichts neu geschrieben.**

### 8.1 Das Gate im Modul

`modules/f16/FBF16Module.cpp` — je Slot ein Vergleich, immer nach demselben Muster:

```cpp
if (SystemWorking(FBSystemId::X)) X_->Run(...);
else SharedState.X.H.Invalidate();
```

| Slot | Gate | Zusatz |
|---|---|---|
| FCR | `Radar` | vorher `SetRangeFactor(Degraded ? kRadarRangeDegraded : 1.0)` |
| Luftdaten | `AirData` | — |
| Radarhöhenmesser | `RadarAlt` | — |
| Navigation | `Nav` | invalidiert AUSSERDEM `Cruise` (dieselbe Box publiziert beide Nachrichten) |
| Feuerleitung | `FireControl` | — |
| SMS | `Stores` | — |
| Kanone | `Gun` | — |
| RWR | `Rwr` | — |
| Gegenmaßnahmen | `Countermeasures` | — |
| Datalink | `Datalink` | — |

Der Warnsatz (`FBWarningSystem`) läuft als LETZTER der Gruppe und ist reiner Konsument alles darüber
Publizierten — inklusive der Gültigkeitsköpfe.

### 8.2 Was sich daraus GRATIS ergibt

| Konsument | Verhalten bei `Invalid` | wo es steht |
|---|---|---|
| HUD | **stricheln** — jede Anzeige fragt `H.Readable()` ab | `modules/f16/displays/FBF16Hud` |
| Warnsatz | die betroffene Warnung meldet sich als **INHIBITED** statt als „keine Warnung" | `systems/FBWarningSystem` |
| Pilot (Schuss) | kommt an `wantShot` gar nicht mehr heran: `zone = fc.H.Readable() && fc.DlzValid`, `weapons = state.Stores.H.Readable() && LoadedCount > 0` | `systems/FBPilot.cpp` |
| Pilot (Kanone) | `if (!state.Gun.H.Readable() \|\| !state.Gun.Ready) return;` und `if (!fc.H.Readable() \|\| !fc.GunValid) return;` | `FBPilot::BfmGunfire` |
| Pilot (Weiterkämpfen) | `CanPressOn` = Waffen an Bord ∧ kein BINGO ∧ strahlendes Radar — alle drei vom BUS gelesen, keines gewusst | `FBPilot::CanPressOn` |
| Kommandobus | Kommando an eine zerstörte Box → `rejected/system_failed` | `FBF16Module::ApplyCommand` |

### 8.3 Das Kommando-Gate

`FBF16Module::ApplyCommand` prüft VOR jeder Box:

```cpp
if (CommandOwner(c.Target, owner) && !SystemWorking(owner)) { Rejected; SystemFailed; return; }
```

Notwendig, weil der Freigabepfad NICHT durch `Run()` läuft, sondern durch diesen Router: ohne das Gate
ließe ein zerschossener SMS weiterhin eine Runde von der Schiene. Der Pilot verhält sich dann gratis
korrekt — er liest die Ablehnung genau wie die eines leeren Magazins.

Zuordnung Kommandoziel → besitzendes System (Auszug): `MasterArm`/`StationSelect`/`WeaponSelect`/
`WeaponRelease` → `Stores`; `GunTrigger` → `Gun`; `CmDispense`/`CmConsent`/`CmdsMode` →
`Countermeasures`; `Datalink*` → `Datalink`; `Radar*`/`Iff*` → `Radar`.

### 8.4 Wo Schaden PHYSIK wird

`FBSimUnit::ApplyDamageToAirframe()` — idempotent, aufgerufen unmittelbar nach jedem `TakeBurst`/
`TakeKineticBurst`, der einzige Ort, an dem Schaden Physik wird:

| Zustand | Triebwerk | Flugsteuerung | Struktur |
|---|---|---|---|
| Failed | JSBSim-Cutoff | `SetControlAuthority(0.0)` | `SetDamageDrag(6.0)` |
| Degraded | `SetThrottleLimit(0.6)` | `SetControlAuthority(0.5)` | `SetDamageDrag(1.5)` |
| Intact | — | — | — |

Alle drei Kanäle sind neutral, bis etwas getroffen wurde: ein unbeschädigtes Flugzeug rechnet
bit-identisch wie eines, das nie von Schaden gehört hat (nachgemessen).

### 8.5 Ereignisse und Spalten

| Ereignis | wann | Felder (Auszug) |
|---|---|---|
| `damage DAMAGE` | je Treffer | Zone, Abstand zur Zellenstruktur, `fluxJm2`, `warheadKg`, `closureMs`, Körperrahmen-Koordinaten, Bitmasken, Trefferzahl |
| `damage SYSTEM` | je System, das den Zustand ändert | `system=…`, `state=degraded\|failed` |
| `damage KILL` | genau einmal, wenn `WasEffective && !NowEffective` | Grund, `failed`-Maske, Höhe/Geschwindigkeit (Luft) bzw. Position (Boden) |
| `gun HIT` | je aufgelöstes Bündel | erwartete Treffer, Bündelgröße, `missM`, `spreadM`, `impactMs`, `areaM2`, `extentM`, `fluxJm2`, Zone |

Spalten: `dmg_*` (§7.6) und — derselbe Vorgang aus der anderen Richtung — die `blk_*`-Spalten, die jeden
Block einer ausgefallenen Box `Invalid` werden sehen.

---

## 9. Zonen und Fragilität als MODULDATEN

### 9.1 F-16 (`modules/f16/FBF16Damage.{h,cpp}`)

Jede Zonengrenze ist aus dem gepinnten `f16.xml` gelesen (Strukturrahmen: x positiv NACH HINTEN, CG bei
FS −193 in), umgerechnet in Meter VOR dem CG — keine Zahl stammt aus einer Zeichnung oder einem
Handbuch:

| Referenz | Station | m vor CG |
|---|---|---|
| Radom-Kontaktpunkt (Nasenspitze) | FS −486,6 in | +7,46 |
| Eyepoint (Cockpit) | FS −336,2 in | +3,64 |
| Bugfahrwerk | FS −299,6 in | +2,71 |
| CG | FS −193,0 in | 0,00 |
| Hauptfahrwerk | FS −158,6 in | −0,87 |
| Flügelspitzen | FS −121,3 in | −1,82 |
| Ventralfinnen (Beginn Triebwerksbucht) | FS −97,6 in | −2,42 |
| Düse | FS 0,0 in | −4,90 |
| Fanghaken (hinterste Ausdehnung) | FS +100,7 in | −7,46 |

(Nase + Heck = 14,9 m — die eigene Länge 15,03 m der F-16.)

| Zone | Bereich [m] | Systeme |
|---|---|---|
| `Nose` | +3,64 … +7,46 | Radar (APG-68 Antenne/Sender), AirData (Pitot-/AoA-Sonden), Structure |
| `Forward` | 0,00 … +3,64 | Nav (INS), **Gun** (linker Strake), FireControl (FCC), RadarAlt (CARA), Datalink (MIDS), Structure |
| `Center` | −2,42 … 0,00 | Stores (SMS + Stationsverkabelung an den Flügelwurzeln), FlightControls (Hydraulik + Aktuatorstränge), Structure |
| `Aft` | −7,46 … −2,42 | Engine, Rwr (ALR-56M achtern), Countermeasures (ALE-47), FlightControls (Leitwerksaktuatoren), Structure |

Die Kanone nimmt die STRUKTUR-Schwellen, nicht die Avionik-Schwellen — nicht aus Effekt: eine Kanone
ist eine mechanische Installation mit Masse und Querschnitt der Zelle um sie herum, keine Black Box im
Rack. Was sie stoppt, ist, was die Struktur um sie herum durchlöchert.

**Die vier Fragilitätsklassen** — die eigentliche SETZUNG des Modells, alle `[SET]`, in J/m²:

| Klasse | Degrade | Fail | gedacht als |
|---|---|---|---|
| Avionik | 1,2e4 | 3,0e4 | eine Box: dünne Haut, keine Redundanz |
| Triebwerk | 5,0e4 | 1,5e5 | Nebenaggregate/Düse: nur Militärleistung |
| FLCS | 5,0e4 | 1,5e5 | eines von zwei Hydrauliksystemen |
| Struktur | 8,0e4 | 2,5e5 | Haut und Stringer: Widerstand |

Als Maßstab, gegen einen AIM-120-Gefechtskopf (20,5 kg) bei ~850 m/s Frontalannäherung:

| Schwelle | Entfernung | Schwelle | Entfernung |
|---|---|---|---|
| 1,2e4 | ~11,6 m | 8,0e4 | ~4,5 m |
| 3,0e4 | ~7,3 m | 1,5e5 | ~3,3 m |
| 5,0e4 | ~5,7 m | 2,5e5 | ~2,5 m |

Lesart: **alles, was den Näherungszünder (10 m) überhaupt auslöst, kostet Avionik; nur ein Burst
innerhalb ~3 m nimmt Triebwerk oder Flugsteuerung mit.** Jeder Zwischenfall folgt dann aus dem
1/r²-Gesetz statt aus einer weiteren Zahl.

Ein Avioniksystem hat außer dem Radar kein ableitbares degradiertes Verhalten (Reichweite über die
Radargleichung), also setzen alle anderen Boxen `Degrade == Fail` und betreten den Degraded-Zustand
nie. „Ein bisschen Rauschen" auf einem INS oder einem ADC zu modellieren wäre eine erfundene Zahl.

**Präsentierte Flächen/Ausdehnungen:**

| Größe | Wert | Herkunft |
|---|---|---|
| `FrontalAreaM2` | 4,0 | **[SET]**, Äquivalentfläche: ~1×1,5 m Rumpfquerschnitt plus die dünne Kante eines 27,9-m²-Flügels und die Finnen. Es wird NIRGENDS eine echte Formprojektion gerechnet |
| `LateralAreaM2` | 14,0 | **[SET]**: Modell-eigene `<wingarea>` 27,9 m², `<wingspan>` 9,14 m über 14,5 m Länge — die Planform liegt in dieser Größenordnung, die Seitenansicht darunter; 14 m² ist die Mitte, mehr kann eine einzelne Zahl für „quer zur Achse" ehrlich sein |
| `FrontalExtentM` | 4,57 | halbe Modell-`<wingspan>` — Modellgeometrie, keine Setzung |
| `LateralExtentM` | 7,3 | halbe Modelllänge — dito |

Die zwei Flächen skalieren die erwartete Trefferzahl LINEAR, weshalb sie hier einmal benannt sind.

### 9.2 Bodenziele (`modules/ground/FBGroundTarget.h`)

Eine Wertetypzeile je Zielklasse, kein Verhalten — dieselbe Entscheidung wie `core/FBStore.h` für einen
Store.

| Modul | Zone | Struktur degradiert ab | fällt ab | gedacht als |
|---|---|---|---|---|
| `target_soft` | ±10 m (20-m-Anlage) | 1,2e3 J/m² (Mk-82: ~69 m) | 2,8e3 J/m² (~45 m) | ungeschützte Anlage, Fahrzeugpark, Stellung |
| `target_hard` | ±6 m (12-m-Block) | 2,5e4 J/m² (~15 m) | 9,0e4 J/m² (~8 m) | Bunker, gehärtetes Bauwerk |

Die Radien sind die ehrliche Lesart der Schwellen: eine Mk-82 (87 kg, `kCaseFraction` 0,5,
`kFragSpeedMs` 1800) mit ~245 m/s Ankunft liefert `flux(r) = 5,71e6 / r²` J/m².

Alle vier Schwellen `[SET]` (`weapons.md` §4.7 nennt Gefechtskopf-Innenleben als echte Lücke; keine
Quelle im Baum nennt einen Wirkradius). VERANKERT sind sie an der offen und vielfach zitierten
Größenordnung für eine 500-lb-Universalbombe gegen ungeschützte Ziele — Wirk-/Ausfallradius der
Größenordnung 50–60 m — also ist 45 m für „erledigt" und 69 m für „verletzt" eine konservative statt
großzügige Lesart. Die harte Klasse sagt dann genau das, was die zwei Klassen unterscheiden sollen:
dieselbe Waffe braucht praktisch einen Direkttreffer.

Nur `Structure` wird deklariert: `CombatEffective` fragt nach Triebwerk, Steuerung und Struktur, und ein
Bauwerk hat genau eines davon. Einem SAM-Standort ein „Radar"-System zu geben hieße, einen Konsumenten
zu erfinden, den es nicht gibt.

`missions/attack-hardened.fbm` fliegt denselben Abwurf gegen `target_hard`: derselbe 22-m-Fehlabstand,
**keine** Schadenszeile (die ankommende Energie erreicht nicht einmal die Degrade-Schwelle) — TIMEOUT,
`result=INTACT`. Damit sind die Fragilitätsklassen ein Modell und keine Dekoration.

---

## 10. Die drei Waffenmodule

### 10.1 `modules/stores` — die ungelenkte Runde

`FBStoreModule.{h,cpp}` + `FBStoreModuleRegistration.cpp`.

- **Ein vollwertiges `FBModule`**, dessen Systemslots ALLE der airframe-agnostische Default sind: eine
  Mk-82 hat weder Autopilot noch Pilot noch Anzeigen noch Radar. RWR wird im Konstruktor stromlos
  gesetzt, damit nichts, was er hält, für ein Bild gehalten werden kann.
- **`Run()` tut genau eines**: das eigene FDM in festen 100-Hz-Substeps integrieren (gleicher
  Akkumulator und gleicher Spiralschutz wie jedes andere Modul, max. 12 Substeps je Frame). **Kein
  Steuerkanal wird je geschrieben** — die Bahn ist die Aerodynamik des gepinnten Modells plus
  Schwerkraft und sonst nichts. Das ist der ganze Sinn, eine Waffe als eigene FDM-Instanz statt als
  handgeschriebene Ballistikformel zu modellieren.
- **Eine Klasse, N Registry-Namen**: `FBStoreModuleRegistration` läuft über `kStoreCatalogue`,
  überspringt jeden `Guided`-Eintrag und registriert dieselbe Klasse unter dem Schlüssel des Stores
  (heute `mk82`). `FdmModelName()` kommt aus dem Spec.
- **`ApplySetup` gibt IMMER false**: ein abgeworfener Store nimmt keine Missionskonfiguration entgegen
  — er wurde konfiguriert, indem er auf einen Pylon geladen wurde. Ein `set` an einen Store könnte nur
  eine Mission sein, die glaubt, ein Flugzeug zu deklarieren, und ist ein Laufzeit-FAIL.

Eine gelenkte Waffe ist ein ANDERES Modul, kein Flag auf diesem. Welches von beiden ein Katalogeintrag
wird, sagt sein `Guided`-Flag, gelesen an genau je einer Stelle in den beiden Registrierungsdateien.

### 10.2 `modules/missile` — der Lenkflugkörper

`FBMissileModule.{h,cpp}`, `FBMissileSeeker.{h,cpp}`, `FBMissileGuidance.{h,cpp}`,
`FBMissileUplink.{h,cpp}`, `FBMissileModuleRegistration.cpp`. Heute: AIM-120.

Der exakte Gegenpart zu `FBStoreModule`: eine Bombe hat keinen Piloten und keine Sensoren, ihr `Run()`
integriert nur; eine Rakete hat beide, ihr `Run()` taktet sie — das ist der ganze Unterschied.

**Drei echte Slots** (keine Sonderfälle, sondern Ableitungen der generischen Systeme):

| Kategorie | Klasse | Basis |
|---|---|---|
| Sensoren | `FBMissileSeeker` | `systems/FBRadarSystem` — das EINZIGE im Modul, das die Registry sieht |
| Comms | `FBMissileUplink` | `systems/FBDatalinkSystem` |
| Pilot | `FBMissileGuidance` | `systems/FBPilot` |

Alles andere ist Default: Guidance/FCS reichen die Ruderkommandos in `Manual` durch, keine Anzeigen,
keine Navigation, kein Warnsatz, keine eigenen Stores.

#### Raten

Sucher und Lenkung laufen INNERHALB der 100-Hz-Substep-Schleife, neben der FCS: eine Runde, die mit
1,5 km/s schließt, legt 15 m je 10 ms zurück — ein 10-Hz-Entscheidungstakt (richtig für einen Piloten)
wäre ein 150-m-Lenkquantum. Die Antenne des Suchers führt ihr eigenes Absolutzeit-Frameraster (0,05 s),
wird also oft betreten, SCHAUT aber nur mit ihrer eigenen Rate. Der Uplink-Empfänger läuft einmal je
`Run()`: die Feuerleitung des Schützen kann ohnehin keine frischere Schätzung als ihr eigenes
Radarframe erzeugen.

#### Das Lenkgesetz: Proportionalnavigation, mit Herleitung

Sei `r` der Vektor Rakete→Ziel und `v_rel = v_target − v_missile`. Die Sichtlinie dreht mit

```
Omega = (r × v_rel) / |r|²        [rad/s, Vektor entlang der Drehachse der LOS]
Vc    = −d|r|/dt = −(r · v_rel)/|r|
```

Die **geometrische Kerntatsache**: bewegen sich zwei Objekte geradlinig, kollidieren sie genau dann,
wenn die Sichtlinie NICHT rotiert (`Omega = 0`), während die Entfernung abnimmt. Konstante Peilung bei
abnehmender Entfernung = Kollisionskurs (dieselbe Regel wie in der Seefahrt). Die ganze Aufgabe eines
Lenkgesetzes ist also, `Omega` auf null zu treiben, und PN tut das mit einer Beschleunigung SENKRECHT
zur LOS, proportional zur Drehrate und zur Annäherung:

```
a_cmd = N · Vc · (Omega × r̂)
```

Der `Vc`-Faktor: dieselbe LOS-Rate zählt umso mehr, je weniger Zeit bleibt, und `Vc/|r|` ist der Kehrwert
dieser Zeit — er ist es, der PN konvergieren statt hinterherjagen lässt.

**Die Navigationskonstante `N`** — die Eigendynamik der LOS-Rate unter diesem Gesetz ergibt
`lambdä ∝ −(N−2)·lambdȧ/t_go`:

| N | Verhalten |
|---|---|
| ≤ 2 | LOS-Rate klingt gar nicht ab → Verfolgungsjagd, nie ein Abfangen |
| 3 | klassisches Minimum: der Fehlabstand aus einem Stufenmanöver klingt ab, das Integral der kommandierten Beschleunigung ist minimal für ein nicht manövrierendes Ziel |
| **4** | Standard für eine Luft-Luft-Rakete gegen ein manövrierendes Ziel: nimmt die LOS-Rate schneller heraus, die Runde kommt mit bereits erledigter Kurve an statt am Ende am härtesten zu ziehen, wo sie die wenigste Energie hat |
| ≥ 5 | verstärkt Sucherrauschen und Schätzfehler in Ruderaktivität — kostet Energie in Widerstand, lange bevor es Genauigkeit bringt |

FlightBox: `kNavConstant = 4.0`, eine Konstante an einer Stelle, damit ein künftiges Experiment sie
MESSEN kann statt darüber zu streiten.

**Schwerkraft**: PN sagt nichts über Gewicht — ein unvorbelastetes Gesetz ließe die Runde durchhängen,
tief ankommen und am Ende hochziehen. Ein g Aufwärts-Bias wird auf das Kommando addiert, genau das, was
der Autopilot einer echten Rakete mit seinem eigenen Beschleunigungsmesser tut.

#### Der Autopilot unter dem Gesetz

Die kommandierte Beschleunigung wird in den Körperrahmen aufgelöst und als **zwei unabhängige
Querbeschleunigungs-Regelkreise** geflogen — SKID-TO-TURN, denn ein kreuzförmiger Flugkörper nickt und
giert ohne zu rollen, es gibt keinen Auftriebsvektor zum Rollen:

```
fin = Ka·(a_kommandiert − a_Beschleunigungsmesser) − Kd·(Körperrate vom Kreisel)
```

Der Ratenterm ist nicht optional: die aerodynamische Nickdämpfung der Zelle ist konstruktionsbedingt
fast null (`aim120.xml`s `Cm_q`-Banner, −500 /rad, ausdrücklich niedrig in Dämpfungsgrad-Begriffen),
weil das ist, was ein kurzer beflossener Körper hat. Die Kreiselrückführung macht den Kreis stabil,
genau wie im Original. Der Rollkanal hält das Flossenkreuz waagerecht.

**Staudruck-Verstärkungsplan** — die Ruderautorität der Zelle ist proportional zu `q`.
`FBTestMissileAirframe` misst ~13,8 g je Einheit Ruderkommando bei Mach 2 / 6 km (`q = 119 kPa`;
dieselbe Zelle kauft bei Startgeschwindigkeit einen Bruchteil und tief/schnell ein Vielfaches davon).
Eine FESTE Verstärkung wäre also träge genau dort, wo die Runde langsam ist (kurz nach dem Start, wo
die erste Kurve gemacht werden muss), und zappelig, wo sie schnell ist. Also wird die Ruder-pro-g-
Verstärkung mit `qRef/q` skaliert:

| Konstante | Wert | Bedeutung |
|---|---|---|
| `kQRefPa` | 119.000 | der Staudruck, bei dem die 13,8 g/Einheit gemessen wurden |
| `kFinPerG` | 1/13,8 | Ruderkommando je g Forderung bei `kQRefPa` (der gemessene Kehrwert) |
| `kLoopP` / `kLoopI` | 1,2 / 2,0 | Proportional- und Integralanteil darüber |
| `kRateGain` | 0,35 | Ruder je rad/s Körperrate, gleich geplant |
| `kGainScaleMin/Max` | 0,15 / 20,0 | Klemmung des Skalierungsfaktors |
| `kIntegralClamp` | 1,0 | Anti-Windup — integriert wird in RUDER-Einheiten und dort geklemmt, damit die Grenze die physische ist (ein Ruder kann nicht über seine Anschläge) statt einer, die je Verstärkung neu herzuleiten wäre |
| `kRollGain`/`kRollRateGain` | 0,05 / 0,02 | Rollhalter |
| `kMaxCommandG` | 25 | **[SET]** Kommandodecke: kein Ruderausschlag kauft mehr, als das Trimm-Alpha der Zelle auf dieser Höhe hergibt; 25 g liegt über dem Erreichbaren außer tief und schnell, begrenzt die Forderung also, ohne je die bindende Grenze zu sein |
| `kUplinkTimeoutS` | 1,5 | **[SET]** wie alt eine Uplink-Nachricht sein darf, bevor die Runde sich nicht mehr gestützt nennt: die Feuerleitung sendet mit ihrer Radarframerate (0,1–1 s), eine Sekunde ohne Nachricht heißt „Stütze beendet", nicht „Nachricht verpasst" |

**Warum der Integralanteil nicht optional ist**: ein reiner P-Beschleunigungskreis hinterlässt einen
stationären Fehler von `1/(1+Kreisverstärkung)`, und mit dem 1-g-Schwerkraft-Bias ist dieser Fehler ein
DAUERSINKEN — die Runde kommt tief an. Genau das zeigte der erste geflogene Abfang, bevor dieser Term
existierte (gemessen: −18 m/s, ~900 m zu tief am Merge).

#### Die drei Phasen — Übergang durch ERFASSUNG, nicht durch Timer

| Phase | Ordinal | Datenquelle |
|---|---|---|
| `INERTIAL` | 0 | die Startprogrammierung (`FBStoreRelease::Target`) — Position und Geschwindigkeit, konstant extrapoliert. Auch der Zustand, auf den die Runde ZURÜCKFÄLLT, wenn der Schütze aufhört zu stützen |
| `MIDCOURSE` | 1 | eine frische Nachricht über den Uplink des Schützen; die Runde zielt neu auf das, was dessen Radar JETZT sieht |
| `TERMINAL` | 2 | der EIGENE Sucher hat einen Lock; ab dann bleibt es terminal — ein Sucher, der das Ziel hat, fragt nicht mehr |

Strenge Priorität in `UpdateTarget`: eigener Sucher > Uplink > das zuletzt Bekannte. Jeder Zweig
schreibt DIESELBEN vier Felder, das Gesetz darunter fragt also nie, woher seine Zahlen kamen — nur, wie
alt sie sind.

**Der Übergang ist ein EREIGNIS**: der Sucher wird eingeschaltet, wenn die geschätzte Entfernung unter
die Aktivierungsentfernung der Runde fällt (Katalogzahl); die Phase wechselt erst, wenn er tatsächlich
ERFASST — was ihm nur gelingt, wenn die Midcourse ihn nah genug ausgerichtet hat (Sichtfeld ±10°). Eine
schlechte Midcourse produziert also einen Fehlschuss, keine magische Terminalphase.

**Uplinkverlust ist überlebbar und beobachtbar**: fällt der Lock des Schützen, stoppt die Aussendung;
die Phase fällt auf `INERTIAL` zurück und die Runde fliegt die letzte Information weiter, extrapoliert.
Ob das reicht, hängt davon ab, wie lange das her ist und was das Ziel seither tat — der ganze taktische
Punkt, gemessen statt behauptet: `missions/intercept-lostlock.fbm` trifft damit noch,
`missions/intercept-defeated.fbm` nicht mehr.

#### Der Sucher

`FBMissileSeeker` ist strukturell DIESELBE Klasse wie das FCR des Jets (`systems/FBRadarSystem`) — kein
Kurzschluss, sondern der Punkt: die Rakete ist eine Welteinheit wie jede andere und nimmt die Welt nur
so wahr, wie es hier erlaubt ist, nämlich über einen simulierten Sensor, der ein Volumen abtastet,
mehrere Blicke für einen Track braucht und ANONYME Geometrie in seinen eigenen `FBState` schreibt.

| Eigenschaft | Wert | Begründung |
|---|---|---|
| Sichtfeld | ±10° | **[SET]** — keine öffentliche Zahl (`weapons.md` §4.7 nennt genau diese Klasse als Lücke). Zehn Grad ist die Größenordnung für eine 7-Zoll-Schüssel, und es hat eine MESSBARE Folge: eine Midcourse, die mehr als 10° neben der Nase übergibt, erfasst nicht — das macht die Midcourse-Qualität zu etwas, das zählt |
| Gimbal | ±45° | **[SET]** und ausdrücklich eine ANDERE, viel größere Größe als das momentane Sichtfeld: nach dem Lock zeigt die Schüssel AUF das Ziel und läuft erst an den mechanischen Anschlägen aus. Ohne diese Unterscheidung verlöre ein gelockter Sucher sein eigenes Ziel, sobald die Geometrie 10° wandert — was der erste geflogene Lost-Lock-Lauf wörtlich zeigte (Verlust und Wiedererfassung 25° neben der Nase, 3 s vor dem Einschlag). Dieselbe Form wie der Suchbox/STT-Split des FCR |
| Framezeit | 0,05 s | **[SET]** — ein STARREN, kein Sweep: 20 Blicke je Sekunde, damit die Terminallenkung auf MESSUNGEN fliegt statt auf Extrapolation |
| Modus | `AutoAcquire` + `SingleTarget` | niemand designiert für eine Rakete; der erste feste Track wird STT, danach starrt sie ihn an |
| Slaving | `SlewTo(losAz, losEl)` bis zum eigenen Lock | die „SLAVE"-Sichtlinienbetriebsart aus `weapons.md` §2.5. BORE-Start ist dieselbe Klasse mit `SlewTo` auf null — deshalb ist die Betriebsart ein Zahlenpaar und keine Unterklasse |
| Zustand vor Aktivierung | AUS | ein ausgeschalteter Sucher ist nicht nur still — er MELDET nichts, damit die Lenkung nicht versehentlich auf einen Track von vor der Aktivierung zielt |
| IFF | Interrogator UND Transponder aus | eine Rakete kann nicht fragen, wer das ist, und niemand antwortet für sie |
| Emission | `FBEmitterKind::MissileSeeker` | das eine, was einen Empfänger interessiert: hinter dieser Antenne sitzt ein Gefechtskopf. Ein RWR klassifiziert dieses Signal als den Startfall, gleich wie es gerade abtastet |

#### Der Uplink-Empfänger

`FBMissileUplink` läuft die Registry ab, findet die EINE Einheit, deren Id der programmierte Schütze
ist, und nimmt dessen `FBUnitSignature.Uplink` **nur**, wenn er noch aktiv sendet. Sie liest sonst
nichts über diese Einheit und gar nichts über das Ziel: der Inhalt ist die RADARSCHÄTZUNG DES SCHÜTZEN,
mit dessen Fehlern und dessen Alter.

Veröffentlicht wird sie als Datalink-Track `Tracks[0]` — weil die empfangene Nachricht GENAU das ist
(eine Position, eine Geschwindigkeit und der Zeitpunkt der Messung); die Lenkung liest sie als
Instrument wie alles andere. Kein neuer Bus-Block, kein Rückkanal, und der Gültigkeitskopf beantwortet
die einzige Frage, die die Lenkung wirklich hat: sagt mir noch jemand etwas?

Der Track heißt `"UPLINK"` und trägt KEINE Unit-Id: das Radar des Schützen weiß auch nicht, wen es
anschaut. Eine Rakete kann keine Identität lernen, die ihr Schütze nie hatte. Der Zeitstempel ist der
BLICK DES SCHÜTZEN, nicht der Empfangsmoment — die Schätzung steht auf dessen Radar, und dessen Alter
ist, womit die Rakete fliegen muss.

Uplinkverlust ist kein Fehlerpfad: `Active` geht auf false, diese Klasse hört auf, Tracks zu
publizieren (`H.Invalidate()` — kein leeres Bild, sondern KEIN Bild), und die Lenkung sieht das Alter
wachsen. Nichts hier entscheidet etwas über den Flug.

#### Das Modell ist FlightBox-EIGEN

`sim/assets/aircraft/aim120/` — in derselben einen Modellwurzel wie f16 und mk82, aber als einziges
Modell OHNE Upstream-Gegenstück (`sim/assets/MODEL-DELTAS.md`, Herkunftstabelle: `—`). Das gepinnte
JSBSim-Submodul hat keine AMRAAM; hier gibt es also nichts zu diffen, sondern nur ein selbstgeschriebenes
Modell. Nichts unter `vendor/` wird durch seine Existenz angefasst.

Modelliert wird darin die GANZE Flugmechanik: Masse und ihre Abnahme über den Brand, Schub über die
Brennzeit, Axial- und Normalkraft über Mach und Anstellwinkel, statische Stabilität, Nick-/Gier-/
Rolldämpfung und die Ruder-Momente. `modules/missile/` schreibt Ruderkommandos und einen Gashebel und
liest einen Beschleunigungsmesser und Ratenkreisel zurück — sonst nichts. Es setzt nie eine Position,
eine Geschwindigkeit oder eine Lage.

Provenienzschema der XML (im Dateikopf): `[T-ED]` / `[T3]` / `[DERIVED]` / `[SET]`. Die aerodynamischen
Koeffizienten sind ALLE `[SET]` oder `[DERIVED]` — es existiert kein öffentliches Aero-Deck für diesen
Flugkörper, und ein Zitat dafür zu erfinden wäre schlimmer, als es zu sagen. Was sie ehrlich macht, ist
dass sie ein KONSISTENTER Schlankkörper-Satz sind (Trimmrelation, erreichbares g und
Widerstandsverzögerung sind je einzeln benannt und je an der Telemetrie dieses Modells messbar), nicht
dass sie belegt wären. Auszug:

| Größe | Wert | Marke |
|---|---|---|
| Durchmesser 7 in → `S = πd²/4` | 0,02482 m² = 0,2672 ft² | [T3] → [DERIVED] |
| Referenzlänge = Durchmesser | 0,5833 ft | [DERIVED] |
| Flossenspannweite 526 mm | 1,726 ft | [T3] (nur für die Rolldämpfung) |
| Startmasse | 335 lb | [T3] |
| Treibstoff | 115 lb | [DERIVED] aus dem Raketen-Grundgesetz gegen ED's „max. ~Mach 4": `Isp 235 s → ve = 2305 m/s`, `m0/m1 = exp(1000/2305) = 1,543`, `m_p = 335·(1 − 1/1,543) = 118 lb`, gerundet |
| Leermasse `<emptywt>` | 220 lb | [DERIVED] |
| CG | Station 69 in | [SET] — knapp vor Rumpfmitte, klassische Auslegung |
| Nickträgheit | 105 slug·ft² | [SET] (ein gleichförmiger Stab überschätzt einen Körper mit schweren Sektionen) |
| Rollträgheit | 0,44 slug·ft² | [DERIVED] `Ixx = m·r²/2` |
| Ruderdynamik | Verzögerung `c1 = 60 1/s` (~17 ms), Weg ±25° | [SET] |
| `Cm_alpha` | −12 /rad | [SET] — statische Stabilitätsmarge, direkt als Moment |
| `Cm_q` | −500 /rad | [SET] — bewusst NIEDRIG in Dämpfungsgradbegriffen; genau darum braucht der Autopilot die Kreiselrückführung |
| `Cl_p` / `Cl_da` | −12 / 0,05 /rad | [SET] — zusammen ~4 rad/s Dauerrollrate bei vollem Ruder und Mach 2 |
| Motor-Anlaufzeit | 0,06 s (sin-Rampe) | [SET] |

Der Motor (`engine/WPU-6.xml`, Rocket-Engine, `Isp 235`) ist ein Boost-Sustain; die AUFTEILUNG zwischen
beiden Phasen ist `[SET]` (die öffentliche Quellenlage sagt „boost-sustain" und sonst nichts).

**Kein Gashebel für einen Feststoffmotor**: die Lenkung kommandiert jeden Tick `ManualThr = 1.0`, und
der Gashebel-Slew in `FBFdm` (0,5 s aus dem Leerlauf) IST die Sicherheits-Separationsverzögerung vor
der Zündung. Es wird jeden Tick kommandiert, weil es den einen Kanal gibt, nicht weil irgendetwas ihn
wieder abschalten könnte.

**Eigene Telemetriespalten** `msl_*` statt der Pilotenkanäle (der Bus wird PRO EINHEIT aufgebaut, also
ändert sich am Trace eines Jets keine Spalte): `msl_phase`, `msl_range` (zur SCHÄTZUNG, nie zur
Wahrheit), `msl_closure`, `msl_losrate` (das, was PN auf null treibt), `msl_los_az`/`_el`,
`msl_nz_cmd`/`msl_ny_cmd`, `msl_fin_pitch`/`_yaw` (was wirklich die Ruder erreichte), `msl_seeker`
(0 aus / 1 aktiv / 2 gelockt), `msl_tgt_age` (seit der letzten echten Messung).

Ereignisse: `missile PROGRAMMED`, `missile PHASE` (jeder Wechsel mit Grund, Flugzeit, Entfernung, LOS),
`missile SEEKER_ACTIVE`.

### 10.3 `modules/ground` — das Modul, das nicht einmal integriert

`FBGroundModule.h` + `FBGroundModuleRegistration.cpp`. Strukturell `FBStoreModule` MINUS einer Sache
statt plus einer: eine abgeworfene Bombe hat keinen Piloten und keine Lenkung, integriert aber; dies
hat nicht einmal das.

- **`Run()` ist LEER.** Kein FDM zu takten, kein System zu cyceln, kein Zustand vorzurücken — sein
  ganzes Tick-Verhalten ist, dass seine Pose die von der Mission deklarierte ist.
- **`FdmModelName()` liefert einen LEEREN String** — das ist das SIGNAL an die Spawn-Bahn
  (`app/FBMissionBoot.h`): hier ist kein Airframe zu laden, und `AttachFdm` wird folglich nie gerufen.
  `UnitKind()` liefert `FBUnitKind::Ground` und sagt damit, was für eine Welt-Entität daraus wird.
- **Die Entwurfsfrage, die diese Klasse beantwortet**: die zwei Wege waren, einem Bunker ein triviales
  JSBSim-Modell zu geben (damit sich sonst nichts ändern muss) oder eine Einheit ohne eines existieren
  zu lassen. Der erste hätte ein erfundenes aerodynamisches Objekt bedeutet — Masse, Kontaktfedern, ein
  Trimmzustand — für ein Ding, das sich nicht bewegt, und 100 Hz Integration, nur um die Position zu
  reproduzieren, an der es gespawnt wurde. Also ist das AIRFRAME auf Unit-Ebene OPTIONAL
  (`std::unique_ptr<FBFdm>` darf null sein): eine Einheit mit einem wird getaktet, eine ohne hält ihre
  deklarierte Pose, und alles andere ist derselbe Code.
- **Das einzige Nicht-Default: `DamageLayout()`** — wo seine Struktur sitzt und wieviel sie aushält.
  Dieselbe Tabellenform, die `FBF16Damage` für die Zelle liefert, damit EIN Schadensmodell beide
  beantwortet. Das ist der Accessor, der aus einem Ziel einen echten Teilnehmer statt eines Markers
  macht.
- **Eine Klasse, N Registry-Namen**, wie bei den Stores; `ApplySetup` gibt immer false (was ein Ziel
  IST, sagt sein Modulname, wo es steht, seine `spawn`-Zeile).
- `UNIT_RESULT` eines Bodenziels lautet `INTACT` oder `DESTROYED` statt eines Flugurteils.

---

## 11. Missionsdaten und Beweisläufe

### 11.1 Die Schlüssel (F-16, `FBF16Module::ApplySetup`)

| Zeile | Wirkung |
|---|---|
| `set store <station> <typ>` | eine Zeile je Pylon; Station = Pylonnummer DIESES Musters (F-16: 1..9, 1/9 Flügelspitze, 5 Mittellinie), Typ = Katalogschlüssel (`mk82`, `aim120`). Unbekannte/doppelte Station oder unbekannter Typ = Laufzeit-FAIL beim Spawn |
| `set gun_rounds <n>` | Trommelinhalt beim Start, 0..510; mehr als die Kapazität = FAIL |
| `set brief_master_arm arm\|sim` | der Pilot setzt Master Arm IM FLUG über den Bus (HOTAS-Klasse) |
| `set brief_release_s <t>` | wiederholbar: wann der Pilot pickelt |
| `set task attack` + `set attack_mode ccip\|ccrp` | Angriffsphase und welcher Cue |
| `set pilot_attack_bias_s <s>` / `set pilot_attack_ccip_m <m>` | Varianten des Abwurfmoments bzw. der Pipper-Toleranz |
| `set pilot_gun_burst_s <s>` | Feuerstoßlänge |

### 11.2 Der Abwurfmoment des Piloten

Der Pilot rechnet **keine** Ballistik: er liest den `FBFireControlBlock` wie jedes Instrument.

| Modus | Cue |
|---|---|
| `ccrp` | `AgTimeToReleaseS <= 0` — die Solution-Cue passiert den FPM |
| `ccip` | derselbe Moment UND `|AgCrossErrM|` innerhalb der Pipper-Toleranz (F-16: 45 m) — das seitliche Urteil, das ein Countdown nicht fällen kann |

**Der Pickle wird um die eigene Betätigungslatenz VORGEHALTEN** (`FBCommandBus::LatencyS`, HOTAS 0,5 s):
genau auf dem Cue zu drücken löste den Store 0,5 s zu spät — bei 231 m/s sind das 115 m, mehr als die
ganze Rechnung wert ist. Der echte Jet löst dasselbe Problem andersherum: in CCRP HÄLT der Pilot den
Knopf und das FLUGZEUG löst aus. Gemessen: ohne Vorhalt 123 m lang, mit Vorhalt 8 m.

Der Gun-Abzug hält analog vor — um `kTriggerLatencyS + fc.GunTofS` —, und liest die Lösung als BETRAG
(eine Lösung, die durch null WANDERT, sagt −1,5° voraus, und das heißt „1,5° daneben", nicht „perfekt";
der frühere Clamp auf 0 machte die am schnellsten wandernde Lösung im Kampf zur besten). Gemessen über
je acht Anflüge, nur diese eine Zeile geändert: Feuerstöße 46 → 30 (geradeaus) bzw. 59 → 38 (kurvend),
Schuss auf dem Ziel je Feuerstoß 1,81 → 4,42 bzw. 2,21 → 3,19, Munitionsverbrauch je Abschuss
394 → 254 bzw. 270 → 204 Patronen.

Die Nachführung selbst (Fehlerrate + Integrator, `systems/FBPilot` Abschnitt 3c) verbesserte über je
acht Anflüge: Trichterzeit 3,2 s → 20,7 s (geradeaus) bzw. 0,0 s → 21,6 s (kurvend), Schuss auf dem
Ziel 11,9 → 111,2 bzw. 0,0 → 120,4 Patronen, Abschüsse 0 → 5 bzw. 0 → 7 von je acht Läufen, mittlerer
Nachführfehler 10,5° → 6,9° bzw. 11,9° → 4,1°.

### 11.3 Startbedingung eines abgeworfenen Stores

`FBMissionBoot.h::FBMissionSpawnStore` — derselbe Vier-Schritt-Spawn wie für jeden Jet, nur kommt die IC
aus dem TRÄGERZUSTAND (`FBFdmSpawn::Ballistic`):

- Position = Trägerposition + Stationsversatz (körperfest, mit der Trägerlage gedreht),
- Lage = Trägerlage,
- Velocity = Trägergeschwindigkeit **an dieser Station**, inkl. `ω × r` (damit ein Abwurf im Roll
  stimmt),
- **kein Ejektor-Impuls** — für dessen Größe existiert keine belegbare Quelle (`weapons.md` §4.5), also
  erbt der Store die Bewegung des Flugzeugs und nichts Erfundenes,
- **kein Trimm** — eine Bombe hat kein Ruder.

Der Stationsversatz wird im SMS gerechnet (strukturell → körperfest, relativ zum CG, JSBSims eigene
`FGMassBalance::StructuralToBody`-Konvention), weil nur er seine Pylongeometrie kennt.

### 11.4 Beweismissionen

| Mission | Gegenstand |
|---|---|
| `mk82-carriage-loaded` / `-clean` | Trägereffekt, numerisch (§2.2) |
| `mk82-safe` | Ablehnung: Master Arm nie scharf |
| `mk82-drop` | vier Abwürfe + ein Pickle auf einen leeren Jet |
| `attack-ccip` / `attack-ccrp` | Abwurfrechnung gegen Wirklichkeit (§4.2) |
| `attack-late` | derselbe Anflug, 2 s zu spät ausgelöst |
| `attack-hardened` | derselbe Abwurf gegen `target_hard` — kein Schaden, `INTACT` |
| `gun-bfm` | Zielverfolgungspass gegen einen geradeaus fliegenden Gegner |
| `gun-turning` | derselbe Schütze gegen einen DAUERKURVENDEN Verteidiger (die harte Prüfung: die Trichterlösung WANDERT) |
| `gun-dry` | leere Trommel, Ablehnung `depleted` |
| `intercept-aim120` / `intercept-dlz` | gelenkter Schuss, DLZ |
| `intercept-lostlock` | Uplinkverlust — trifft noch |
| `intercept-defeated` | Uplinkverlust + Ausweichen — trifft nicht mehr |
| `damage-amraam` | die KONSEQUENZ: Detonation, Schadensauflösung, Systemausfälle, Blockungültigkeiten und 340 s Nachspiel bis zum Aufschlag (Exit 2) |
| `make -C sim test-gun` | Streuungs-Fit gegen MIL-DTL-45500/1A, Flugzeit, Trichtergeometrie, Vorhaltelösung gegen die geflogene Bahn, Munitionsverbrauch, Ablehnung bei leerer Trommel |
| `make -C sim test-missile` | die AIM-120-Zelle open-loop (Motor/Widerstand/Trimm) |

---

## Offene Punkte

**Bekannte Lücken (im Code benannt, nicht versteckt):**

- **Kein IR-Sucher.** `FBCountermeasureSystem` zählt Fackeln und sie wirken NICHT — es gibt keinen
  IR-suchenden Flugkörper, gegen den sie wirken könnten. Steht so im Header.
- **Kein Lofting der AIM-120.** Das Lenkgesetz ist reines PN plus 1-g-Schwerkraft-Bias; eine
  Steigflug-Mittelphase (die einem BVR-Schuss reale Reichweite bringt) existiert nicht. Damit ist jedes
  gemessene `Raero` das einer flach fliegenden Runde.
- **Waffen sind im Renderer unsichtbar.** `render/stages/FBUnitsStage` und `FBSpritesStage` sind NoOps
  (in der Encode-Ordnung verdrahtet, aber ohne Inhalt) — eine Rakete, ein Store und ein Bodenziel
  existieren in der Simulation vollständig und im Bild gar nicht.
- **WASM hat keinen Freigabe- und keinen Schadenspfad.** `app/FBAppWasm.cpp` leert weder
  `Stores().TakeRelease()` noch `Guns().TakeBurst()`, hält keinen `FBGunProjectiles`-Pool und löst
  keinen Burst auf. Der Browser-Client kann eine Waffe laden und tragen (Punktmasse + Widerstand
  wirken), aber nichts verlässt den Jet. Der ganze Kapitel-5-Apparat lebt in `app/FBMissionRunner.cpp`.
- **Kein Strafing.** Kanonen-Bündel werden nur gegen `Aircraft` aufgelöst, Bodenziele deklarieren
  Fläche/Ausdehnung 0. Beheben hieße: Geschosse bis zum Boden verfolgen (heute per `kMaxAgeS`/
  `kMaxPathM` ausdrücklich nicht) UND eine präsentierte Fläche für Bodenziele setzen.
- **Bodenburst gegen Flugzeuge fehlt bewusst** (§5.4) — ein tief über der eigenen Bombe fliegender Jet
  bleibt unversehrt.
- **Keine Splitter-Richtcharakteristik.** Die Isotropie-Annahme ist die eine geometrische Setzung des
  Fragmentmodells; ein echter Gefechtskopf hat ein fokussiertes Band, dessen Lage vom Winkel zur
  Raketenachse abhinge.
- **Kein Querschnitt, keine Abschirmung, keine Splitterzählung.** Die Zelle ist ein Achsensegment.
- **Der Näherungszünder trifft immer, wenn die Geometrie stimmt.** Es gibt kein Zünderversagen, keine
  Blindgängerrate und keine Zünderlogik jenseits von Radius + Schärfzeit.
- **Kein Gewicht der Munition** (§4.1) und keine Munitionsmischung; eine homogene Runde.
- **Kein Einbauwinkel der Kanone** (`FBF16Gun`: Bore 0°/0°, weil `doc/f16/` keinen nennt).
- **Stationsgeometrie längs kollabiert**: alle neun F-16-Pylone teilen die CG-Station (FS −193 in), weil
  `weapons.md` §4.5 die Stationsdaten selbst als T4 markiert. Eine Zuladung erzeugt damit kein
  Nickmoment; die LATERALEN Versätze sind modelliert.

**Zahlen, die reine Setzungen sind und jede daran hängende Aussage tragen** (jede einzeln zu messen,
sobald eine Quelle auftaucht):

| Setzung | Wert | hängt daran |
|---|---|---|
| `kCaseFraction` | 0,5 | JEDER Gefechtskopf-Schaden, linear |
| `kFragSpeedMs` | 1800 | dito, quadratisch (dominiert `v_closure` bei ~850 m/s deutlich) |
| M61A1 `RoundMassKg` | 0,100 | JEDE kinetische Schadenszahl, linear |
| M61A1 `DragCoef` | 0,30 | Flugzeit, Trefferenergie |
| Fragilitätsklassen F-16 | 6 Werte | jedes Kill-/Degrade-Urteil |
| Fragilitätsklassen Bodenziel | 4 Werte | dito |
| präsentierte Flächen F-16 | 4,0 / 14,0 m² | erwartete Trefferzahl, linear |
| AIM-120 `FuzeRadiusM` | 10 m | ob ein Schuss ein Treffer ist |
| AIM-120 `SeekerRangeM`/`ActivationRangeM` | 14,8 / 18,5 km | Terminal-Übergabe |
| AIM-120 Sucher-FOV/Gimbal | ±10° / ±45° | ob eine Midcourse „gut genug" war |
| Mk-82 `ArmingS` | 2,0 s | die Blindgänger-Schwelle des Pull-Up-Cues |

**Ungelöste Fragen / Widersprüche:**

- **Die CCIP/CCRP-Genauigkeit hat keinen absoluten Aussagewert.** Die 22 m Gesamtfehler (davon 10,6 m
  quer, §4.2) sind gegen ein Bombenmodell gemessen, das sich selbst als möglicherweise grobe Näherung
  bezeichnet, deren einzige Ähnlichkeit mit dem echten Objekt der Name sei. Die Fehlerbudget-Aufteilung
  bleibt gültig (Guidance gegen unsere eigene Ballistiktabelle); die absolute Zahl ist kein
  Fidelity-Beleg. Ein Mk-82-Modell mit belegter Aerodynamik zu beschaffen oder zu bauen ist offen.

- **Trommelinhalt 510 vs. 512** — derselbe Guide nennt beides (§3 Spezifikationstabelle vs. §2.5 Text).
  FlightBox nimmt 510 (Spezifikationstabelle gewinnt) und notiert die Differenz statt zu mitteln.
- **Gemeinsame Währung J/m² für Splitter und 20-mm-Einschläge** ist eine erklärte Modellentscheidung
  und ausdrücklich KEINE physikalische Äquivalenzaussage (§6.3). Ob eine getrennte Schwellentabelle je
  Wirkmechanismus besser wäre, ist offen — sie wäre heute unkalibrierbar.
- **`kMinReportedHits` 0,1** ist eine Berichtsschwelle, keine physikalische: unterhalb wird gar nichts
  aufgelöst, also ist der Übergang von „Treffer" zu „Vorbeischuss" nicht ganz stetig.
- **Der Anker `IntBriefHdgDeg_` = 000** bei einer Einheit ohne Flugplan (in `damage-amraam.fbm`
  ausdrücklich als „wart worth fixing in the pilot" dokumentiert): der Intercept-Pilot verankert den
  Kurs, den sein Zustand beim ERSTEN Entscheidungstick trug — und der liegt vor dem ersten FDM-Schritt,
  also 000 statt der Spawn-Peilung.
- **`FBReleaseSolution::StampS`-Verzögerung** (§2.5): die Stores-Kommandogruppe wird VOR der
  Feuerleitung bedient, eine Runde trägt also die Lösung des vorigen Sweeps. Protokolliert (`solAgeS`),
  aber nicht behoben — die Bus-Reihenfolge wäre die Stellschraube.
- **`gun MISS` und `stores MISS` melden die dichteste Annäherung**, aber ein Bündel, das nach einem
  Treffer retiriert wird, erzeugt gar keine Miss-Zeile — die Statistik „wieviele Bündel gingen daneben"
  muss deshalb aus `gun BURST` minus `gun HIT` gebildet werden, nicht aus `gun MISS` allein.
