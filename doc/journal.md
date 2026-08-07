# Journal — the chronicle of the rounds

**What this file is:** one line per finished round, in the order they happened — commit, what it
built, what it measured. It is history, not a plan and not a contract.

- **What each area must do** → the `## Spec` section of its topic file ([`INDEX.md`](INDEX.md)).
- **What is built right now** → the `## State` section of that same file.
- **What is missing, and what was tried and rejected** → its `## Gaps` section.
- **What comes next, in order** → [`roadmap.md`](roadmap.md).

Every round adds a line here; nothing here is ever rewritten to look better. Rejected approaches are
kept — in the Gaps of the file they belong to, with their measurements.

State of the entries below: commit `793e1fe` + the model-root/delta round (2026-07-27).

## Maturity per area

| Area | State | Doc |
|---|---|---|
| FDM adapter | **finished** — instanceable, IC-sealed, damage and stores channels | [fdm.md](fdm.md) |
| Core / avionics bus | **finished** — typed blocks with three-state validity, command bus with acknowledgement | [core.md](core.md) |
| Mission orchestrator | **finished** — four steps, no mission knowledge in the code | [missions/runtime.md](missions/runtime.md) |
| Multi-unit | **finished** — cast as mission data, thread per unit in the gym, deterministic | [missions/runtime.md](missions/runtime.md) |
| Formation | **built** — roles as mission data, station keeping, target sort, cover rule; rejoin and lead-level tactics missing | [formation.md](formation.md) |
| Sensors | **built** — datalink, radar, RWR, IRST, countermeasures. Without terrain masking. | [sensors.md](sensors.md) |
| Weapons | **built** — AIM-120, Mk-82, M61A1, ground targets, damage model | [weapons.md](weapons.md) |
| Pilot AI | **in progress** — takeoff/route/landing, BFM, BVR intercept, air-to-ground all fly; refinement ongoing | [pilot.md](pilot.md) |
| Renderer | **built** — stage split complete; Flugzeuge sind sichtbar, Effekte/Waffen noch nicht. | [render/renderer.md](render/renderer.md) |
| HUD | **built** — generic default HUD + full F-16 symbology, coverage AA | [modules/f16/module.md](modules/f16/module.md) |
| Cockpit displays | **built** — three translucent MFD bays, pages chosen by the pilot AI over the command bus | [modules/f16/cockpit-displays.md](modules/f16/cockpit-displays.md) |
| HOTAS | **not started** — deliberately last, it is only a mapping | [clients/clients.md](clients/clients.md) |

## Chronology

### 2026-08-04 — Acht stille Gene waren DREI verschiedene Tatsachen, und nur eine davon war ein Defekt

**`E19` hat gemessen, dass acht von neun Genen auf der MiG-29 bitstill sind, und das als EINE Tatsache
gelesen. Es sind drei.** Diese Runde nimmt sie auseinander, jede mit ihrer eigenen Messung — und das
Instrument wurde zuerst gegen `E18`s committete Tabelle geprüft (**5 von 5 Zellen exakt**), bevor ihm
irgendetwas geglaubt wurde. Die Basislinie reproduziert `E19`s Kernzahl auf den Bit: **1 von 24 Hebeln
bewegt die MiG, und es ist `sort-near`.**

| Gen | Urteil | Beleg |
|---|---|---|
| **G5** Emission | **echte Lücke — repariert** | siehe unten |
| **G4** Energie | **war immer erreichbar; die RIG, nicht die Zelle** | `duel-merge`, wo eine MiG `Phase::Bfm` betritt: drei Allele, **drei verschiedene Trajektorien**. `sat-*` betritt BFM auf keiner Seite — das steht in `sat-07`s eigenem Kopf |
| **G1** Form ×3, **G2** Deckung | **korrekte Asymmetrie, belegt** | Lazur-M ist ein Boden-Kommandokanal, kein Rottenbild. Und die von `formation.md` F6 offengelassene Alternative — Sichtformation — ist mit dem Auge dieses Baums nicht baubar: `FBVisualContact` verweigert Entfernung STRUKTURELL und Identität immer, und bei 1 852 m Kampfstaffelung misst der Führende **0,536°** gegen eine Erkennungsstufe von **0,8°**, trägt also nicht einmal einen TYP |
| **G6** Bias, **G7** CCIP | **korrekte Asymmetrie, belegt** | auf `mig29-opt-low`, einer MiG im vollen Luft-Boden-Pass: alle fünf Allele **bitgleich**. `DirectorPass` ersetzt den generischen Abwurf, und der wartet auf einen Freigabe-Cue, den dieser Rechner nie veröffentlicht |

**G5 war eine echte Lücke, und die zwei Zahlen, die `D3d` verlangt hat, haben allein KEIN Bit bewegt.**
Es brauchte zwei Nähte: (1) die EMCON-Betätigung lief über den MODUS-Wähler, der auf dieser Zelle schon
RAD-gegen-ACM trägt — jetzt benennt `FBPilot::EmissionControl()` den SCHALTER, mit den alten zwei
Ordinalen als Vorgabe, also ist die F-16 dieselbe Codezeile; (2) `EmconSilent_` verlangte einen
kooperativen Meldeblock, las also als Code *„eine Zelle ohne Datenlink darf nie schweigen"* — die
Umkehrung der Doktrin dieses Flugzeugs. `BriefedPictureRangeM()` macht den **letzten Anruf des
Leitoffiziers** zu diesem Bild, mit dessen eigenem gebrieftem Takt als Lebensdauer. Die zwei Zahlen aus
eigener Quelle: still = **DUMMY** (`DCS-EA p.63`, „strahlt nicht" — und nie OFF, das nimmt dem Set den
Strom und sperrt genau den Rückweg, den `pilot.md` §7.6b erzwingt), Reichweite = **27,0 nm** (die eigenen
50 km des N019, T4 §7.1) gegen die 40,0 der F-16.

**Gemessen, Bit-Ebene:** `emcon-wide` bewegt die MiG jetzt auf **3 von 3** Zellen — **2 von 24 statt 1**.
Dünn, und der Grund wird gesagt statt geglättet: die committeten Zellen briefen GCI-Entfernungen von
120/90/60 km gegen ein 50-km-Tor, also liegen `emcon-tight` und `-mid` auf derselben Seite jeder
Schwelle (X-14 im Spiegel). Auf der eigenen Auflösung des Bandes (10 Punkte, die Rigs **nicht** angefasst)
sind es **4 / 6 / 4 unterscheidbare MiG-Trajektorien** auf `sat-02` / `sat-04` / `sat-07`, und die Stufen
sitzen exakt dort, wo `f × 27,0 nm` die gebriefte Leiter 120/90/60 km kreuzt — **f = 2,4 / 1,8 / 1,2**,
aus dem Missionstext hergeleitet und nicht angepasst. **Erhaltungsbeweis:** bei
`f ≥ 2,5` — „nie still" — ist die reparierte MiG **byte-gleich zur Binärdatei vor der Reparatur**.

**Die Kosten am Rest des Baums, und der Zugehörigkeitstest ist EXAKT:** 293 Missionen, **237 bitgleich,
56 bewegt, 3 Exit-Codes**. Eine Mission bewegt sich GENAU DANN, wenn sie eine MiG-29 mit
`set task intercept` und mindestens einem `brief_gci` jenseits der eigenen 50 km trägt — 56 vorhergesagt,
56 bewegt, **0 in beide Richtungen daneben**; keine Mission ohne MiG-29 bewegt sich. Die drei Exit-Codes
je auf dem eigenen bindenden Instrument ihrer Datei, und die Richtung ist konsistent: eine MiG, die auf
das Wort des Leitoffiziers schweigt, wird **nicht stärker** — sie baut kein eigenes Bild, während die
F-16 ihres über den Datenlink behält.

**`X-19` repariert, und zwar prüfbar:** `Genome.line()` druckt den Kanal-Bit jetzt immer, die Vorgabe ist
„die Mission in Ruhe lassen", und **jede Archivzeile wird beim Schreiben gegen ihren eigenen Parser
geprüft**. Die drei `dl`-Zustände ergeben drei Zeilen und drei verschiedene Missionen; vorher druckten
zwei davon identisch.

**Die Ko-Evolution neu gefahren, 2 220 Läufe, 2 Generationen — und der Gegner ist zum ersten Mal kein
Fixpunkt.** Rots Champion BEWEGT sich in Generation 1, und zwar in genau dem Gen, das diese Runde
repariert hat: `r0_s4` (`emcon 1,5 sort=none`) → **`r1_02` (`emcon 3` sort=none)**. Zwei rote Champions
statt einem, Archive **2/3 statt 1/1**, und Rots Population ist nicht mehr flach (`emcon 0` erreicht
0,536 gegen 0,484 der Saat). **Blau steht weiter ab Generation 0 fest**, also ist die Entartung
VERRINGERT und nicht behoben — der feste Maßstab ist auf beiden Seiten flach, Instrument (b) mit 2 bzw.
1 Champion weiter nicht rechenbar, und veröffentlicht wird daraus nichts. Das Genom wurde je Seite auf
die Gene beschnitten, die sie auf DIESER Arena ausdrücken kann; die Beschneidung steht VOR dem Lauf und
folgt den committeten Kopfzeilen der Zellen. Und die Archive sind jetzt nachfliegbar: jede blaue Zeile
trägt das `dl=off`, das `E19`s Archive verschwiegen haben, und jede Zeile erzeugt beim Zurücklesen
wieder ihre eigene Mission.

**Zwei Defekte nebenbei — einer repariert, einer gebucht.** Repariert: `FBRadarSystem::Powered_` steht
auf `true`, `FBMig29Radar::Emit_` auf `OFF`, und `SetEmission` kehrt bei unverändertem Zustand früh
zurück — der Block meldete ab Spawn ein lebendes Set hinter einem toten Schalter. Gebucht (`X-20`): eine
F-16, die eine EMCON-Stille beendet, erfasst **sofort** — `pb2` kommt nach 31,2 s Stille mit **8 festen
Kontakten im selben Takt** zurück, weil das Frame-Raster beim Moduswechsel nicht resynchronisiert wird;
die MiG im selben Lauf tut das nicht, weil `SetEmission` `ResyncScan()` ruft. Nicht in dieser Runde
repariert: das bewegt jede F-16-EMCON-Mission und braucht seine eigene Regression.

**Tore:** `make gym` · `verify-layers` (6 Registry-Leser, unverändert) · `verify-guards` 8/8 ·
`verify-models` grün · **10 von 10 Harnesses rc=0** · Determinismus `--threads 1/2/4` byte-gleich auf
`sat-07` und `duel-merge` · `sim/vendor` und `sim/assets/aircraft` unberührt · keine Commits.

### 2026-08-04 — Fünf saubere Zellen, die Arena besteht — und der Sweep darüber findet nichts

**Die Decke ist erreicht, das Ergebnis darunter ist negativ.** `E17` hinterließ zwei chaos-saubere
graduierbare Zellen und p ≤ 0,250. Diese Runde baut drei weitere, das Gate nimmt alle drei an —
**ARENA: PASSED, 8 Zellen, 5 informativ, Signifikanzdecke 2⁻⁵ = 0,031** — und der Hebel-Sweep über die
fünf liefert **keine zulässige Verschiebung**. Beides ist das Produkt, nicht nur das erste.

**Das strukturelle Mittel ist das TROCKENE Gefecht.** `brief_master_arm sim` auf BEIDEN Seiten: die
Hardware verweigert jeden Start, aber Sortierung, Lock, Crank, RWR-Antwort und Emissionsdisziplin
laufen unverändert. **[MESS]** 63 `sms RELEASE_REJECTED`, 0 `damage KILL`, 0 `monitor KO`. Die Folge
ist eine Zahl: die Chaosamplitude einer gewerteten Verweilzeit fällt von **1,0 s** (sat-03/sat-04, nass)
auf **0,10–0,30 s**; ein `task formation`-Transit ohne Gegner liegt bei **0,1 m / 0,025 s**. Der
diskrete Einschlag ist der Verstärker — nimmt man ihn weg, bleibt eine 3-m-Störung eine 3-m-Störung.

**Die drei Zellen, je auf einer der tragenden Grundlagen, Marge im Kopf:** `sat-07-dry-merge`
(deklarierter Zylinder, Sprossenleiter, **3,20 s gegen 0,30 s = 11×**), `sat-08-ident-qra` (geometrische
`identify`-Box, 42 Sprossen, **56,0 m gegen 5,58 m = 10×**), `sat-09-gate-strike` (Freigabe-Gatter,
`ccip-tight` 4 Freigaben → **0**, dazu 21,3 m Marge gegen den 45-m-Wirkradius). Alle drei: S1 ok, S2 ok,
**S7 0 von 8**.

**Zwei Bauregeln fielen dabei an, beide Instrumentenbefunde.** (a) Die Ergebnisklasse ist eine SUMME —
ein Hebel, der ändert WELCHES Ziel erfüllt ist, ohne zu ändern WIE VIELE, ist ihr unsichtbar; vier
Entwürfe hatten **10 lebende Hebel und 2 Klassen**. Die Reparatur ist eine monotone Sprossenleiter auf
EINER stetigen Größe. (b) Jede Sprosse sitzt in der Mitte einer GEMESSENEN Lücke und wird nur emittiert,
wo die Hebel-Lücke ≥ 10× die GEMESSENE Chaosspanne derselben Größe ist — per Werkzeug hergeleitet. Diese
Regel verwarf `sat-08`s naheliegende Achse: dieselben acht Paare über die DWELL gewertet erreichen 5×,
über die MINDESTENTFERNUNG 10–3 180×. Die natürlich aussehende Achse war die Münze.

**§4a wurde erfüllt, nicht umgangen — und der erste Versuch war zweifach falsch.** `stores IMPACT
lat=/lon=` wird mit `%g` gedruckt, also auf **9,3 m** quantisiert; und der CCIP-Einschlag FOLGT dem
bezeichneten Ziel mit einer Verstärkung unter 1, ist also eine Fixpunkt-Iteration. Iteriert: Grundlauf
**0,0 m** vom Ziel, beide Bias-Schienen **±23,7 m** gegen 45 m — **21,3 m Marge**, `bias-early`/`bias-late`
inert. Davor war `bias-early` auf allen drei Zellen ein Beweger mit **3,0 m** Marge, und **S7 ließ ihn
mit 0 von 8 durch**: ein Schirm für den Anfangszustand sieht keine Schwelle, an der das GENOM klebt.

**Der Sweep, auf der Ergebnisklasse gelesen:** kein Hebel ist auf 5 von 5 besser. Die zwei, die p = 0,031
erreichen (`bias-rail`, `ccip-tight`), sind beide SCHLECHTER und beide auf einer Schiene ihres Bandes —
§6 §2 verweigert das. Der stärkste positive ist `pilot_emcon_frac` 1,0 → 0,4 mit **4 von 5, p = 0,188**,
nicht monoton im eigenen Gen (0,1 gewinnt 3 von 5, 3,0 gewinnt 1 von 5): ein Punkt, keine Richtung. Er
besteht X4a (0 von 8 mit seinem EIGENEN Genom auf allen fünf), X4b (hält bei `timeout` × 1,5) und X4c,
und ist trotzdem nicht veröffentlichbar. Sein Mechanismus ist benennbar (`flt_switch` 110 → 29,
`sort_assign` 316 → 69, `flt_assign` 12 → 22: leiseres Radar → weniger eigene Echos → die Zuteilung
BLEIBT) — ein benennbarer Mechanismus kauft keinen p-Wert.

**Die schärfste Warnung der Runde:** in der VOLLEN Ordnung (V → M → C) erreicht `bias-late` p = 0,031 auf
5 von 5 und sitzt auf keiner Schiene. Auf der Klassenebene ist er `-....`. Die ganze „Signifikanz" ist
`C_aim`. §6 nennt einen Rangwechsel innerhalb von C als Erstes unter dem, was ausdrücklich kein Befund
ist — **und nichts im Werkzeug erzwingt das.** Sechs Exploits gebucht (`X-9`…`X-14`).

### 2026-08-03 — Die Scheibe ist die Zielfläche: durchscheinende MFDs, ein entkerntes HUD — und der Projektor war 20 Grad zu weit

Drei Sätze des Eigners, drei Messungen.

**1. Die MFDs zeigen jetzt die Welt.** Bisher lag die untere Rasterreihe im Schwarz, das der
Szenen-Viewport dort übrig ließ. Statt eines Scherenschnitts trägt die Projektion den Versatz: die
VOR dem Raster gültige Vollbild-Projektion plus EIN Term, ein konstanter NDC-Versatz
`shift = 1 − hVp/hFull` auf der y-Zeile der z-Spalte, der die Visierlinie von der Bildmitte in die
Mitte der SCHEIBE hebt. Der Szenenpass hat seitdem weder Viewport noch Schere; die Welt läuft hinter
der Bank weiter, und jeder Schacht bekommt davor ein Schleierquad. **Die Deckung ist keine
Geschmacksfrage, sondern eine Rechnung:** der HUD-Pass mischt linear, es bleibt `(1−a)` vom Untergrund,
der hellste GEMESSENE Schachtuntergrund (99,5-Perzentil) ist der weiße SVS-Boden mit L = 0,93, und
HUD-Grün (L = 0,740) braucht 4,5:1 → **a ≥ 0,865**, gesetzt **0,87**. **[MESS]** zurückgemessen auf
`payerne-full`, je Schacht, Tinte ausgenommen: **Grün 4,67–5,91:1, Bernstein 3,45–4,37:1**. Der
Durchlass selbst: 0,23 des linearen Werts bei a = 0,78 — die Mischung ist exakt `1−a`.

**2. Die Beschnitt-Eigenschaft überlebt den Umbau, und das ist gemessen.** Zwei Frames, gleiche
Kamera, Nick 0 gegen −20 Grad, die Himmel/Boden-Kante spaltenweise gelesen: das **Schnittmodell**
(K = 623,5 px je Tangenteneinheit) sagt die Zeile mit **Median-Residuum −0,22 px** über 107 Spalten
voraus, das Briefkasten-Modell liegt **76 px** daneben. Pixel je Radiant ist, was es war.

**3. Das HUD ist entkernt UND es füllt endlich das Fenster.** Gefallen sind Bank-Skala (→ SYS),
Bullseye, Restflugzeit und Schrägentfernung (→ HSD) — jede Zahl steht weiter auf einem publizierten
Block, keine ist gelöscht. Geblieben sind Geschwindigkeitsvektor, Nickleiter, Horizont, die drei
Bänder, Raute mit Kaulquappe und EINE Steuerzeile. Gezeichnet wird nicht mehr die ~25°-Apertur,
sondern die Scheibe selbst (1260 × 460 statt 520 × 348) — eine benannte Abweichung von der Quelle,
kein Lesefehler.

**Und dabei fiel ein Defekt heraus, der die ganze Klage erklärt:** der konforme Projektor des HUD
rechnete mit **80 Grad** Bildwinkel, die Szene mit **60**. Die Symbolik war also nie konform — sie war
um 623,5/429 = **1,45 zur Visierlinie hin gestaucht**, was ein gutes Stück des „klebt in der Mitte"
war. Beide lesen jetzt EINE Konstante (`core/FBCamera.h`). **[MESS]** HUD-Horizont gegen die
Projektion bei Nick +10/0/−10: 365,1/254,5/145,1 gemessen, konform 363,9/253,5/144,0 — Residuum ≤ 1,2 px;
der alte Projektor hätte 325,3/249,3/173,9 gezeichnet, bis 40 px daneben.

**Was nicht gemessen werden konnte, steht als Lücke da:** die Regression über alle Missionen ist in
dieser Sitzung NICHT zurechenbar, weil ein zweiter Agent zeitgleich die Missionsschleife im selben
Baum umbaut. Der statische Beleg steht dafür ein: `BuildHud`/`BuildMfd` werden ausschließlich von
`render/stages/FBHudStage.cpp` gerufen, und `fb-gym` linkt keinen Renderer — kein Pfad dieser Runde
ist von einer Missionsschleife aus erreichbar. Ebenso konnte kein Browser-Bildschirmfoto entstehen
(keine Bildschirmaufnahme-Berechtigung, nur Safari, Apple-Events gesperrt); die Bildbelege kommen aus
`gpu_native`, dem Frame-Orakel derselben Quellen.

### 2026-08-02 — Das Cockpit als Fenster in die Aufmerksamkeit der KI: drei MFDs, und der Pilot schaltet sie über den Bus

Der Bildschirm ist ein **3x3-Raster**: obere zwei Reihen Aussenansicht + HUD, untere Reihe drei
Multifunktionsdisplays. Kein Overlay — die Szene bekommt einen echten **Viewport** und die Projektion
wird so korrigiert, dass sie BESCHNITTEN und nicht gestaucht wird (`f` skaliert mit `hFull/hVp`, der
horizontale Term `f/asp` bleibt bitgleich). Die Bank zeichnet in dieselbe `FBHudGeometry` wie das HUD:
**`passcount` bleibt 6 / 7 mit Wolkendeck / 5 ohne HUD.**

**Der Kern ist nicht das Layout, sondern wer schaltet.** Ein Display wechselt seine Seite, weil
`FBPilot::SelectCockpitPage` eine Bedienhandlung gepostet hat — neues Kommandoziel `MfdPageSelect`,
HOTAS-Klasse, Wert = das Seitenordinal DIESES Moduls. Damit sieht der Zuschauer, worauf die KI gerade
schaut. **[MESS, `mig29-intercept`]** die MiG spawnt mit `n019_emission off`, es GIBT also keine
FCR-Seite, und der Pilot nimmt bei t = 0,0 die RWR-Seite; das Emissionskommando wird bei **t = 27,9**
quittiert, die FCR-Seite entsteht, und im selben Takt postet er `mfd_page 0` (Quittung t = 28,4).
**[MESS, `payerne-full`]** drei Wechsel in 734 s: SYS bei 0,0, HSD bei 16,0 (der Nav-Block kam hoch),
SYS bei 663,2 — die ALOW-Warnung ging im Anflug an, und die SYS-Seite zeigt genau sie in Bernstein.

**Die Schichtung liegt im KATALOG, nicht in einer Unterklasse.** `systems/FBMfdSystem` hat kein einziges
virtual; jedes Modul meldet einmal seine Seiten an. F-16 `{FCR, SMS, HSD, RWR, SYS}` — Datenlink ja,
IRST nein, wie ihr `NotImplemented` auf `IrstMode`. MiG-29 `{FCR, IRST, SMS, RWR, SYS}`. Darüber
schneidet die **Beladung** aus dem publizierten `FBStoresBlock`: leere Rails und leere Trommel = keine
SMS-Seite, und zwar mitten im Einsatz.

**Das HUD wurde entkernt**: G, Spitzen-G, Mach, ARM/SIM, Radarhöhe und ALOW sind nach unten gewandert.
Geblieben ist, womit man zielt und navigiert. Gelöscht wurde nichts — jede dieser Zahlen steht auf einem
publizierten Block und wird auf SYS oder SMS gezeichnet.

**Die Regression bewegt sich in genau einer Dimension.** Über 281 Missionen: **0 Exit-Code-Änderungen**
und, nach Abzug der neuen `mfd_page`-Zeilen und der dadurch verschobenen `seq=`-Nummern, **0 echte
Unterschiede im Ereignisstrom**. Auf einer geschichteten Stichprobe von 10 Missionen wurden alle
Telemetriespalten Zelle für Zelle verglichen: bewegt haben sich **ausschliesslich** die sieben
`cmd_*`-Buchhaltungsspalten. Keine Flug-, Sensor-, Waffen-, Engagement- oder Urteilsspalte.

**Ein Defekt fiel dabei an und wurde behoben statt gebucht:** der Pilot wiederholte einen Seitenwunsch,
solange die Quittung ausstand, und kassierte zwei `channel_busy`-Ablehnungen in `four-4v4-asym`. Das
Werkzeug dagegen steht seit Langem im Bus (`FBCommandBus::SwitchReady` — *„gefragt statt geraten"*);
mit ihm sind die Ablehnungen wieder Zeile für Zeile die des Basisstands.

### 2026-07-29 — Koordinatenrahmen: dreimal derselbe Fehler, also wurde der TYP gebaut und nicht die vierte Stelle geflickt

Drei Runden hintereinander fanden je einen Defekt derselben Art — ein **Weltwinkel** in einem
**körperfesten** Befehl. Statt den nächsten zu flicken, wurde der ganze Baum abgelaufen: **41
Winkelübergaben** inventarisiert, jede mit Erzeuger-Rahmen, Verbraucher-Rahmen und Urteil
([`sensors.md`](sensors.md) §10 — die korrekten stehen mit drin, eine Liste nur aus Fehlern sagt nichts
über Abdeckung). **Vier waren falsch, alle vier in derselben Richtung, alle vier behoben.** Drei waren
einzeln bekannt (O5, Katalog-Runde, W5), der vierte — die gebriefte GCI-Höhe der Katalogzelle — fiel
beim Ablaufen an.

**Die drei Zahlen.** `o5-02-scramble`: die MiG-Rotte steigt mit +5,65…+6,01° Nick, der Angriff steht bei
−3,41…−3,74° körperfest, die N019-Suchleiste ist ±6,0°; kommandiert wurde **+2,891°** (Unterkante
−3,109°, der Angriff 0,3–0,6° darunter) — **null Radarkontakte in 700 s**. Jetzt **−2,754°**: erster
Kontakt **t = 48,0 s auf 25,70 sm**, vier R-27R, **beide Angreifer abgeschossen**, Exit **3 → 0**. Der
Live-Netz-Cue der Katalogzelle: **+0,025° → −3,358°** auf `air-awacs-cue` (eine steigende MiG-25). Und
der **Spawn-Tick**: `FBMissionBoot` veröffentlichte einen Zustand mit NUR der Position, also lief für den
ganzen Tick 0 jede Körpertransformation im Baum gegen die Identität — Empfänger wie Strahler. Auf der
committeten `pair-2v2-f16.fbm` meldete viper1 eine Feuerleitung bei **brgDeg = −180,0** mit Signal
0,9998, wo die geflogene Geometrie −0,0 ist.

**Strukturell gesichert statt gezählt.** Der Rahmen ist jetzt ein **Typ**: `core/FBBodyAngle` ist über
genau drei benannte Konstruktoren erreichbar (`FromTrueBearing`, `FromWorldElevation`, `Measured` — die
EINE Hintertür, benannt, damit ein unverdienter Gebrauch an der Aufrufstelle sichtbar ist). Es gibt keine
Syntax für „nimm einfach dieses double". `FBCommandBus::PostAntennaAz/El` nehmen nur diesen Typ und sind
die **einzige** Stelle im Baum, die `FBCommandTarget::RadarSlewAz/El` in einem Post nennen darf;
`make -C sim verify-layers` druckt **`1 antenna-cue poster(s)`** und fällt beim zweiten — dieselbe Form
wie die Zahl der Registry-Leser daneben. Die Bodenstellung ging durch dieselbe Umrechnung, obwohl sie
schon richtig war (eine Lafette veröffentlicht Roll = Nick = 0): arithmetisch ein No-Op, byte-identisch,
und die Richtigkeit hängt jetzt an der Transformation statt am Zufall.

**Der Preis, offen ausgewiesen.** Der Spawn-Tick-Fix bewegt **190 von 193** Missionen, weil er die ersten
0,01 s der Regelung jeder Zelle mit Zellen versorgt, die ihre echte Lage UND ihre echte Geschwindigkeit
kennen. Die reinen Rahmen-Fixes allein bewegen **35** — genau die Missionen mit `brief_gci` oder Netz-Cue.
Acht Exit-Codes wandern, jeder einzeln belegt und jeder eine Messerschneide am Zünderradius: `net-belt-high`
(V-750-Zündung 11,55 → 7,82 m gegen 12 m Zünder ⇒ Flugsteuerung *degraded* → *failed*), `bvr-duel-decided`
(2,36 → 9,67 m gegen 10 m), `cm-beam-only` (7,83 → 9,94 m), `duel-doctrine-mig` (9,35 → 13,22 m gegen
13,8 m), `o4-04`/`o4-06` (Trades statt Entscheidungen), `o4-09` (die Nacht-Messung schrumpft von sechs
Spalten auf eine, weil ihr Tageskontroll-Kampf jetzt bei 29,5 s statt 76,9 s endet), `o5-02` (siehe oben).
Die halbe Variante — nur die Lage statt des ganzen Zustands — wurde gemessen und verworfen: sie bewegt
158 Missionen und lässt das Artefakt dahinter stehen (ein Jet mit 5° Nick und 0 kt).

**Tore.** `core-lib gym native wasm` warnungsfrei, sieben Harnesses rc = 0, `--threads 1/2/4` identisch
über alle 193, `verify-models` grün (1 deklariertes Delta), `verify-layers` 301 Dateien / 6 Registry-Leser
/ **1 Antennen-Poster**. Alle fünf gebauten Kampagnen bestehen beide Determinismus-Kriterien erneut —
9 Läufe je ein Fingerabdruck, 10/10 Schritte einzeln nachgespielt; die neuen Werte stehen in den
`## State`-Abschnitten neben den alten mit Datum.

### 2026-07-28 — `C7` built: eighteen catalogue rows, ten generated decks, and the gate they do not pass

`modules/air/` exists: ONE class with eighteen `core/FBAircraft.h` rows, ten JSBSim decks GENERATED by
`tools/gen_air_decks.py` from eight published anchors apiece, eight kinematic movers, five pilot tiers,
seven new rounds, six new guns, and one new sensor slot (`sensors/FBNetLinkSystem`, the controller feed
that needed a block of its own because a fighter's Datalink block already carries Link-16 PPLI).
The perception boundary did not widen: **6 registry readers**, unchanged. All 133 existing missions are
byte-identical at `--threads` 1/2/4.

**The round's own gate says the round is not finished, and that is the entry.** `make -C sim test-air`
measures each deck's eight anchors against the bands `flight-model-recipe.md` §7.1 derived from the
MiG-29 deck's own misses: **0 of 10 rows `ACCEPTED`, 10 of 10 `ALPHA`.** A2 (Vmax at sea level) is
inside ±5 % on all eight rows that publish one (−0.3 %…−1.4 %) — the closed-form inversion reproduces
itself exactly where it was inverted. A1 (Vmax at altitude) misses on nine of ten, always low, −9.5 %
to −58.1 %; `mig25`'s worst-in-set result was predicted by R2 in advance. Mass closure is inside ±1 %
on ten of ten.

**Three steps of the recipe did not survive contact with the data, all recorded as R11–R13.** A4 can
invert a subsonic `CD0` for NO row — the published climb rates imply negative drag on two rows and 0.0038
on the F-5E against its own published 0.0200 — so it became a probe and the subsonic level is taken from
the catalogue's one published `CD0`, the same generalisation §4.1 already makes for `e`. A deck without
a throttle channel cannot light its afterburner (F-5E: M 1.14 instead of M 1.63). A deck whose mains sit
0.06 L behind the CG cannot rotate at all (take-off run 4 643 m against a published 610).

**The attribution instrument is built and it answers.** `tools/fb_tournament.py --attribution <row>`
prints `band_deck` against `band_doctrine` and, below the 0.25 rule, prints the two bands INSTEAD of a
result. `mig21` 2.4/38.5 = 0.061 · `mig23` 4.5/32.3 = 0.140 · `f15c` 1.9/39.5 = 0.048 · `su27`
3.7/1231.7 = 0.003 · `mig25` 0.5/0.0 = ∞, not admissible. The control cell disagrees with `band_deck`
on `mig21` and is a no-op on `f15c`; §Spec 11 calls that a defect of the instrument and it is booked as
one.

### 2026-07-28 — clouds: the proof set becomes reproducible, and the grain is measured rather than assumed (this round)

The R5 rebuild was built and merged with three things open. All three were taken to a number; two of
them ended somewhere other than where the gap text expected, and that is recorded as such.

**The proof set is reproducible now, and the recipe is the finding.** The stored PNGs could not be
reproduced from the committed source (99.88 % of pixels differed — tuning drift, re-measured on the
merge). The cause of the *irreproducibility* was never the clouds: the screenshot venue streams tiles
while it renders, so a short run frames a half-built quadtree. Holding the camera still for 180 frames
and writing only the last one puts the streamer at `pending=0`, and the converged tile set is a pure
function of the camera — **12 frames, two independent runs, byte-identical sha256, in both SVS and
EVS**. The consequence is that the fly-through had to become a LADDER through one column rather than a
flown mission: a moving camera never converges, so a flown sequence cannot be hashed. Named in Gaps.

**The march grain fell 13 %, not the 50 % the gap hoped for, and the honest reason is in the way.**
Removing the erosion term entirely dropped the grain from 0.0263 to 0.0149, so the erosion — 1.6 km
wide, 300 m tall, against a 260 m step — is what a 6–12 node march undersamples. Two changes went in:
a **composite trapezoid** in place of the jittered rectangle rule, and a **band-limit of the erosion
against the march's own step** (`ErodeFlat`, a new parameter of the SHARED density function, so
`--cloudcheck` still covers it — AGREE at 1.90·10⁻⁵). Grain 0.0329 → 0.0285 (3×3 high-pass), cost
+5 %. The bigger win is one the gap did not ask for: the rectangle rule rendered a thick deck **3.7 %
too dark**, and the trapezoid renders it to +0.8 % of the converged reference. Five alternatives were
built, measured and rejected — no jitter (−58 % grain, but 72 px contour bands), half-amplitude jitter,
a front-loaded geometric step ladder (+39 %, worse: it is right for a steep crossing and wrong for the
grazing far field), a stratified 4×4 ordered jitter (−1 %: stratification fixes the block mean, the
artefact is per-pixel), and an amplitude fade of the erosion, which measured BEST and was rejected
because the improvement was a 3.5 % brighter deck in the metric's denominator.

**One ceiling, three étages: a weight, not a choice.** The predecessor clamped the reported ceiling
into the band of the lowest broken deck and carried two discontinuities — the base stuck at the band
edge while the ceiling walked on (4.2 km of assertion the data contradicts, measured), and the choice
of deck flipped whenever a cover crossed 0.5. It is now an ownership weight with a 2 000 m handover:
étages hand the ceiling over continuously, a deck below a tenth of the sky cannot own one, and the
outermost edges saturate because there is nothing to hand to. The Payerne proof corridor is unchanged
to the metre. The price is named and measured: during a handover the deck slides at up to 325 m/km
against the data's own 135 m/km.

**And the question the owner actually asked — do you see them when you fly?** Yes, on this run.
Probing the committed fixture point by point: over the Swiss box (90 points) 60 % of points carry
≥ 25 % cover in some étage and a ceiling is reported at 71 % of them; Payerne itself is 75.7 % low
cloud with a 2 991 m ceiling. The distribution is strongly bimodal — that is GFS' own layer
diagnostics, not a defect here — and one run is one atmosphere, so a mission that needs a guaranteed
sky still has to say `wx fixture`.

One defect was found that predates the round and is NOT fixed: **the underside of an optically thick
deck receives no light**. The slant optical depth to the sun is ≈ 57, all three multi-scatter octaves
evaluate to zero, and the base ends up the ambient floor times the sky — dark blue where a real
overcast is bright grey. It is an ambient/multi-scatter model change, not a march change, so it is
named with its numbers instead of patched in the middle of a proof round; the cirrus frame shows the
same code producing Beer damping and a silver rim as soon as the deck is thin.

Gates: all targets clean, `verify-layers`/`verify-models` green, nine harnesses rc=0, `fb-gym` still
GPU-symbol-free, WASM builds, `--cloudcheck` AGREE, and 14 telemetry CSVs over six missions
byte-identical to `HEAD` — the clouds do not touch the physics, and that is now measured rather than
argued.

### 2026-07-28 — out of single fighters, an air force: the FLIGHT

A "flight" was an appearance. `fl` in the datalink's contact filter meant "the first unit of that
faction in mission order", two jets of one side flew two private wars, and nothing in the tree could
tell whether they had prosecuted the same target while a third went unengaged. This round makes it a
mechanism, in five pieces, and every one of them is a no-op without the declaration that turns it on.

**Roles are mission data.** `flight <name> <position>` in a `.fbm` unit block, beside `team` and for
the same reason: it is both mission data and world identity (`core/FBFlight.h`, `FBUnit`), so the
cooperative datalink reads it off the registry as it reads the team. Position 1 is the lead, a flight
without one is a parse error, and `fl` now selects it.

**The wingman holds a station on a moving point**, in two channels that never talk to each other:
across and vertical through the path law that already exists (`SetDirectLeg` onto the LEAD's course
line through the station), along through the throttle at `sqrt(2·a·|e|)` with the airframe's own
measured brake authority. That separation is the law — a `Direct` at the station is pursuit of a point
moving at combat speed, which is the regime that produced the merge roll problem. Measured
(`pair-formation.fbm`): **45.2 m** median station error on a straight leg, 1,937 m peak through a 90°
turn, no standing offset.

**The sort is three levels of information**, applied in order of worth: what a mate SAYS (a target
POINT, correlated against one's own echoes — never an identity, because this radar does not know whom
it sees), what the flight can WORK OUT (a greedy minimum-cost matching on time-to-a-shot, identical on
every member), and what was AGREED before takeoff (`set brief_sort`, the only sort an aircraft without
a cooperative terminal has). Measured: the cooperative pair holds different targets in **93 %** of the
ticks both were assigned, and `dup && free` — a target engaged twice while another was free — is
**0 over every unit of every formation mission**.

**Cover is one rule whose price is the weapon.** A member does not fire a round that would bind it
while a mate is already bound; a bound shooter flies at its own antenna and a flight with nobody free
cannot answer a launch. Measured: an AIM-120 binds **0.3 s**, an R-27R **17.3 s** — a factor of 58 —
and `pair-cover.fbm` measures **7.8 s** of real deferral with the flight never left uncovered. The
MiG cannot apply the rule at all, because "I am bound" has no channel on its aircraft; that is the
round's sharpest finding and it is the doctrine contrast arriving at its consequence.

**The asymmetry as one number** (`four-4v4-asym.fbm`, same run, same geometry): distinct targets per
engaged member, cooperative **0.962** against briefed contract **0.750**.

Five missions (`pair-formation`, `pair-2v2-f16`, `pair-2v2-asym`, `pair-cover`, `four-4v4-asym`), one
analysis tool (`fb_flight_report.py`), a `--flight N` mode for the tournament with `dl=`/`sort=` on a
variant line, 14 appended `flt_*` columns. **All 79 stock missions byte-identical** on every column
they ever had and line for line in `events.log`; one fingerprint per new mission over `--threads
1/2/4` × 3. Rejected with their measurements: symmetric yielding (a 1 Hz oscillator, 60 consecutive
swaps), age-compensating the contact range (switches 33 → 87), a blink hold (no effect), and capping
the along-track correction at one spread (a wingman stuck 40 km out for 230 s). Full contract,
derivations and gaps: [`formation.md`](formation.md).

### 2026-07-28 — the F-16's roll law gets an END: the merge becomes two-sided

The previous round moved the merge's blocker onto the F-16: with the MiG acquiring and flying aggressive
locked pursuit, the F-16 departed `duel-merge` at t=18.0 (`LOC`). **Diagnosis first.** The regime is not
"a reversal": it is a head-on pass at **898 kt of closure** whose line of sight sweeps at up to
**543 °/s** — 34 × the airframe's corner turn rate — against `gun-bfm`'s 17.7 °/s and `bfm-basic`'s
6.2 °/s. There the commanded lift direction rotates WITH the aircraft, the roll error never closes, and
the law rolled **290° in 3.1 s** on a steering error of 10–20°. Neither existing guard sees it: the
conversion guard has exactly this premise but tests the target's angular OFFSET (its zone floors at 90°,
the merge never exceeded 76.6°) and only freezes the turn SENSE, which permits an unbounded roll in that
sense; the rate cap did bind and held 103–109 °/s against its declared 90.

**The fix is the missing half of the same closed form, not a fourth hook.** §5.7.2 derived the cap as
"180° in `kBfmTurnTimeS`" and applied it as a PEAK — next to a judge whose rule is a SUSTAINED quantity
(|ω| > 60 °/s for 3 s), so 90 °/s is 1.5 × the threshold and survivable only while no geometry holds it.
The law always takes the short way, so no correction can require more than 180° of roll in one direction;
the same sentence therefore also bounds the roll flown per window. The cap is scaled by the share of a
reversal still open (`cap · (1 − |∫p dt over kBfmTurnTimeS| / 180)`), which is exact at an empty window
and has the fixed point `p = cap/2` — 45 °/s for the F-16 — because `cap · T = 180`. That margin against
the judge is thin on purpose: measured, it is what the softer variants do not have.

**Measured.** `duel-merge`: longest |ω| > 60 stretch **2.9 → 0.8 s**, roll per 2 s window
**195.8 → 110.8°**, run **18.0 → 232.3 s**, no F-16 departure; viper `lock_s` 16.1 → 28.2, fulcrum
14.2 → **79.4** and the campaign's first WVR employment (12 GSh-301 rounds, t=195.2, miss). 16-approach
sweep (now a committed tool, `sim/tools/fb_bfm_sweep.py`, instead of a scratch script every round has
to guess back): departures **6 → 0**, kills **5/16 → 7/16**, peak roll rate **182.2 → 103.4 °/s** (2.02 × → 1.15 ×
the cap), seconds above the cap **63.2 → 5.4**. Rejected with their numbers: the bang-bang form of the
same constraint (sweep departures 3, `gun-bfm` loses its kill) and giving the MiG back the derived 90 °/s
peak (restores `mig29-bfm`'s control position, costs a MiG departure at t=103.6).

**Re-baseline:** 79 missions, **73 byte-identical**, 6 moved, ONE verdict change — `gun-dry` 1 → 3, whose
twelve rounds all still arrive but into the nose zone at 25.7 kJ/m² instead of the centre at 153. Two
costs are declared rather than explained away: `bfm-blind` and `mig29-bfm` lose their control position
(`bfm_ctrl_s` 48.4/88.2 → 0.0), causally, because a sustained conversion roll is now half the peak. A
matching chaos measurement bounds how much of the sweep is signal at all: perturbing `gun-bfm`'s spawn in
0.8 m steps over ±3 m flips it between KILL (t=77.9…197.1) and no kill in 2 of 8 samples. Determinism
1/2/4 threads × 3 on all six moved missions plus `payerne-pair`/`-four`; nine harnesses, `verify-models`,
`verify-layers`, `nm`, native frame proof out of the merge, WASM rebuilt and hash-verified against the
baseline build. What the merge STILL does not test is the R-73/GSh-301 thesis, and the reason is now
honest: 190 of its 232 s have the F-16 blind, 143 the MiG, 133.6 both — after the first pass neither ACM
box re-acquires, both jets sink and the MiG loses that race. New gaps: `pilot.md` 2.8 (the lift-vector
law is singular for a downward demand above 1 g — the mechanism that TRIGGERED the roll) and 2.9
(close-combat re-acquisition + the BFM floor).

### 2026-07-28 — MiG-29 value round: the merge acquisition and the second dispenser

Two coupled gaps, both closed. **(A) The N019 acquires in a turning merge.** Its documented close-combat
modes (CC/VS/BORE) are azimuth PENCILS (±1.75°/±1.5°/±1.25° — the vertical reading DCS-FM p.12 forces),
and a pencil cannot hold a manoeuvring nose-on merge target: `duel-merge` fulcrum lock_s was **0**. A
BROAD forward auto-lock volume was added, `FBMig29Radar::kAcm*` — ±37° azimuth [T4 §7.1, read as azimuth
against the vertical reading CC takes; the two sources genuinely conflict], a [SET] ±30° nose-centred
vertical band (measured threshold: ±25° never firms, ±30° acquires, wider buys nothing), Doppler-EXEMPT
like CC (a co-speed merge is the low-closure case), frame **0.75 s** [DERIVED from T4's "1-2 s lock" over
the generic two-look firming, the same construction as the RAD 3.0 s frame]. The pilot SELECTS it in the
fight phase: a new generic hook `FBPilot::BfmRadarModeOrdinal` + `BfmSelectRadarMode`, the F-16 leaving it
−1 (byte-identical, its `acm_hud` is mission-set). Measured: `duel-merge` `n019 MODE acm` t=0.5,
`RADAR_LOCK` t=3.8 (3.32 nm), **lock_s 14.2**; `mig29-bfm` improves 203→296 lock_s / 5.3→88 ctrl_s.

**(B) The BVP-30-26 dispensers.** `modules/mig29/FBMig29Cmds`, a `sensors/FBCountermeasureSystem`
override: 60 cartridges [DOC], a [SET] 30/30 split (a named source gap), 5/5 BINGO and the three geometry
programmes on the generic slot machine ([SET] values, schema from the source — the F-16 ALE-47 pattern).
Wired in (cycled RWR→CMDS at 10 Hz, `CmDispense`/`CmConsent`/`CmdsMode` routed, `cmds_*`/`brief_flare_s`
keys, gated on the `Countermeasures` health id the damage layout already zoned). Its flares seduce the
AIM-9 through the SAME deterministic model that seduces the R-73 (`sensors/FBIrstSystem::SelectFlare`):
`mig29-defend.fbm` measures `FLARE_SEDUCED tgtIntensity=0.16` (head-on/dry) and the round expiring 16.0 m
wide, against an astern control detonating 0.04 m out. The defensive asymmetry (D5) is now two-sided.

**The finding the merge exposed.** With the MiG now flying aggressive LOCKED pursuit, the F-16's own
UNCAPPED BFM roll law (defaults 90 °/s, `BfmSearchRollCap` 1.0) goes into a full-deflection roll-reversal
PIO at the close-in high-closure reversal and DEPARTS at t=18 (`duel-merge` exit 3→2, result=LOC). It is
CAUSAL — with the MiG's ACM disabled the F-16 does not depart and the run is exit 3 again. Per the
campaign rule a loss to an AI defect is not a result, so this is NOT "the MiG wins the merge"; the
R-73/GSh-301 thesis stays untested and the merge blocker moved a third time (departure → acquisition →
the F-16's roll law). The F-16 cap is F-16-scoped and would touch its byte-identity, so it is deferred.

**Gates.** All **57 F-16-only** stock missions byte-identical (before/after `telemetry*.csv` SHA-256);
`test-corner` unchanged (380 / 16.18 / 5.44); MiG non-combat missions byte-identical; the BVR duels
(`duel-offset`/`duel-emcon`) move only in `cmd_*` bookkeeping (the intercept CmDispense is no longer
rejected `NotImplemented`, flight state and outcome identical). Determinism 1/2/4 threads ×3 = one
fingerprint on `mig29-defend`, `duel-merge`, `mig29-bfm`. `verify-models`, `verify-layers`, WASM and
native all green; proof frame `notReadyDraws=0 violation=0`.

### Foundation (24–25 Jul)

| Commit | Section |
|---|---|
| `59f08c8` | module architecture runtime-polymorphic, nine system slots with NoOp defaults |
| `c9206eb`…`2099cb0` | renderer stage split in four slices — at the end zero inline shaders in `FBRenderer.cpp` |
| `4cb92e8` | HUD stopgap → generic default HUD in the displays slot |
| `2f3c277`, `8997eec`, `6f160af` | HUD font: coverage AA instead of alpha test, split into generic font system / MAX7456 hook, 16×16 glyphs from B612 Mono, the same AA technique for all strokes |
| `6802a6d`, `d31b1a9` | F-16 main HUD with the real combiner aperture, legibility for 720p |

### Pilot AI and the control loop (26 Jul)

| Commit | Section |
|---|---|
| `681c5f8` | pilot-AI framework: `FBPilot`, units, airframe controls |
| `65d334c` | mission runner + telemetry — **the control loop itself**, the prerequisite for everything that follows |
| `e49d335` | phase 1: takeoff flies |
| `e4d7c26` | telemetry/log architecture: declarative sources, central bus, `FBLog` |
| `705c90a` | lib/client split: core lib, `fb-gym`, elevation hook, baked Swiss DEM |
| `28e74e5` | `FBFlightMonitor` — incorruptible physics K.O., model-derived |
| `92fe8a4` | mission orchestrator down to four steps, declarative spawn, `FBMissionMonitor` |
| `8cd3a74` | phase 3: landing — `payerne-full` flies fully autonomously |
| `bf4ee62` | **hardening**: silent wrong values, aborts, client divergence — see "Defect classes found" |

### Multi-unit (26 Jul)

| Stage | Commit | What it built |
|---|---|---|
| 1 | `c1bc9de` | FDM instanceable — `FBFdm` as an object, no global instance |
| 2 | `c08a168` | the actor is ONE object (`units/FBSimUnit`) |
| 3 | `2c03704` | the formation is mission data — two jets fly |
| 4 | `6d7ed5a` | thread per unit in the gym, lockstep barrier, bit-identical |
| 5 | `9190e7c` | datalink — units see each other through a system |
| 6 | `4049a7b` | FCR radar with ACM modes, anonymous contacts, IFF |
| 7 | `b375bef` | BFM manoeuvre AI — flies on radar contacts alone |
| 8 | `071ea2b` | avionics data model: output blocks with validity + command bus |

### Knowledge base (26 Jul)

`2dd1142`, `e22f228`, `c4e96e7` — the official ED documentation distilled into `doc/modules/f16/`.
`weapons.md` and `defence-rwr-cm.md` from SHALLOW to FULL; `controls-commands.md` new, as the template
for the command blocks.

### Weapons, damage, tactics (27 Jul)

| Commit | Section |
|---|---|
| `b62c769` | weapons foundation: the weapon is a unit of its own with its own FDM |
| `5c68fc5` | AIM-120 with seeker, guidance and datalink support |
| `439f53a` | RWR and countermeasures — who notices being seen |
| `1ecd433` | intercept AI: BVR tactics — guide, shoot, support, defend |
| `6d84647` | damage model: hits become system failures, failures become invalidity |
| `82df2e2` | combat objectives and evolutionary tournaments |
| `a1a8fbf` | M61A1 cannon: derived ballistics, EEGS funnel, kinetic damage |
| `1eeff72` | air-to-ground: ground targets without an FDM, CCIP/CCRP from one integration |

### Refinement of the AI (27 Jul, ongoing)

| Commit | Section |
|---|---|
| `cac7b62` | pilot memory: the datum instead of the last measurement point; gun tracking with a rate term; roll-rate controller |
| `9673e00` | guidance holds a track where a track is declared — cross-track error and waypoint capture |
| *(this round)* | the close-combat law survives a raw airframe — the MiG-29 flies BFM without departing (`duels.md` D1) |

### The MiG's close-combat law (D1) — surviving a deckless airframe

**What it built.** The BFM control law is written for the F-16, whose JSBSim deck holds α and roll rate
under any stick. The MiG-29's deck has no FLCS, so the law departed it in 22.8 s from a merge. Four
measured, airframe-scoped screws close it, F-16 byte-identical: (1) the Manual-path pitch-deflection cap
`PitchStickMax` and (2) an α limiter allowed to push to recover (both in `systems/FBFlightControl`), plus
the pilot hooks (3) `BfmSearchRollCap` (0.20 — the search is a scan, not a combat roll) and (4)
`BfmRollRateMaxDegS` (60 vs the F-16's 90 — the twitchy K=201 roll overshoots the 10 Hz cap into a PIO).
Each was diagnosed by telemetry before it was turned, each exposed the next (α tumble → mush → search
limit cycle → pursuit PIO). **What it measured.** `mig29-bfm.fbm` (new): full BFM run, no KO, α ≤ 27°,
acquires and locks a trail defender (lock_s 203 / ctrl_s 5.3), deterministic over threads 1/2/4 × 3.
`duel-merge`: exit 2 → 3, the MiG survives (aoaMax 24.6°); the F-16 dominates the angles (lock_s 298 /
ctrl_s 78) but neither converts — a draw, the remaining blocker being gap 4h's ACQUISITION half (RAD
cannot hold the target in a turning merge), not the flying. 13 F-16 BFM/gun/BVR/attack missions
byte-identical; every other MiG mission's exit code unchanged (`mig29-full` touchdown 143.4 → 143.7 kt).
`doc/pilot.md` §5.10, `doc/duels.md` D1, `doc/modules/mig29/module.md` gap 4h.

### One model root and the delta rule (27 Jul)

**What it built.** All flown JSBSim models now live under `sim/assets/aircraft` — `f16` (incl.
`Systems/` and the two referenced engine XMLs, which moved into the model directory as `f16/engine/`:
JSBSim's own per-aircraft layout, which its loaders search first), `mk82`, and the `aim120` that was
already there. `FBModelRoots` has ONE root, `FBModule::FdmModelVendored()` and `FBStoreSpec::Vendored`
have been dropped without replacement, `FBFdm`'s engine/Systems probing (`stat` + parent truncation)
has become two unconditional paths, and the WASM build embeds one root instead of five individual
paths.

**Why.** Principle 1 has moved from "never patched" to the **delta rule**: the pinned submodule is the
base, the copy flies, and every deviation is a named, evidenced entry in `sim/assets/MODEL-DELTAS.md` —
a better mission result is explicitly not evidence. The gate is `make -C sim verify-models`
(`sim/tools/verify_models.py`): canonical unified diff per file, character by character against the
diff block of the entry. Deliberately no `patch`/`git apply` — an application with fuzz could swallow a
deviation.

**Measured.**

| Check | Result |
|---|---|
| Regression, 50 missions | **121/121 telemetry files byte-identical**, all 50 exit codes equal, `events.log` identical except for output path and wall clock |
| `verify-models` | green (4 upstream-covered paths, 0 deltas, 1 FlightBox-own model) |
| Negative test | one changed byte in `f16.xml` → rc=1 with the missing block; likewise a declared but absent delta, a non-matching diff, and an undeclared model directory |
| Harnesses | all seven rc=0; corner speed unchanged at 380 KCAS / 16.2214 °/s |
| Determinism | 5 missions × `--threads 1/2/4` × 2 repetitions = one signature each |
| WASM | builds; JSBSim loads `f16` from the embedded `/fb/aircraft` and trims (`trimConverged=1`) |
| Frame | `gpu_native --mission payerne-takeoff --interval 20` → 28 PNGs, terrain + HUD |

## Defect classes found

What the control loop brought to light that an inspection would not have found. The list is both a
warning and a test pattern.

| Class | Concrete case |
|---|---|
| **Silent wrong values** | `ApplySetup` returned 0.0 for unparsable text and reported success. An HTML error page from the `/elev` endpoint was cached as a sea-level elevation — a whole 216 s mission flew over sea level and reported SUCCESS. |
| **Missing divergence check** | 16 injected NaN cases all ran through with `tripped=0`. |
| **Unguarded calls** | unchecked JSBSim calls → `std::terminate`, exit 134. |
| **Missing header dependencies** | The Makefile had no `-MMD -MP`: stale objects, phantom measurements. Proven by a deliberate header change that altered a telemetry hash and was then reverted. |
| **Architecture leak** | `FBFlightMonitor` knew about runways. Physics K.O. and mission verdict were separated. |
| **Module specifics in generic code** | F-16 references in `FBFlightMonitor`; limits are now derived entirely from the model. |
| **Non-determinism through ordering** | Log line position depended on the scheduler. Solved via merge order instead of locks. |
| **Two copies of the same data** | `sim/web/missions/*.fbm` was a hand-kept copy in the old format — the WASM app stayed black. Now a build copy. |
| **Aliasing through tick rates** | The seeker looked at 20 Hz at poses published at 10 Hz: 446 m/s measured instead of 654 m/s. Solved via a dwell window instead of two single measurements. |
| **Zombie state** | A detonated missile kept radiating for 74 s after its detonation. `Retire()` now clears the signature. |
| **Wrong controlled variable** | A pure P controller against a ramp (the gun solution against a turning opponent) parks at ramp rate × time constant. A point controller against a track has a steady-state cross-track offset. Both are a matter of controller type, not tuning. |
| **Stale documentation in a data file** | Two mission headers still documented "ends in a timeout" after both runs had become kills. The header carries the reading rule and must be maintained with it. |

### Documentation: the spec-driven restructuring (27.07.)

**What it built.** `doc/` moved from "one file per subsystem plus a central TODO" to a
spec-driven shape: every topic file now carries `## Spec` / `## State` / `## Gaps` / `## Knowledge`,
grouped into `sim/`, `aircraft/`, `render/`, `clients/`. New: `vision.md` (the direction),
`roadmap.md` (R1–R10, thin, pointing at the Spec each stage must satisfy), `aircraft/mig29.md` and
`render/units-visual.md` (both spec-only, nothing built), `aircraft/stores.md`,
`clients/clients.md`. `PROGRESS.md` became this file; `TODO.md` dissolved into the Gaps sections of
the files it belonged to, plus `roadmap.md`/`vision.md`. `render/rendering.md` split into
`renderer.md` + `hud.md` + `clouds.md` (whose Spec is the owner-approved rebuild, including the
cirrus layer) + `units-visual.md`.

**Rule change.** The maintenance obligation is now spec-first: change the Spec, build until State
meets it, then update State/Gaps and add a line here (`conventions.md`). There is no second list of
open work any more.

**Not done at the time.** Existing bodies stayed German (each file said so); the translation wave plus
the schema alignment of `doc/modules/f16/` and `doc/modules/mig29/` was roadmap R10. `world-and-terrain.md` stayed at its
old path until the `/wx` round lands, then splits into `world/terrain.md` + `world/weather.md`.

### 2026-07-27 — /wx: worldwide weather on the tile server (`24ac1fc`)

New `/wx` endpoint on fb-tiles: NOAA GFS 0.25°, decoded by an own 330-line GRIB2 reader (wgrib2 is
not packaged in Debian trixie; ecCodes serves as the test oracle — max error 0.5 quantisation steps
over all 20 fields × 259,920 points), delivered as ONE packed 8.3 MB blob per run ("one run is one
atmosphere" — split blobs could straddle a cycle boundary). Byte-identical deterministic builds
across two compilers; the fixture in `tiles/testdata/` doubles as the gym dataset. Poisoned-cache
lesson applied: NOMADS failure writes nothing, ever.

### 2026-07-27 — weather in the simulation (`43b82b5`)

`core/FBWeatherProvider` (calm / constant-wind instrument / FBWX blob from file or memory),
`FBFdm::SetWindNedMs` → `FGWinds` (derivation in the header; only the owner writes, only on change),
`wx` mission declaration (mission always wins; defaults gym/native calm, **browser live**).
Measured: crosswind drift 3.3078° vs 3.2765° derived (0.95 %); uncorrected CCRP in 25 kt crosswind
shifts 12.8 m — far below wind×TOF (127 m) because a 227 kg bomb barely couples laterally in a 10 s
fall; GFS fixture wind recovered from the flown trajectory to 0.12 m/s. All 50 pre-existing missions
byte-identical. Found and open: guidance cannot close a steerpoint inside its drift-widened turning
circle at 18 m/s crosswind (permanent 59° orbit); the 10 m wind anchors at 10 m ASL, not AGL.

### 2026-07-27 — R10: English throughout, schema everywhere (this commit)

The four-part wave: (a) the seven big `sim/` bodies translated (~8,300 lines, zero content loss,
anchors fixed); (b) the rest of `doc/` plus legacy markers on the twelve old cloud
studies; (c) `doc/modules/f16/` on the Spec/State/Gaps/Knowledge schema — producing the first **coverage
map** FlightBox-vs-real-jet (near-full: command bus, HUD symbology, RWR/CMDS; nothing: startup,
displays, HOTAS, refueling; and the surfaced fact that the model flies an F100-PW-229 while the
doc describes the F110); (d) `doc/modules/mig29/` on the schema plus the citation reconciliation — where
the task premise ("uniformly PDF pages") proved wrong: the files were internally mixed, so all 131
DCS-FM citations were scored individually against the extracted PDF text (88 converted, 43 already
printed, re-grep proof 125 printed / 0 PDF). Provenance tags (`[MESS]`/`[ABL]`/`[MODELL]`) stay
German deliberately — they appear identically in code and three doc trees; renaming is only sane as
a coordinated sweep. Remaining German: `world-and-terrain.md` (splits into `world/` in phase 3 of
the mirror refactor) and the pre-refactor `sim/src` paths inside the seven translated files (also
phase 3).

### 2026-07-28 — the first model delta, and the landing that follows from it (this round)

**D1 — the flaperon mixer** (`sim/assets/MODEL-DELTAS.md`, the delta rule's first live entry, and its
first practical test: the emitted block collided with the verifier's own HTML-comment stripping, so
`tools/verify_models.py` now protects the inside of a ```diff fence — otherwise a delta that touches an
XML comment would be undeclarable). `f16.xml`'s flaperon summer carried the flap command
DIFFERENTIALLY and the roll command SYMMETRICALLY, so `fcs/tef-control` cancelled out of
`fcs/flaperon-mix-rad` and twice the aileron command took its place. The correct mixing is derived from
the model's own consumer structure — the mixer's only two consumers, `CLDflaps` and `CDDflaps`, are
symmetric per-radian force coefficients, while the rolling moment travels through `fcs/aileron-pos-rad`
— and the evidence is a physical impossibility the model produced: **+6,420 lbf of forward "drag"** on
a right roll at 350 KCAS. Measured before → after: `flaperon-mix-rad` under a pure roll step
−1.28 → **0.0000**; Nz peak in the roll-in −1.54 g (right) / +3.46 g (left) → **+0.97 / +0.97**; flaps
fully out 0.0002 → **0.349 rad** = the 20° the Flaps channel commands, ΔCL **0.122**, ΔCD **0.028**;
roll rate at 400 KCAS +187.8/−132.3 → **+156.4/−156.6 °/s**, direction asymmetry across 250–600 KCAS
from **55.5 → ≤ 0.2 °/s**.

**Hook cascade, each one re-measured rather than assumed:** corner SPEED unchanged at 380 KCAS, the g
at it 5.6 → **5.4** (`BfmCornerG`), best rate 16.22 → 16.37 °/s (peak moves to 400); 11°-AoA trim speed
165 → **154 KCAS** (`ApproachSpeedKt`). Unmoved and reported as such: `BfmBrakeMs2` (2.531 → 2.527 m/s²
— the flaps only deploy below 250 KCAS, that hook is measured at 325–400) and the ~0.2° cruise
asymmetry (median |φ| on settled route legs 0.186° → 0.185° over 60,900 samples — it is the roll PID's
steady-state residue, not the mixer's; the hypothesis that D1 would fix it is **falsified**).

**The long landing roll.** The deceleration budget named the cause and it was not the model's µ:
JSBSim brakes on `static_friction` (0.8, upper end of dry-runway values), and the measured brake
deceleration is 3.3–3.8 m/s², working correctly. The loss sat between the two-point attitude and the
brake gate. In the aerobrake the wings carry the whole aircraft (wheel normal load **0 lbf** at 12°),
so no brake can bite and the 5,295 lbf of aero drag is the entire budget; the moment the nose falls,
drag collapses to 1,477 lbf. The pilot gated the brakes on `AerobrakeSpeedKt` (100 kt) while the
elevator actually loses the attitude at ~106 KCAS — a **361 m / 6.7 s coast at 0.45 m/s²** in between.
The gate now hangs on the fact instead of the speed: `FBAirframeControls::GetNoseWheelOnGround()`
(the forwardmost bogey's WOW, selected by geometry, `FBFdm::GetNoseGearOnGround`), latched for the
roll-out, exactly as `procedures-landing.md` sequences it. Landing roll at Payerne RWY23:
**1,597 → 785 m** (`payerne-landing`, −51 %) and **1,341 → 928 m** (`payerne-full`, −31 %). Attributed:
D1 plus the new approach speed does 1,597 → 1,039 m and 1,341 → 909 m (the flaps finally give the
two-point attitude real drag), the gate does 1,039 → 785 m on `payerne-landing` and is NEUTRAL on
`payerne-full` (909 → 928 m) — there the nose happens to fall at 99.6 KCAS, so old gate and new gate
fire at the same instant. That neutrality is the point: the gate does not brake EARLIER, it brakes when
the aerobrake is over, whenever that is.

**The approach speed is the honest one, and it costs distance.** With the pre-D1 165 kt the same build
rolls 642 m / 578 m and greases the touchdown (126.7 kt, 0.29 m/s sink) — but it flies final at 9.2° AoA
and floats 38 kt before touching. At the measured 154 kt it flies final at 11.0° AoA and touches at
12.8° AoA, both exactly as `procedures-landing.md` prescribes, at 142.9 kt and 2.96 m/s of sink (peak
gear load 2.05 W against the monitor's 3.0 knockout). The extra 143 m is the price of a procedurally
correct approach instead of a float. What this exposed and did NOT fix: the flare law targets a pitch
ATTITUDE 1.7° above the approach attitude and therefore barely arrests the sink — it had been masked by
11 kt of excess approach speed for as long as the flaps did not work.

**Re-baseline:** 53 missions, 48 verdicts unchanged, five changed and all five explained rather than
papered over — `attack-ccip`/`attack-ccrp`/`wx-ccrp-wind` (the release vertical velocity flips sign
because a roll-in no longer produces a lift step, and the FCC's own table-vs-aero prediction error of
53–64 m stopped cancelling the aim error instead of adding to it: aim error 28 → 80 m), `gun-bfm` and
`bvr-duel-decided` (the BFM/launch geometry rides on the roll behaviour that changed). No mission file
was edited to make any of them green. Determinism 1/2/4 threads identical on five multi-unit missions;
eight harnesses, `verify-models` (green WITH exactly one declared delta, and its negative directions
re-checked), `verify-layers`, WASM + smoke (the corrected gain is in `gpu.wasm`, the old one is not)
all pass.

### 2026-07-28 — the re-tune against the corrected physics (this round)

D1 left five missions on TIMEOUT with a suspended reading rule. All five are back — and none of them by
a number chosen to make them green: each of the three faults it exposed was a real defect that the old,
broken roll authority had been paying for.

**`gun-bfm` — the closure schedule was capped on the wrong measurement.** Attribution first, by running
the CURRENT code against the PRE-D1 model in a scratch tree: over a 16-approach sweep (8 geometries ×
straight/turning defender) the pre-D1 model scores **4/8 straight + 8/8 turning**, post-D1 **0/8 + 8/8**
— the whole regression sits against the STRAIGHT defender, and it is one event: the first stern
conversion now tips the other way and becomes a fly-through that costs the ACM box its contact. Under it
sat the real fault. `BfmBrakeMs2` bounds the closure schedule's cap `a/k`, but it had been measured as
the airframe's LEVEL-FLIGHT deceleration (2.4 m/s², 238 samples) — a different quantity, because a
closure carries the pursuit geometry as well as the drag. Measured on the thing itself (one-second
windows in the conversion, idle + full speedbrake + valid track, N=4,595): **median 1.86, p20 1.16, p90
5.76 m/s²**. A braking LIMIT takes the pessimistic end of its own distribution, so the hook is 1.2 and
the cap 140 → 70 kt. `gun-bfm`: the pursuer used to arrive at 0.5 nm with 105–120 kt against a schedule
asking for 27 and fly through at 0.11 nm; it now tracks at t=59.5 and KILLS at t=66.7 on 70 rounds.
Sweep after: **3/8 + 8/8 = 11/16** against 12/16 pre-D1 and 8/16 post-D1, with mean tracking error
41.1° → 25.5° (straight) and 7.2° → 4.6° (turning). The last kill does not come back and it is named as
such, not papered over.

**`bvr-duel-decided` — the round, not the shot.** The launch geometry is unchanged (24.8 km, the same
beaming defender to within 2° of heading and 30 m of altitude); shooting closer was measured and does
nothing (`pilot_shot_rtr` 1.0 → 0.5 gives 6.25 / 5.35 / 8.11 / 7.52 / 7.20 / 4.03 m — noise, no trend).
What D1 exposed is an instability in the AIM-120's terminal acceleration loop: past ~10 g of demand the
fins ran onto their stops, the integrator wound into a reversal, and the round's own alpha rang (mean
tick-to-tick |Δα| in the terminal phase 0.70°). Two structural fixes, no damage-model change:
**conditional integration** (a fin on its stop cannot answer more integral) and **`kLoopI` 2.0 → 1.5**,
the largest gain on the stable side of the measured boundary (|Δα| 0.698 at 1.75 → 0.139 at 1.50 — an
edge, not a trend). Miss 6.25/7.09 → **2.36 m, one shot, exit 0**. Everything else the round flies got
better with it: `intercept-lostlock` 4.12 → 0.755 m, `damage-amraam` 1.90 → 1.49 m, `cm-beam-only` from
no detonation at all to a 7.83 m hit — which is what its own 2×2 table claims (beam alone leaves the
seeker nothing to be confused by; only chaff AND beam still defeat the shot, and that leg still does).
Two verdicts follow: `cm-beam-only` 0 → 1 and `intercept-lostlock` 0 → 1, both explained in their heads.

**The three attacks — two errors that used to cancel, now separated.** D1's report said the fire
control's own table-vs-aero error (53–64 m) had stopped cancelling the aim error. Measured, the aim error
had a cause and it was not the computer: the pilot set `AtkReleased_` when the pickle was POSTED, so the
escape turn began during the actuation latency and the store left the rail at **32° of bank** and
−0.6 m/s. He now flies the run-in until his own SMS counter says the store has LEFT (roll −0.16°,
vertical +0.01 m/s at separation), and he leads the cue by his own DECISION TICK as well as the bus
latency — between reading a number and pressing lies one slot, worth 21 m at 211 m/s. Result, per
`stores DELIVERY`: `predErrM` 63.8 → **43.6 m** (inside the ~45 m the target requires, and NOT corrected
— it is the declared property), `aimLongM` 78.8 → **40.9 m**, i.e. the release-moment error is now ~0 and
what remains IS the computer's error. `attack-ccip`/`attack-ccrp`/`wx-ccrp-wind` exit 0.

**Rejected, with their measurements** (now in `pilot-ai.md`'s Gaps): integral action on the BFM throttle
— the textbook fix for a P-only loop, and it turns the straight-defender sweep 0/8 → 8/8 while turning
the other one 8/8 → 0/8, because exact speed matching leaves the pursuer at the defender's own 248 KCAS
and his rounds miss by 7–8 m instead of 1.6–4 (the miss is ½·V·ω·TOF², so it is the shooter's speed);
and a turn-rate speed floor meant to replace that accidental energy bias, which does not bind (the
"max-rate" defender actually turns at 5.4 °/s) and binds everywhere the moment the aim error is added.

**Regression, all 53:** 7 verdicts changed — the five targets plus the two missile neighbours above; the
other 46 keep theirs. 27 missions differ byte-wise, in exactly three families: the BFM-phase ones (the
closure cap), the attack/store ones (the release timing) and every AIM-120 one (the terminal loop).
Determinism: 9 runs each (1/2/4 threads × 3) on the five targets → one fingerprint each. Eight harnesses,
`verify-models` (green, still exactly one declared delta), `verify-layers`, `nm` (0 GPU symbols in
`fb-gym`), native + WASM green, and the WASM A/B is decisive: `gpu.wasm` built from this tree carries one
more `1.2` double than one built with the old hook, and the two binaries differ. Proof frame:
`gpu_native --mission attack-ccip.fbm --interval 20` → SUCCESS, bunker DESTROYED, eight PNGs.

**Left stale on purpose:** `doc/weapons.md` §10.2's gain table still prints
`kLoopI = 2.0` and does not mention conditional integration — that file was outside this round's write
permission.

### 2026-07-28 — R5 clouds merged (`9ca2c0e`), MiG-29 stage 1 merged (`b411b2b`)

**Clouds:** the rebuild per the approved Spec — ONE bounded-volumetric stage, one separable density
function evaluated in C++ AND WGSL (constants printed from the C++ side, max |Δ| 1.87e-5 over 12,288
samples), the six FBCloud* stages and the tonemap's second pipeline deleted. Cost measured worst-case:
8.8 ms full-res vs ~23 ms of the old quarter-res+temporal chain — 2.6× cheaper at 4× the marched
pixels. Weather-driven via FBWorld::Weather() (no weather ⇒ no cloud pass, 6/7 passes). Five proof
sets incl. seamless fly-through and cirrus fibres along the real 250 hPa wind (3.8° residual).
Merged on its own branch against `ab40bac` by a dedicated agent (deletions win over namespace edits;
`--wx` is the screenshot venue's weather, mission venue reads the .fbm — combining both is now an
argv error). Known gaps: stored proof PNGs are stale vs the committed source (predecessor tuning
drift — re-capture wanted), march grain 0.04–0.08 by design, one ceiling clamped into three decks.

**MiG-29 stage 1:** the model exists — `sim/assets/aircraft/mig29/` (FlightBox-own, GPL-2.0-or-later,
every table tagged INV/GEO/ANALOGY/SET) plus `make test-mig29` measuring 23 anchors. 10 hit or in
band (Vmax SL +0.2 %, rotation/liftoff/ROC in band), 4 missed with diagnosis instead of anchor-fitting:
Ps SL −24.8 % is the borrowed thrust analogy (needs aug factor 1.16, F100 surface gives 1.02 — NOT
drag-closable without destroying Vmax SL), ceiling +8.7 % same family, takeoff run +29.8 % is the
spec's own §12.3 doubt. Roll rate 241 °/s declared a model property — no anchor exists at any tier.
Two JSBSim findings for the house: FGTrim drives `pitch-trim-cmd-norm` (a pitch channel without that
summer cannot trim at all), and linear table interpolation overstates a quadratic drag rise ~4.5× at
the first breakpoint. F-16 untouched (corner numbers byte-identical).

### 2026-07-28 — Phase 3 of the mirror rebuild: `doc/` becomes `sim/src/`

The documentation is now a **1:1 mirror of the source tree**. `doc/flightbox/` is gone; the seven meta
files sit at the root beside `core.md` / `fdm.md` / `systems.md` / `sensors.md` / `weapons.md` /
`pilot.md`, and the four subdirectories `missions/`, `modules/`, `render/`, `world/`, `clients/` carry
the same names as their source directories. Every move was a `git mv`, so the history follows.

**The two splits, both with translation** (the last German prose in `doc/`):

- `mission-format.md` → `missions/` — nine files (`INDEX`, `syntax`, `verdict`, `sensors`, `avionics`,
  `weapons`, `combat`, `weather`, `output`) plus `units-and-missions.md` → `missions/runtime.md`. Each
  new file carries the Spec/State/Gaps/Knowledge frame; the leading rules (exit codes, "a mission
  file's header comment is a binding reading rule") live in `missions/INDEX.md`.
- `world-and-terrain.md` → `world/terrain.md` (§1–§8) + `world/weather.md` (§9). The two points the
  roadmap had parked for exactly this split — DEM cache per worker instance, imagery mode not
  declarable in `.fbm` / TLS not wired — got their home in `world/terrain.md`'s Gaps. **The Parked
  table is now empty.**

The two reference bases moved under their module (`doc/f16/` → `modules/f16/`, `doc/mig29/` →
`modules/mig29/`), each now sitting beside the `module.md` that implements it; the cloud studies became
`render/clouds-legacy/`. **One skill instead of three:** `f16-systems` and `mig29-systems` are deleted,
their routing tables absorbed into `.claude/skills/flightbox/SKILL.md` as "The module reference bases".

**Path sweep:** 209 relative links inside `doc/` re-resolved against their new locations, plus 475
plain-text mentions across `doc/`, `CLAUDE.md`, the `.fbm` headers, `sim/tools/`, `sim/assets/`,
`tiles/` and ~150 comment banners in `sim/src/**`. Comment banners only — the three CLI usage strings
that name the format were deliberately left, because touching a string literal would move the
`strip_comments` hash. That hash is unchanged (`8d85837e…`, 233 files), the link check over every `.md`
is clean, `core-lib`/`gym` build, and three mission samples run byte-identically.

### 2026-07-28 — two value gaps: the wind orbit and the roll-limiter fixed point (this round)

**A — a steerpoint the guidance cannot close** (`doc/systems.md` §7.5.1). A capture circle is a GROUND
test of fixed radius; the circle the aircraft can fly lives in the AIR MASS, and a fix WITHOUT a leg is
flown by the bearing law, which controls the nose and not the ground track. New instrument
`missions/wx-orbit.fbm` (the GFS fixture's 9,000 m wind as the closed form `wx wind 338 39`): closest
approach **614 m** against a 500 m circle, then a permanent limit cycle — 1,793…4,851 m, −59.1° bank,
99.2 s per lap; the same file in calm captures the same fix with **4 m** to spare. Answer: a THIRD
sequencing ground, `orbited` — two failed approaches (closest approach, opened by more than the capture
radius, closed again, opened again) — bound to the SUCCESSOR as `passed` is bound to the predecessor, so
the deliberate terminal orbits of `bfm-basic`/`gun-turning`/`bvr-duel` are out of scope by construction
(re-measured: bandit `activeWp` 0 for the whole run, zero `WP_REACHED`). Threshold 2 is measured, not
chosen: at 1 the attack missions sequence their target fix out of the egress at t=87.9 s. Both
authorities state it independently, both fired at t=311.6 s; `wx-orbit` SUCCESS at t=485.4 s. All 53
pre-existing missions byte-identical.

**B — the roll limiter had no fixed point** (`doc/pilot.md` §5.7). `cmd_prev·cap/rate` is not a limiter:
linearised against the identified plant it is `z² − 2az + a = 0`, i.e. an oscillator with **|z| = √a**,
and it held **1.52 ×** its own declared cap over the 16-approach sweep (pooled autocorrelation of the
rate while active: first recurrence 0.70 s). Replaced by a memoryless ONE-STEP PLANT INVERSION off an
ARX(1) identification (15,325 samples below the cap, open loop: a = 0.734 / τ = 0.323 s, K = 78.7
°/s per stick) — 1.23 × at the same cap, and stretches ≥ 4 s above 0.8 × cap 11 → 0. The cap itself
became a closed form: the largest error this law can command is 180°, flown in the time constant the
roll serves → **90 °/s**, with `kBfmReverseS` falling out identically `kBfmTurnTimeS`. Re-measured over
six cap values × 16 approaches, 90 is also the measured optimum (12/16 against 8/16, and the only value
with no departure in the eight committed BFM missions); the control with the limiter removed scores
7/16 at 132 °/s peak. New instrument `missions/bfm-pointblank.fbm` (0.8 nm head-on, the swinging
stimulus): 1.37 × → **0.89 ×** the cap, 9.2 s → **0.0 s** above it. Costs, all declared in their
headers: `gun-dry` 3 → 1 (all twelve rounds now arrive), `gun-bfm` kill 66.7 → 84.2 s, `bfm-blind`'s
blind interval 41 → 199 s (chaotic across every cap tested), one departure in a non-committed sweep
geometry. Exactly five missions move, all BFM; nothing else in the tree changes by a byte.

### 2026-07-28 — MiG-29 stage 2a+3: the module flies end-to-end (merge of `b3da424`)

`sim/src/modules/mig29/` (module, pilot numbers, damage zones, registry name `mig29`) plus four
missions; `mig29-full` flies takeoff, route and landing autonomously to a stop on the Payerne
threshold (exit 0, 730.6 s; rotation 130.1 kt, touchdown 143.4 kt at 11.66° AoA and 3.59 m/s).
`mig29-pair` proves two DIFFERENT modules in one formation. The FBW preset is its own for a
structural reason: behind the g output the F-16 has an FLCS, here the output IS the deflection.
Three measured failures stand in the preset comment and determine it (saturating yaw → LOC t=28 s;
double-integrator limit cycle, 20 s period; no α limiter → α 90°, LOC t=122 s). The SOS limiter is
thereby built where `flight-model-spec.md` §7.3 placed it, behind one preset number. `test-mig29`
gained the two measurements the module cites: 136.8 kt at the documented 11° touchdown α, and corner
420 kt / 24.18 °/s / 7.83 g. F-16 byte-identical across all 53 stock missions.


### 2026-07-28 — MiG-29 stage 4: the asymmetric duel as a measurement campaign (this round)

**What the round was for.** Everything since stage 1 existed so that two DIFFERENT aircraft could meet.
[`pilot.md`](pilot.md) gap 2.3 had recorded for three rounds that the symmetric F-16 duel is a
stalemate by construction, and `modules/mig29/module.md` had said in as many words that the MiG exists
to turn the coin toss into a choice. This round is the measurement that says whether it did.

**What it built.** Eight missions (`sim/missions/duel-*.fbm`), an analysis tool
(`sim/tools/fb_duel_report.py`), a `module=` key on the tournament so a variant file can pit an F-16
doctrine against a MiG doctrine, and a new topic file [`duels.md`](duels.md) — which is a family of
MISSIONS rather than a directory of source, and the first entry in `INDEX.md` that is not a mirror of
`sim/src/`.

**The answer, and it was not the expected one.** Neither side structurally dominates; the launch
DOCTRINE does. With both pilots on the shipped rule (shoot at Rtr) five of five BVR geometries draw —
head-on, 50° offset, 6,000 m to either side, EMCON — because the two Rtrs sit within half a mile of
each other (AIM-120 9.78 nm, R-27R 10.25) and every round then arrives outside its warhead's lethal
radius. Change the rule on one side and the same geometry decides, and what each side needs is
different: **the MiG needs only the early launch** (`duel-doctrine-mig`, exit 0 — R-27R away at
14.41 nm, 25.8 s of unbroken illumination, 9.35 m detonation, the F-16 defensive 1.5 s before its own
trigger and never firing), **the F-16 needs the early launch AND 6,000 m** (`duel-doctrine-f16`,
exit 0 — its early launch alone is 10.7 s ahead and still draws at 4.79 m; from 6,000 m higher the
identical decision arrives 1.77 m out and kills).

**Three AI defects, all found by measuring, all fixed.** The GCI entry chain advanced on the POST
rather than on the acknowledgement, so the one entry that makes the N019 exist could be lost to a
single g-loaded tick (measured: 400 s of a duel flown blind). The intercept antenna was centred on a
COASTED look while the jet's own attitude moved, freezing a ±6° bar after one look through a 6,000 m
descent. And `FBMig29Pilot::InterceptSpeedKt` was a unit error — a CAS derivation fed to a TAS command
— that had the MiG cruising to every BVR merge at 217 KCAS / M 0.54, 40 % below its own departure
speed. **That last one is the round's second finding:** with it in place the F-16 won four of the five
BVR geometries outright. Correcting it turned all four into draws, i.e. most of the F-16's apparent
BVR dominance was a MiG tuning error rather than a weapon-system difference.

**Measured.** 66 of 69 stock missions byte-identical, all 69 exit codes unchanged; the three that
moved are `bvr-duel` and `bvr-duel-decided` (one to two extra antenna slews — `cmd_*` counters, plus
2.9 s in which one jet's RWR carries an extra SEARCH-class contact behind it that nothing acts on) and
`mig29-intercept` (same exit code and verdict, everything earlier and tighter: kill 87.7 → 78.1 s,
miss 1.13 → 0.34 m). No flight-state column and no verdict moved anywhere. All eight duels one fingerprint over
`--threads 1/2/4` × 3. The mixed tournament decides 12 of 30 runs where the symmetric one decides
**0 of 30**, and the early launch is worth an entire outcome band on the MiG (−393.7 → +585.0) against
nothing on the F-16 (601.8 → 603.3) — the same asymmetry the named missions found, reproduced by a
fitness written before the campaign existed. Open, and now with numbers: the MiG's close-combat law
DEPARTS the airframe in 22.8 s from a nose-on merge (`duel-merge`, kept as a reproducer), and an
AIM-120's terminal miss runs 1.37 → 7.66 m as closure runs 744 → 1053 m/s, which against a 1/r² damage
model is the difference between a kill and a jet that flies on.


### 2026-07-28 — MiG-29 stage 2c: the weapons and the signature

**What the round was for.** The MiG-29 had sensors and no weapons; the F-16 had no infrared round at
all; flares had been dispensed and counted since the countermeasure round with nothing to work on; and
`RADAR_DESIGNATE` was unreachable because the intercept pilot correctly disengages from a target it
cannot shoot. All four are the same missing piece, and it is the SEEKER.

**The one architectural idea.** A guided round is still ONE module and N catalogue entries; what makes
an AIM-120, an AIM-9 and an R-27R three different weapons is `FBSeekerKind`, and each kind names a
derivation of a SENSOR SLOT THAT ALREADY EXISTS. The infrared seeker is an `sensors/FBIrstSystem`, so
it inherits the aspect law, the afterburner term, the cloud deck and the anonymity, and the perception
boundary does not grow by a file (`verify_layers`'s `RESTRICTED` list is unchanged — the scan lives in
the base). The semi-active seeker is an `sensors/FBRadarSystem` that never transmits. Two seeker kinds,
no new architecture, and the tactical differences fall out of the sensors' own limits.

**The measurements that decided things.**

- **Flares now work, deterministically.** One inequality between two received irradiances in one unit
  (a clean airframe seen dead astern = 1.0), so the ASPECT does the whole job: head-on and dry an
  aircraft radiates 0.16 and a cartridge beats it six times over; astern in afterburner it radiates
  2.25 and cannot be deceived. Both branches measured on BOTH airframes at exactly `tgtIntensity=0.16`
  — the same number from the same code — and the decoyed rounds miss by 22.8 m (R-73, 3.5 m fuze) and
  25.96 m (AIM-9, 6.0 m fuze).
- **The semi-active penalty, as a number.** 28.56 s of unbroken illumination for one R-27R shot against
  the AIM-120's 5-15 s; break the lock in flight and the round misses by 27.04 m where an AIM-120 with
  the same loss still hits by 0.755 m.
- **The RCS calibration is the identity for the F-16.** `σ^¼` scaling with the F-16's own 1.2 m² as the
  reference, so all 55 F-16 missions came out byte-identical on every column and every event, and the
  asymmetry (1.351× / 0.740×) exists only across types.
- **30 mm is a different weapon in the same currency.** A kill on 67 of 150 rounds at 294 m of round
  path; the FULL drum at 571 m wipes the target's avionics without downing it. The documented
  200-790 m effective band emerging from the dispersion model rather than from a range limit.

**Three defects the measurements found**, each fixed where it belonged rather than where it showed:
the MiG's gun never learned its own unit id and therefore shot ITSELF down at the muzzle (the runner's
shooter exclusion compares `LauncherId`); `FBFlightControl` returned before its alpha limiter in
`Manual`, so every hand-stick phase on an airframe whose deck has no limiter was unbounded — invisible
until BFM became the first phase that really pulls; and the BFM roll-rate cap inverts a PLANT, so with
another aircraft's constants it is an oscillator rather than a cap (identified for this airframe:
a = 0.819, K = 201 °/s against the F-16's 0.734 / 78.7).

**One long-standing gap closed by measuring instead of arguing.** The MiG's corner formula read −16 %
against the harness. Neither hypothesis survived: the altitude loss inside the window is worth +1.7 %
and the convexity of `√(n²−1)` +0.4 %. The harness was measuring the rate of the body's EULER HEADING
while the formula predicts the turn rate of the VELOCITY VECTOR, and at 22.7° of incidence in an
85°-banked pull those differ by 18 %. Measured directly, the formula is right to **1.4 %** — better
than the F-16's own −2 %. The correction went to the harness's reporting, not to the formula.

**What is honestly not finished.** The MiG-29 has no dispensers at all (no source states the
BVP-30-26's programme parameters), so the flare asymmetry currently runs entirely one way. And its BFM
is unfinished: the N019's close-combat modes are pencils in azimuth and its wide mode does not
auto-lock, so a manoeuvring MiG cannot acquire — 0 contact ticks in 134 s, measured.
`FBPilot::BfmDesignate` gives the pilot the thumb he needs (a no-op on an auto-locking set), but the
cold-search law still rolls the jet before the first two looks land. `mig29-gun` therefore measures the
WEAPON from a stable position with a briefed burst, and says so in its header.

### 2026-07-28 — MiG-29 stage 2b: the sensors and the guidance

Three real sensor derivations, **one new generic slot**, and GCI as mission data.

**`sensors/FBIrstSystem` is the fourth sensor slot and the fifth file allowed to read
`units/FBUnitRegistry`.** The boundary was never a COUNT — it is "only simulated sensors, each paying a
stated price" — and the widening is recorded where it is enforced: `tools/verify_layers.py`'s
`RESTRICTED` table FAILED on the new include until the file was added to it by name. An IRST pays in
range (25 km at best against the radar's 50), in identity (no interrogator, and `core/FBIrstContact` has
no field one could be put in) and in weather, and gives back the one thing no other sensor here does:
it costs the observer nothing to look.

**Two generic constants became hooks, both defaulting to the previous behaviour exactly.**
`DopplerNotchMs(rangeM)` + `NotchRejectsDetection()` (until now the notch was ONLY chaff's channel — a
target in the filter stayed visible; a set whose source QUANTIFIES the threshold now rejects) and
`CoastS(volume)` (the N019's source names a duration, not a frame count). The RWR grew four:
`Blanked`, `ReportBearingDeg`, `ClassifyMode`, `PriorityRank`. `FBUnitSignature` gained
`Afterburner`, read off JSBSim's own `FGTurbine::GetAugmentation` rather than off a throttle position.

**Measured against the documented numbers** (four new rigs, all TIMEOUT by construction): detection
latency **6.0 s** (the derivation runs the other way — the documented "up to six seconds" over the
generic two-look firming IS the 3.0 s frame time); the Doppler envelope rejecting at **7.94 m/s vs
41.67** beyond 8 nm and **4.34 vs 16.668** inside 5.4 nm; **`coastS=6`** inertial tracking; the SPO-15's
forward hemisphere going dark in the SAME tick ILLUM is acknowledged, with the emitter's `fcr_on`
unchanged, and its bearings reported as channel centres (−10.0° where the F-16 reports 0.045°); the IRST
aspect law separating a tail-on detection at **19 562 m** from a 103°-aspect one at **15 222 m**; the
6 km laser stepping `irst_lock_nm` from **−1** to 3.199 nm; a target above a GFS deck never detected
(`irst_masked`, the first tactical weather effect on a sensor here); and the GCI chain taking **8.0 s**
from the controller's call to a radiating radar, with the opposing RWR lighting up 0.1 s later.

**Two defects found by building the rigs, both fixed and both measured.** (1) A set powered up mid-run
replayed its whole silent period through the catch-up guard and reported a firm track in the tick the
switch moved (t=27.9 instead of one frame later) — `ResyncScan()`, opt-in, so the F-16 is untouched.
(2) Timing the SPO-15's documented 125-250 ms illumination event classified EVERY search emitter as
tracking, because the emission model publishes a searching beam as continuous (`mig29-pair`, t=0.3:
the F-16's CRM sweep reported as TRACK). The event half of that rule now waits for a pulsed emission
model; the channel half — the actual device defect — is what the override contributes.

`set task intercept` is unlocked for this module, and the honest outcome is that the intercept
DISENGAGES on first contact: `pilot/FBPilot`'s own rule is "a target on the scope and nothing on the
rails → Abort", and this jet has no weapon yet. F-16 byte-identical across all **56** stock missions on
every column they ever had; the four MiG missions move exactly once, because the N019's power-up
emission position is OFF and this aircraft now starts silent by doctrine.

### 2026-07-28 — C2: the mission clock, and the ephemeris moved down a layer (this round)

One mission-wide `time 1999-03-24T22:00:00Z` line, Zulu only, 1901…2099, converted by a
days-from-civil calendar in `core/FBCivilTime.h` rather than by `timegm` (which would read the host's
zone and make the same file mean a different sky in a container); absent means **no clock at all**, which
is why all **84** pre-round missions stay byte-identical — 259/259 telemetry files bit-for-bit, 84/84
`events.log` identical modulo `wallS`/`speedup`/path, at `--threads` 1, 2 and 4 — and a client `--utc`
that contradicts a declared `time` is a **boot error** (`missions/FBClockBoot.h`), not a precedence.
The price of the round was structural, not the parser: `render/FBEphemeris.h` became
`core/FBEphemeris.h` (`FBSunPos`/`FBMoonPos`, `double` seconds) so `sensors/` can reach the sun for
`C3`, proven pixel-exact by identical `--utc 922312800` PNGs in SVS and EVS; and `fb-gym`, which had
no ephemeris at all, now writes `FBEnvironmentBlock` through `FBSimUnit::UpdateSolar` →
`FBModule::SetSolar`. `missions/clock-night-payerne.fbm` proves arrival twice — `mission CLOCK
utc=1999-03-24T22:00:00Z sunElDeg=-37.0489` in the log, `blk_env`=1 for all 2 167 rows — and proves
the clock is a stamp and not an input: against `payerne-airstart.fbm`, same spawn and route, the two
telemetry files differ in **exactly that one column**.

### 2026-07-28 — C3: the eye — the sixth registry reader, and what measuring it corrected (this round)

`sensors/FBVisualSystem` is built to the contract of `sensors.md` §9 and is the SIXTH file allowed to
read `FBUnitRegistry`, declared in the gate before a line was written. One inequality — presented extent
over range against a contrast-scaled 12-arcmin threshold — fed by a chain of laws that were all already
in the tree: the presented dimensions come from `FBDamageLayout` (which gained the plan extent, so the
gun and the eye read ONE table), the haze from Koschmieder + the ISA scale height, the daylight from
`FBDaylightFactor` moved out of the renderer into `core/FBEphemeris.h` (its fourth consumer, not a
second dusk), the glare from Stiles–Holladay collapsed into one readable half-angle, and the cloud from
a MARCH through `core/FBCloudDensity` — the price §6.5 declined for the IRST. **Measured, not asserted:**
head-on 2 493 m against side-on 3 784 m (the ratio IS the two presented dimensions); zero contacts at
night against nine by day on byte-identical geometry; 1 206 m against 2 373 m looking into a 6.6° sun
(the reach ratio 1.97 equals the contrast ratio 1.94 — exactly inverse-linear, as specified); a crossing
line of sight through a deck at optical depth 22.5, never seen, with `vis_masked` marking exactly the
window in which the same air without the deck would have shown it. The anonymity claim `w5-03`/`o2-08`
hang on is now a measurement: two runs differing only in the target's `team` token produce
**byte-identical telemetry and identical `vis` lines**, both naming `mig29`.

**Three places the contract was wrong, corrected in `sensors.md` rather than quietly satisfied.** §9.4's
worked ranges used dimensions the layout does not declare and left out its own haze term. §9.6c bought
the march on "a 40 % deck is mostly hole" — measured over the whole committed fixture the opposite
holds: at 40–50 % cover every ray is optically closed while a LID calls it clear, so the march differs
from a lid only in the 20–50 % band and there it is STRICTER. And §9.9's promise that `events.log` stays
byte-identical "because no mission declares a visual scenario" is false: 38 of 93 missions have aircraft
inside a few km of each other. The requirement BEHIND it held exactly — 285/285 telemetry files carry
every pre-existing column identical position-for-position with nine appended, 93/93 `events.log` have no
pre-existing line changed, and no trajectory moved, because nothing consumes the block yet (deliberate,
the D3 precedent). Determinism `--threads 1/2/4` byte-identical; the march costs −1.1 % on an
8-aircraft mission, i.e. below the run-to-run noise.

### 2026-07-28 — C12: the target vocabulary — four objective kinds, and a cover rule with an exit code

`identify unit X range <m> hold <s>`, `protect unit|team`, `no_fire` and `deny release unit|team` are
built. The whole price is what the spec said it would be — one monotone bit (`ReleasedWeapon`) and one
float (`RangeM`) on `FBUnitObservation`, both filled by the OWNER from registers it holds itself: the
bit at the one place the runner drains `Stores().TakeRelease()`/`Guns().TakeBurst()`, the range from the
published poses the CPA already runs on, and only when some unit declares an `identify`. One correction
against the estimate: `no_fire` asks about the DECLARING unit, and the monitor has no identity with
which to find itself in the roster, so the same bit also rides on `FBMissionMonitorSample` beside
`CombatIneffective` — a field more than counted, in the struct that already carries the twin. `identify`
measures the GEOMETRY and not the sensor event, at the stated price and for the stated reason; the IFF
half is beside the verdict in the log (`radar IFF_REPLY … reply=none`), never inside it.
**`FBObjectiveCovers` returns false for all four**, and that is measured rather than asserted:
`missions/objective-covers-none.fbm` exits 1 with `decisive=1` on the shot-down striker, and with the
predicate patched to cover, the same file exits 0 — one line of source, two verdicts. Honest limit,
measured too: inside a single mission a wrong `protect` cover is invisible, because the protector's own
FAIL is decisive either way; that half is held by the exhaustive `switch` (`-Werror=switch`). Eight new
missions, seven of them a pair ONE number apart, cover fulfilment and violation of every kind that can
be violated: 0/3 for the identification box, 1 for the broken weapons hold, 0/1 for `protect`, 0/3 for
`deny release` — the last pair being the one thing `kill` cannot say, since the kill succeeds in both.
Conservation held at full strength against the pre-round binary: **260/260 telemetry files and 85/85
`events.log` of the 85 pre-round missions byte-identical** (modulo `wallS`/`speedup`/path) at
`--threads` 1, 2 and 4.

### 2026-07-28 — C0: the campaign layer — an order, three facts, and two determinism proofs (this round)

The fourth and last foundation contract, built where its spec put it: `core/FBCampaignFile` parses the
`.fbc`, `core/FBCampaignState` holds the three carried facts as canonical text and applies them,
`missions/FBCampaignRunner` loops `FBRunMission` and aggregates, `fb-gym --campaign` drives it. The
mission runner grew **one** optional parameter (`const FBMissionCarry *`) and no phase: the overlay
lands between step 1 and step 2, the outcome is read off the same actors step 4 judges. Null carry =
the run that existed before — measured over all **104** `sim/missions/*.fbm` + `negative/*.fbm`
against a reference binary built from the same tree with the five touched files reverted: **104/104
fingerprints identical** (exit code, telemetry bytes, `events.log`).
The overlay came out NARROWER than the contract allowed: the spec permitted changing the value of an
existing `set` line, nothing needed it, and `FBApplyCampaignCarry` therefore contains no path that
writes a value — it erases a `unit` block or a `set store` line and asserts afterwards that neither
count grew. That is measured in both directions: `viper-attrition` drops `bandit`, `bunker` and three
`set store` lines with a `campaign CARRY` line each, while a hand-written state file demanding
`mk82=9`, a unit `ghost` and a `ground newtarget` produces the **identical fingerprint to the run with
no state at all** and zero `CARRY` lines. Stores land as a per-(unit, kind) stock that enters the book
on the first sortie declaring the kind — so a type the jet has never carried is not capped, which is
the difference between attrition and inventing state.
Both acceptance criteria of §5 held, on BOTH ground bases: criterion 1 gives **9 runs, one campaign
fingerprint** each (`f6dda7e6…` under `swiss`, fb-gym's own default, `0811c2cc…` under `const`, what
the four missions declare), 7.9 s for nine runs; criterion 2, the one that matters, gives **4/4 steps
reproducing STANDALONE** from the previous step's `campaign-state.txt` alone, fingerprint and exit code,
under both. The layer adds no hidden state.
The first version of criterion 2 was a FALSE PASS that a central re-check turned into a false alarm:
`fb_campaign_verify.py` defaulted its own `--elev` to `const` while `fb-gym` defaults to `swiss`, so a
campaign started the way a human starts it replayed 4/4 DIVERGED on `groundAsl=782.97` against
`groundAsl=0`. The lesson is not a better default. A fingerprint compares two runs over the SAME ground
or nothing, so the run now RECORDS the ground beside its state (`elev`/`swiss_dem`/`base`/`threads` in
`campaign-summary.txt`, carried in by `FBCampaignEnv` because the runner sees only an
`FBElevationProvider&`), the check READS it, `--elev` became an override, and a tree without the record
is refused instead of replayed against an assumption. The comparability rule is now in the Spec, as a
fourth way determinism can be lost. Result-relevant switches, enumerated: `--elev`, `--swiss-dem`,
`--base` (recorded); `--threads` recorded and result-neutral by criterion 1; `--timeout` structurally
unreachable for a campaign step; weather and clock have no gym flag at all. The tool also names the
fingerprint's normalisation exactly (`wallS`/`speedup` and the `--out` path inside `telemetry=`,
nothing else) — without the second, criterion 2 cannot even be stated.
The campaign itself is the demonstration: two of its four steps change verdict because the carry
worked — the DLZ rig finds its bandit dead and flies with the one round the first sortie left it (0 →
3), and the CCIP pass finds the bunker already rubble (0 → 3). `fb_tournament.py` stays where it is;
one measures a pilot over independent geometries, the other a force over a dependent sequence.

### 2026-07-28 — C1: the unit level, specified — one class, nine rows, and two collisions (spec only)

Step 2 of the owner goal, and a **spec round with no code**: `doc/modules/ground/` (`INDEX.md`,
`module.md`, `catalogue.md`, `cast.md`). `C1` blocks six campaigns and is the top four rows of the
aggregated cast table collapsed into one system; the gap entry that stood in `weapons.md` with five open
questions is now a pointer, and the five are answered.
The decision the round exists for is a **line**, not a feature: a module is a *flown* airframe (FDM,
avionics bus, pilot phase machine, one class per type, a reference base of its own); a unit is one
data-driven class with N catalogue rows. The test is not importance but **"does anybody fly it"**. Both
levels share the whole of `FBSimUnit` — identity, published pose and signature, health register, damage
model, roster, telemetry, mission judge, the `.fbm` `unit` block, the registry key — and differ in exactly
the four things a jet has because somebody sits in it.
Three structural answers carry the round. **The registry gate does not widen:** the site's four detectors
derive from `FBRadarSystem` (×2), `FBVisualSystem` and `FBRwrSystem`, and a derivation adds no include —
`verify-layers` printing *6 restricted header(s) respected* is an acceptance criterion, not an intention.
**The health register is used, not inherited:** `FBSystemHealth` stays monotone, private-mutator,
one-friend and untouched, and killing a site's `Radar` silences it through §8's coupling, written years
before this. **Doctrine is a sensor:** `set emcon hold` means the set is dark until the site's own passive
receiver hears an airborne emitter — a timer or a range trigger would be the site knowing something it
never measured, and the resulting experiment (a silent attacker never cues the defence) is `w4-01/02` from
the defender's side.
The round asks the tree for **one** core change and two enum values: `FBUnitSignature` carries two emitter
beams (a battery is two antennas; collapsing them by precedence deletes the search→track transition for
every observer except the tracked one), `FBSeekerKind::CommandGuided` (the uplink branch of the existing
phase machine, forever), and `FBEmitterKind::SurfaceEarlyWarning`/`SurfaceFireControl` — the discriminator
`sensors.md` gap 25 was waiting for, deliberately left unwired.
**Two collisions found by writing it, both booked in `weapons.md`:** the weight-on-wheels interlock refuses
100 % of ground launches (a unit without an airframe reports `AnyWow = true` by definition), and the SMS
declares its station masses through an `FBFdm` a site does not have. Both are rules written for a pilot
meeting a machine that has none. A third finding stays open: `CombatEffective()` is `Structure` alone on a
site, so **suppressed and destroyed are the same word** — and SEAD is precisely the difference.
Two things fell out of existing measurements without being tuned. The eye's measured reach (3 784 m
beam-on, 2 493 m head-on, **zero at night**) is shorter than every MANPADS envelope, so the *sensor* binds
a MANPADS engagement and the weapon becomes a last-two-kilometres daylight weapon. And on a stationary
mount `OwnClosureOn = 0`, so the Doppler notch degenerates to "the target's own range rate" — the beam
manoeuvre works against a ground set with no new code, and a conical-scan row declaring notch 0 is simply
not notchable, which is the honest statement about that hardware.
The catalogue is nine rows with a source and a tier each, seven disputes carried unresolved, and thin
sourcing declared: eight rows rest on [T4], one has a [T3] monograph that disagrees with the [T4] entry on
the envelope, and no [T1] threat handbook was read. The honest headline of the whole spec: a site can be
**heard and not seen** (no air-to-ground radar mode, no HARM), so `C1` gives the ground the ability to
shoot back long before it gives the air the ability to shoot first.

## 2026-07-28 — `C1` gebaut: die aktive Bodenbedrohung

Neun Stellungen, EINE Klasse (`modules/ground/FBSiteModule` + `core/FBSite.h`), gebaut gegen das am
selben Tag geschriebene `## Spec`. Die drei benannten Hindernisse haben getragen, zwei davon anders als
vorgeschlagen: `FBUnitSignature` trägt jetzt `Radar[2]` und **303/303 Telemetriedateien plus 104/104
`events.log` aller Bestandsmissionen bleiben byte-gleich** (Threads 1/2/4) — bauartbedingt, weil ein
Flugzeug nur Index 0 schreibt und die RWR-Schleife bei einer Keule dieselbe Arbeit tut wie der
Skalarzugriff vorher. Die Gewichts-auf-Rädern-Verriegelung wurde NICHT gelockert, sondern als
unanwendbar erklärt: `DeclareGroundLauncher()` ist privat mit genau EINEM Freund (dasselbe Schreibtor
wie `FBSystemHealth`), gibt false zurück, sobald je eine Zelle gebunden wurde, und `AttachFdm`
assertiert die Gegenrichtung — ein Flugzeug kann die Zeile nicht einmal aufrufen. Die zweite Kollision
(„der SMS setzt eine Zelle voraus") war bereits gelöst: `PublishLoadout` und `Release` halten ihre
`Fdm_`-Wächter seit jeher.

Zwei vorhergesagte Effekte, beide reproduziert ohne eine Zeile dafür: der Doppler-Notch auf stehendem
Mast degeneriert zur reinen Zielradialgeschwindigkeit (`ownClosMs=0`, gemessen), und das Auge bindet die
MANPADS auf 3 288 m am Tag und auf NICHTS in der Nacht (dieselbe Datei, eine Zeile Unterschied).

Verworfen mit Messung: eine MANPADS trifft in dieser Geometrie NICHT (883 m), weil eine Runde auf der
Schiene keinen Sucher-Ton hat — die Lücke steht als B1 in `## Gaps` statt als breiteres Sucherfeld im
Code. Ein Abnahmekriterium des Vertrags maß nichts: `verify-layers` druckte die Zahl der geschützten
Header (zwei), nicht die Länge der Registry-Leserliste (sechs); das Werkzeug druckt jetzt die Liste
selbst, und sie ist unverändert **6**.

## 2026-07-28 — Verbundene Luftabwehr: das Netz bewegt eine Antenne, es erzeugt nie einen Track

`C22`/`C23`/`C24` gebaut, `C13` halbiert. Der ganze Bau ist ZWEI Wertheader (`core/FBNetReport.h`,
`core/FBZone.h`), vier Setter plus ein Test am bestehenden `sensors/FBDatalinkSystem`, vier kurze
Schritte an der bestehenden `modules/ground/FBSiteFireControl`, eine Zielart, zwei Missionsgeltungs-
bereiche und ein publizierter Skalar. **Keine neue Klasse liest die Registry** — `verify-layers` meldet
weiterhin *6 registry reader(s) inside the perception boundary*, und ein siebter wird nachweislich
abgewiesen (gegengeprüft, rc=1). `core/FBZone.h` kommt als geschützter Header mit LEERER Includer-Liste
dazu: ein Pilot, der einen deklarierten Gürtel läse, wüsste ohne Sensor, wo die SAMs stehen — auch das
gegengeprüft (rc=1).

Der tragende Satz ist gemessen: `net-blind-cue.fbm` legt eine Mk 82 auf 52,32 m neben eine eingewiesene
Stellung (2 086,81 J/m² — `Radar` FAILED, `FireControl`/`Structure`/`Stores` nur degradiert), die
Einweisung liegt von t=8,1 s bis zum Ende ununterbrochen an (`net_cue` = 1, keine weitere Transition),
und es entstehen **null** `site TRACK`-Zeilen.

Was das Netz wert ist, in einer Zahl: dieselbe Geometrie mit und ohne `net`-Block ergibt 2 `site LAUNCH`
gegen 0 und `site RADIATE` bei t=8,0 s gegen nie. Der Schichtkuchen: dieselbe Route, nur die Höhe
verschieden, ergibt 34,5 s in `flak` / 0 Starts / 54 Feuerstöße gegen 0,0 s in `flak`, 320,0 s in
`sambelt` / 1 Start / 0 Feuerstöße. Blind gegen zuversichtlich blind: mitten im Lauf gestört verliert
die Stellung ihren Knoten bei t=128,0 s, nachdem sie sich seit t=8,0 s durch Strahlen verraten hat; von
Anfang an gestört bleibt sie stumm und unsichtbar. Und Störung nimmt NUR die Leitung: die `site
TRACK`-Zeile ist byte-identisch zur ungestörten (`brgDeg=206.713 rangeM=21977.2 closureMs=223.135`).

Zwei Funde beim Bauen. Erstens: `RadioHorizonM` rechnete mit NN-Höhen — zwei Stellungen auf 936 m ASL
„sahen" einander 252 km weit. Jetzt sind beide Argumente Höhen ÜBER GRUND plus die deklarierte
Masthöhe des Netzes; **die Luftreichweiten ändert das nachweislich nicht** (336/336 Telemetrien,
112/112 events.log byte-identisch), weil der Horizont auf Jägerhöhe nie gegen die 150 nm des Terminals
bindet. Zweitens: `emcon hold` prüfte `ThreatCount > 0` statt „airborne emitter", wie die Spec es sagt —
eine Batterie ging hoch, weil das eigene Frühwarnradar nebenan drehte. Ab dieser Runde steht so eines
nebenan; korrigiert, byte-identisch.

Verworfen mit Messung: die Einweisung als DETEKTIONS-Vorteil ist in diesem Baum nicht messbar. Ohne
Terrainmaskierung (`C4`) findet ein 50-km-Suchset alles innerhalb von 50 km, gleich welches
Elevationsfenster — was der Cue messbar wert ist, ist das AUFWECKEN einer stummen Stellung und die
Feuerleitautorität, also Doktrin statt Detektion. Steht so in `## Gaps`.

## 2026-07-28 — Die Luft-Boden-Hälfte: eine Waffe, die auf Sender zielt, und ein Gegner, der abschalten kann

`C8` (ohne Raketenpod), `C26` und `C27` geschlossen; `C25` unberührt. Sechs Stores fliegen — `agm88`
`mk84` `gbu12` `cbu87` `fab250` `fab500` —, zwei neue `FBSeekerKind`-Werte, die Abwurfhülle als
Prüfung 8, `objective suppress`, `set emcon react`, `set attack_mode arm`. Der Antiradiationssucher IST
der Warnempfänger (`FBMissileArSeeker : FBRwrSystem`, zwei bestehende Hooks, **kein neuer Include**):
`verify-layers` meldet unverändert *6 registry reader(s) inside the perception boundary*. Alle 113
übrigen Missionen byte-identisch, Telemetrie UND `events.log`, über `--threads 1/2/4`.

**Abschalten hilft, und die Grenze ist gemessen statt gesetzt.** Das Gedächtnis ist eine RATE: nach dem
letzten Empfang wird die gemessene Sichtlinienrate 4 s gehalten, dann null — die Proportionalnavigation
befiehlt seitlich nichts mehr, die Schwerkraftvorspannung überlebt, der Ausrollflug ist gerade. Die
Entkommensgrenze bei 20 km liegt gemessen bei **85,0 %** der Flugzeit (5°-Schuss) und **88,1 %**
(35°-Schuss) — vorhergesagt waren 61 % und 76 %. Die QUALITATIVE Vorhersage hält exakt: der Frontalschuss
ist der schwer zu entkommende, und das fällt aus der Geometrie. Die quantitative ist zugunsten des
Angreifers optimistisch, aus drei benannten Gründen: `ZEM(0)` in der Spec ist nur die seitliche Hälfte
(die 11,3°-Depression trägt 3,9 km), der Abfall ist gemessen `(t_go/t_f)^2.3` statt `^4` auf einer
verzögernden Runde, und das Gedächtnis verschiebt die Grenze um +6,4 pp. Nicht angeglichen, gemeldet.

**Die Bruch-Vorhersage für `B1` war zweifach falsch.** Nicht zwei `events.log` ändern sich, sondern
fünf (dazu `deny-release-broken` `escort-protect` `escort-protect-lost` — genau der Fall, den die Spec
in ihrem eigenen Restrisiko-Absatz nannte: der RWR der F-16 ist per Default an), und fünfzehn
Telemetriedateien in acht Missionen bewegen zwei Spalten, die die Vorhersage gar nicht bedacht hatte:
`fcr_on` und `iff_xpdr` des SENDERS über sich selbst. Kein Verhalten ändert sich irgendwo — jeder Diff
ist eine reine Löschung von Phantomzeilen. Und `net-blind-cue`s `set alert cold` war KEINE Umgehung:
ohne die Zeile bekommt die Batterie einen festen Track und die Datei verliert die Null, die sie zeigen
soll. Kommentar korrigiert, Vorhersage zurückgezogen.

Zwei weitere Funde. `msl_sig` (Spec §9) und Byte-Identität (Spec §10, Kriterium 2) schließen einander
aus — `msl_*` ist nicht die letzte Quelle am Bus einer Runde, eine Spalte dort verschiebt 95 gemessene
Dateien; die Anhänge-Regel gewinnt, die Zahl steht in `rwr_leth` und in den Ereignissen. Und ein F-16
mit bugfestem Bezeichner kann eine Lenkbombe aus einem waagerechten Anflug **nie** bis zum Einschlag
beleuchten: eine antriebslose Bombe legt Boden mit `v·cos θ` zurück, der Jet mit `v`, also ist er immer
zuerst da. Bei 250 kt/4 000 m hält er den Fleck durch und die Bombe trifft auf 3,9 m — bei 450 kt
verliert er ihn 5,7 s vorher und liegt 229 m kurz. Steht als F4 in `## Gaps`.

Nicht gebaut und benannt: der Raketenpod (`hydra70`/`s8`, Design C) und die Luft-Boden-Entfernungs-
messung (`C25`). Ein Schlagzeug, das die Spec noch nicht hatte: `set emcon` nimmt jetzt einen
gebrieften Emissionsplan (`free <offS> [<onS>]`) — ein Wert an einem bestehenden Schlüssel, sonst ist
das Entkommensfenster nicht messbar, weil `scoot_s` einen Start und `react` einen Treffer braucht.

## 2026-07-28 — Vier Katalogzeilen von ALPHA auf ACCEPTED, und ein Instrument, das seine eigene Regel las

Vier gemessene Ursachen, in der Reihenfolge, in der sie gewirkt haben — und die erste war eine andere
als benannt. Der Schubkanal war seit `d1e1d79` da und der Nachbrenner brannte; an seiner Stelle stand
der **Prüfstand**: der Tank lief WÄHREND der Messung leer (`f15c` verlor die Augmentation bei t = 870 s
und M 2,04 wurde als Vmax gebucht, während `(T−D)/W` noch bei +0,025 stand), und acht Zeilen flogen
unter der Startmasse, auf die jeder Anker bezogen ist. Beides festgehalten macht die Residuen ZUERST
schlechter (A1 −18,4 → −23,4 auf `f15c`) und alles Folgende überhaupt lesbar.

Dann die eigentliche Ursache von A1, und sie war weder Widerstand noch Schub, sondern **Buchführung im
Deck**: der auftriebsabhängige Widerstand stand als Tabelle über α mit 5°-Stützstellen, und ein
Überschall-Dash sitzt bei 1,8° — die Sehne durch eine Parabel liefert dort das 2,9-fache
(`CDi = 0,00519` gegen `k·CL² = 0,00179`). Dazu `kCLmach` doppelt gebucht. Gegen `aero/cl-squared`
geschrieben, mit 1°-Stützstellen: A1 auf sechs von zehn Zeilen von −20 % auf −3 %.

Die Startstrecke war nicht das Fahrwerk. `Cmδe`, „INV gegen A5" gerechnet, kam 7,5-fach unter dem, was
das eigene Leitwerksvolumen aus §2 hergibt, und konnte keine Nase heben — Vollausschlag ab 167 kt hielt
2,8° Nicklage bis 226 kt. `[GEO]` aus dem Leitwerk: alle drei veröffentlichten Startstrecken im Band
(−17,8 / +0,7 / +9,7 %).

A4 bleibt Sonde, aber **je Zeile und gerechnet**: für welche Masse ist die veröffentlichte Steigrate mit
dem eingefrorenen Schub erreichbar? `f15c` 13 141 kg, `su22` 9 982, `mirf1` 6 024 — alle drei UNTER der
eigenen Leermasse. `mig17` erreicht seine 65 m/s bei Startmasse und behält A4 als Anker (−1,9 %).

Die Abfangmaschine flog jede Zeile mit den Gains der MiG-29. `FBFlightControl::Raw(P, α_lim, g_lim)` ist
der fehlende Satz: der Grenzwinkel der ZEILE, eine hergeleitete Nickautorität (voller Stick trimmt das
1,5-fache des eigenen Grenzwinkels) und die g-Gains auf diese Autorität normiert. Dazu der
g-Begrenzer, den §6 seit jeher versprach und den die Klasse nicht hatte. `air-bomber-intercept.fbm`
hält 8 002,8 m über 400 s statt bei 510 m aufzuschlagen.

**Das Instrument:** die 2,4 gegen 1 203,6 waren kein Widerspruch, sondern eine zweiseitige Lesart einer
einseitigen Regel. Eine differentielle Empfindlichkeit (±10 %) neben einer endlichen Differenz (ein
anderes Flugzeug) — die gesunde Signatur. Der Defekt, den §Spec 11 wirklich benennt, ist die andere
Kombination, und `fb_tournament.py` prüft ihn jetzt, statt die Zahl zu drucken. Für die Spenderzeile
selbst gibt es keine Zelle: Regel statt Zahl. Und die reparierte Prüfung fällt sofort durch — die Arena
ist gesättigt (`mig21`: 19 Läufe, ein einziger unterscheidet sich), was vorher ein Verhältnis zweier
Gleitkomma-Nullen als 0,6295 verdeckte.

**4 ACCEPTED** (`f15c` `mig21` `mirf1` `f5e`), 6 ALPHA mit je ein bis zwei benannten Ankern: die
Spreizflügel-Wahl kostet 26–28 % Dienstgipfelhöhe (R14, im Voraus deklariert), die Turbojet-Schublapse
−14/−23 % auf zwei Zeilen (R15), der Begrenzerabfall 1,2° auf `mig17` (R16), und `su27`s A1 −6,3 % ist
die eine Abweichung ohne gefundene Ursache (R17). Kein Band geweitet. Rückschritt, gemessen und
benannt: `air-awacs-cue`s Kriterium 5 galt nur, weil der Abfangjäger dabei abstürzte — er fliegt jetzt,
und die Zelle erfasst nicht mehr.


### 2026-07-29 — Doktrin-Evolution `E1`: Fitness, Genom, Archiv, Arena

**Wofür die Runde war.** `doc/doctrine-evolution.md` war eine reine Spezifikation. Diese Runde hat sie
gebaut, in der Reihenfolge, die sie selbst vorschreibt — erst die Eingabe der mittleren Stufe, dann die
Fitness, dann die Arena, dann Gene und Archiv — und nach jedem Schritt gemessen.

**Der Richter veröffentlicht jetzt einen Zielvektor.** `mission OBJECTIVE unit=… kind=… state=met|unmet|
violated`, eine Zeile je erklärtem Ziel, an dem EINEN Punkt, den jeder Abschluss passiert
(`FBMissionMonitor::Conclude`), nicht in `Finalize` — eine Einheit, die im `Tick` FAILt, käme dort nie
an. Preis wie angekündigt und gemessen: **432/432 Telemetriedateien byteidentisch**, 77 von 137
`events.log` unverändert, 60 um genau **136** Zeilen gewachsen, keine entfernt, keine andere bewegt.

**Die Fitness ist lexikographisch und wohnt in einer Datei.** `(V, M, C)`, links nach rechts verglichen,
Aggregation über paarweise Dominanz statt über einen Mittelwert. `hits landed` und `no shot` sind
ENTFERNT, nicht neu gewichtet; letzteres ersetzt ein Tor, das man nicht zurückkaufen kann.

**Beleg C, abgelesen statt hergeleitet.** Der Mechanismus stimmt exakt — ein Bündel IST ein Tick
(`gun BURST rounds=1 → 5 → 10` im 0,1-s-Takt), `NoteHit()` genau einmal je Bündel, `dmg_hits` gleich der
Zahl der `gun HIT`-Zeilen (8/8, 24/24, 23/23). Die ZAHL stimmt nicht: vorhergesagt 1.500 Fitnesspunkte je
Sekunde Feuer, gemessen **900 / 1.038 / 1.278** — die Herleitung ist eine Obergrenze, weil
`kMinReportedHits` 15–40 % der Bündel wegfiltert.

**Beleg A hat sich NICHT umgedreht, und das ist der wichtigste Befund.** Auf `mirror` steht die
Einzelkämpfer-Variante weiter oben — aber der Abstand fällt von 120,7 Punkten `hits landed` auf **0,9
Punkte `shot lead`** bei exaktem Gleichstand in V und M über alle acht Läufe. Auf `split`, der einzigen
Geometrie des Paars, die überhaupt etwas entscheidet, liegt die kooperative Variante auf **beiden**
entscheidenden Stufen vorn (V 4,25 gegen 4,00). Die Spezifikation behauptet in ihrem Wissensteil, die
neue Fitness sei in einer gesättigten Arena „ehrlich still" — sie ist es nicht, weil C bei Gleichstand
konsultiert wird und zwei Fließkommazahlen nie gleich sind. Statt an der Ordnung zu drehen, meldet das
Werkzeug es jetzt: `decided at level: V 2  M 0  C 18` und ein `SATURATED`-Block.

**Die Arena war gesättigter als die Zahl, die das Kriterium erzwungen hat.** `mirror` — die Geometrie,
auf der jedes veröffentlichte F-16-Doktrinergebnis dieses Baums gemessen wurde — hat **100 % modale
Ergebnisklasse und 1 von 9 Hebeln**. Die alte Arena (2 Geometrien, 1 informativ) fällt durch. Elf
F-16-gegen-F-16-Kandidaten wurden geflogen und alle fielen durch; entsättigt hat nicht die Geometrie,
sondern die **Zelle**: die neue Arena hat 8 Geometrien und **4 informative** (`far`, `split`, `xmirror`,
`xclose`), drei davon mit einer MiG-29 im Ostsitz oder mit langem Anflug. S3 ist auf dieser Arena nicht
berechenbar und sagt das, statt eine Null zu drucken — ihr Instrument stört einen GENERIERTEN
Katalogdeck, und F-16 und MiG-29 sind Prinzip-1-Modellkopien.

**Das Genom kann keinen Absolutwert buchstabieren, und das ist Syntax.** `FBPilotTuning` trägt jetzt
`Free` und `Scale`; ein `Scale`-Eintrag verlässt die Klasse nur durch `Scaled(p, own) = own · Or(p, 1)`
und zwei `static_assert`s weisen ein `Scale`-Band zurück, das nicht dimensionslos ist oder keinen Haken
nennt. Die Laufzeit-Hälfte ist ein Exit-Code: `genome-absolute-refused.fbm` schreibt die eigene
Eckgeschwindigkeit des Jets (380) ins Gen und **endet mit 1** vor dem ersten Tick;
`genome-scale-flown.fbm` ist dieselbe Datei mit 0,85 und bewegt den Gashebel in **2.979 von 3.001**
Ticks. `fb-gym --pilot-keys` druckt das Alphabet, damit kein Werkzeug eine zweite Kopie der Tabelle führt.

**Archiv und Kreis-Messung stehen, und der erste Lauf misst einen Gleichstand.** Nicht-dominierte
Aufnahme, deterministische Schrittprobe, Kappe 64, alle drei Instrumente (fester Maßstab, zyklische
Tripel `T`, Doktrin-Trajektorie). Beide Läufe: **jedes Individuum exakt 0,500**. Grund, benannt statt
kaschiert: G4 wirkt nur in der `bfm`-Phase, die ein BVR-Abfang bis zum Timeout nie betritt, und G2 ist
inert — `flt_defer_s` = 0,0 in **132 von 132** Spuren an beiden Enden des Bandes, weil die AIM-120 0,3 s
bindet und die Regel damit nie feuert. Genom und Arena schneiden sich noch nicht; das steht als E-13.

**Tore.** `core-lib gym native wasm` warnungsfrei, `verify-layers` und `verify-models` grün,
`git status --porcelain sim/assets` vor und nach jedem Evolutionslauf leer, Determinismus über
`--threads 1/2/4` (434 Telemetriedateien, 139 Logs, 0 Unterschiede), und die von
`fb_tournament.py` erzeugten Missionen sind gegen das HEAD-Skript über beide Altgeometrien
**480 verglichen, 0 verschieden**.


### 2026-07-29 — Doktrin-Evolution `E2`: der Gleichstand war der VERGLEICH, nicht das Genom

**Der Auftrag war „Genom und Arena zum Schneiden bringen".** Die Diagnose der Vorrunde — jedes
Individuum 0,500, weil die Gene in dieser Arena nicht wirken — ist zur Hälfte falsch, und die falsche
Hälfte war die Ursache.

**Erst je Gen der Hebel, an einer Einzelmission, ohne Aggregation.** G1 und G5 sind keine Schlüssel:
`set pilot_flight_shape 1` bzw. `set pilot_emcon_frac 0.5` liefern `SET_INVALID_VALUE` +
`SET_REJECTED` bei t = 0,0 und **Exit 1** — harte Sperren (F5, D3), nichts zu messen. G4 greift, aber nur
in der `bfm`-Phase: `bfm_ctrl_s` 267,4 / 267,9 / **268,8** s und `bfm_es` 17.397 / 17.392 / **17.378** ft
über das Band, gegen einen LEBENDEN Gegner `bfm_es` 42.690 / 44.976 / **46.360** ft. G3 greift und die
Allel-Tabelle war falsch: `SORT_ASSIGN` 0 / 2 / 6 / 41 / 41 / 41 — die drei `dl=on`-Zeilen sind bis auf
die letzte Stelle identisch, weil die kooperative Zuweisung den gebrieften Vertrag überstimmt. Vier
Allele waren zwei. G2 ist NICHT inert: die 132/132 Nullen der Vorrunde standen auf `xmirror`, dessen
Ostsitz eine MiG ohne Datalink ist — mit Netz auf beiden Seiten `flt_defer_s` 0,0 → **6,3** s und
`flt_both_s` (5,6; 4,6) → (0; 0) auf `split`. Die obere Hälfte des Bandes bleibt tot.

**Und dann die eigentliche Ursache.** Auf `split --flight 2` trägt der WESTSITZ den Schlüssel:
West-C = +505,5…+526,7, Ost-C = **+69,0 in 12 von 12**. §1.4 vergleicht die beiden SEITEN EINES Laufs,
also über die Sitze hinweg — und gibt damit in beiden gespiegelten Läufen denselben Sieger zurück, jede
Variante nimmt genau einen von zwei Punkten, das Feld steht **konstruktionsbedingt** bei 0,500. Der Sitz
ist dort 457 Handwerkspunkte wert, das Genom 21,2. Gebaut: `fb_fitness.match_points` — ein MATCH ist das
gespiegelte Laufpaar, verglichen wird **Sitz gegen denselben Sitz**. Die Ordnung ist unangetastet.
A/B auf DENSELBEN Telemetrien: quer 0,500 × 12, gleich-Sitz **0,227…0,773**. Die veröffentlichten
Turniere bewegen sich nicht (Beleg A und B bit-gleich in ihren Zahlen) — ein heterogenes Feld kollabiert
unter der Querregel nicht, ein homogenes total.

**Der Nahkampf ist gebaut und entscheidet keine Doktrin.** Drei neue Geometrien mit eigenem
Missions-Profil (`merge`, `xmerge`, `xmergesplit`), weil `FBPilot` genau EINEN Übergang nach `Phase::Bfm`
hat und das der gebriefte Task beim Spawn ist. Gemessen über 70 Nahkampfläufe: **6 Feuerstöße, 0 Treffer,
0 Startvorgänge**, und was die Klasse `(2,0)` wirklich ist, ist die MiG-29 im Boden — 9 von 11
Ost-Ergebnissen `CRASH`. G4 bewegt als einziger Hebel eine Ergebnisklasse (2 von 9 auf `xmergesplit`),
und jede Geometrie, auf der es das tut, fällt durch S2. **Das Tor wurde nicht gelockert** — G4 bleibt
gesperrt, jetzt mit drei benannten Ursachen statt einer Vermutung.

**Die Arena, zweimal gemessen.** Bei `--flight 1` mit den erklärten neun Hebeln: 12 Geometrien, **4
informativ, BESTANDEN**. Bei `--flight 2` mit dem ALPHABET DES GENOMS (`tools/levers-genome.txt`,
`--flight N` neu im Tor — E-12): **1 informativ von 12, ABGELEHNT.** Die eine ist `xfarsplit` (langer
Anflug + Energiesplit + Waffenbindung, 3 Beweger von 9, alle drei G3-Allele, Feld 4 Klassen bei 48,3 %
Modalanteil); 17 Kandidaten wurden dafür gesiebt, 16 lieferten 0 oder 1. Ein 2v2 ist **gesättigter** als
ein 1v1: acht von zwölf Geometrien legen bei `--flight 2` jeden Lauf in dieselbe Klasse.

**Der Schnitt-Nachweis.** `xfarsplit --flight 2`, 4 Generationen × 6: Verteilung statt Mittelwert —
Gen 0 `0,700 0,700 0,400 0,400 0,400 0,400`, Gen 2 `0,773 0,773 0,500 0,500 0,500 0,227`. Entschieden
wird beim ERGEBNIS, nicht beim Handwerk: **312 Sitzvergleiche, V 96 · M 0 · C 152 · exakt gleich 64**,
vier Ergebnisklassen über 720 Seitenschlüssel. Kein §1 veröffentlicht: der Champion ist ein Fixpunkt
(Maßstab 0,583 flach, T = 0,0000), und die eine bewegende Geometrie hängt an einer einzigen
Hebel-Familie.

**Tore.** `core-lib gym native wasm` warnungsfrei, `verify-layers` (297 Dateien, 12 Schichten) und
`verify-models` grün, fünf Harnesses rc=0, `git status --porcelain sim/assets` vor und nach jedem
Evolutionslauf leer, Determinismus `--threads 1/2/4` über `xfarsplit`/`merge`/`xmerge`/`xmergesplit`/
`split` byte-gleich, und alle 140 bestehenden `.fbm`-Dateien unverändert.

## 2026-07-29 — Der Merge starb an einem Dämpfer, den nur der Autopilot hatte

**Der Befund kam vor der Korrektur, und er hat den Schuldigen ausgeschlossen statt ihn zu vermuten.**
E-15 las „was die Merge-Geometrien entscheiden, ist ein CFIT" — bei n = 120 Läufen je Durchgang sind es
**77 Monitor-K.O.s, und in 77 von 77 ist es die MiG-29** (38 ATTITUDE_CONTACT, 37 CFIT, 2
STRUCTURE_CONTACT). Die F-16 stirbt in keiner Merge-Zelle, in keinem Sitz. Sitztausch: die MiG stirbt
auch im Westsitz (t = 351,2); MiG gegen MiG überlebt 420 s. Also weder Geometrie noch Sitz.

**Der isolierende Versuch hat gar keinen Gegner.** Eine MiG-29 auf `set task bfm`, nächster Feind
100 km weg — nur der kalte verankerte Suchlauf, 300 s, drei Starthöhen, derselbe Missionstext ein Modul
weiter für die F-16: mittleres g-Kommando **4,57** gegen 1,22, mittlere Querlage **76°** gegen 40°,
p95 |VS| **183 m/s** gegen 9, **CFIT aus allen drei Höhen** gegen keinem. Die Ursache steht in den
ersten acht Sekunden: 0,32 Nickstick erzeugten **3,05 g auf ein 0,37-g-Kommando**, danach schlägt die
Schleife im 1,5-s-Takt zwischen +PitchStickMax und −kBfmPushMax um.

**Die Ursache war eine Zeile in der Zellenschicht, nicht im Piloten und nicht am Boden-Deckel.**
`KqDamp`/`KpDampRoll` sind der Dämpfer DIESER Zelle (SAU-451 „DAMPER", drei Achsen; flight-model-spec
§7.4 nennt ihn wörtlich „FBFlightControl's inner rate loop") und banden nur im FLCS-Zweig — BFM
kommandiert `Manual`. **Die MiG flog jeden Nahkampf mit ausgeschaltetem Dämpfer.** Es ist dieselbe
Auslassung wie bei `PitchStickMax` und dem Anstellwinkel-Begrenzer, eine Ebene tiefer (pilot.md §5.10a,
die fünfte Schraube). Beide Gains sind auf sich selbst getort; die F-16 hat sie exakt 0 und läuft den
Zweig nicht. Die Gierachse bleibt aus: dieser Ruderzweig ist GEMESSEN abgeschaltet.

**Was das Deck angeht: nicht angefasst.** Ein `Cmq` hätte dieselbe Kurzperiode gedämpft und niemand
hätte es gemerkt — es wäre kein belegtes Delta gewesen, und ein besseres Missionsergebnis ist kein
Beleg. `verify-models` grün, `sim/assets` byte-gleich.

**Vorher/nachher, dieselben 120 Läufe je Durchgang:** K.O.s **77 → 0**; Kanonen-Salven **191 → 386**,
und **→ 2 652** mit `gun HIT` **0 → 897**, nachdem das Merge-Profil die KANONEN-Kontrollposition brieft
(0,15–0,40 nm; die Vorgabe 0,5–1,5 nm ist eine Raketen-Halteposition außerhalb des Trichters, und
`Phase::Bfm` hat überhaupt keinen Raketenschuss — gemessen: 132,4 s Kontrollposition bei median 2,64 nm,
`gun_in_funnel` 0 in 4 200 Takten). `duel-merge` Exit 2 → 3, volle 300 s, kein K.O.; `mig29-bfm`
`bfm_ctrl_s` **0,0 → 287,6 s** bei unverändertem 60-°/s-Rolldeckel — die Kontrollposition, die §5.7.3
als Preis der Roll-Schranke verbucht hatte, war nie deren Preis.

**Und die Energieregel? Der Hebel greift, das Ergebnis nicht.** Dreipunkt-Sweep auf `xmergesplit`:
`bfm_es` 33 592 / 39 358 / 42 492 ft, `bfm_ctrl_s` 34,2 / 31,9 / 2,1 s, Schuss 150 / 150 / 0, Treffer
23 / 23 / 0 — **die NIEDRIGE Schiene konvertiert.** Die Ergebnisklasse bewegt sich trotzdem nicht: die
ganze Trommel legt **6,37 Schuss von 150** bei 3,41 m mittlerem Fehlabstand auf die F-16, drei Systeme
aus, `dmg_effective` 1,00. Also kein §1 veröffentlicht — und die Merge-Zellen verlieren ihren S1-Pass
mit dem Defekt, der ihn getragen hat (2 Klassen bei 50,0 % → 1 Klasse bei 100 %). Das Tor wurde NICHT
gelockert. Der Blocker ist jetzt ein einziger benannter: eine Kanone, die trifft und nicht tötet.

**Tore.** `core-lib gym native wasm` warnungsfrei, `verify-layers` (297 Dateien, 12 Schichten) und
`verify-models` grün, zehn Harnesses rc=0, Determinismus `--threads 1/2/4` über alle 139 Missionen
byte-gleich, und **134 von 139 Missionen byte-identisch** — die fünf, die sich bewegen, sind alle
MiG-29 und je einzeln begründet (`duel-merge` 2 → 3, `mig29-bfm` ctrl_s 0 → 287,6, `mig29-full` /
`mig29-landing` weichere Aufsetzer bei 143,2 → 143,3 bzw. 141,0 → 142,2 kt, `mig29-takeoff` −0,2 s).

---

## 2026-07-29 — Der Kurvenkampf kann töten: eine Waffe, die nie eingesetzt wurde, und ein Abzug, der in die Zukunft zielte

**Zwei Befunde, und der erste ist eine Herleitung, keine Zahl.** Die GSh-301 legte 6,37 von 150 Schuss
bei 3,41 m mittlerem Fehlabstand auf die F-16 und tötete nie. Aufgeteilt: ein `damage KILL` durch 30 mm
kostet **17,0 Treffer** in einer Zone (`kFlcsFail` 1,5·10⁵ J/m² gegen 8 803 J/m² je Treffer) — und bei
der Streuung, mit der der Merge tatsächlich gefochten wird (σ 3,78 m auf 630 m Geschossweg), landet eine
**perfekt gezielte** Trommel 20,2 Schuss und tötet. Also ist die Wirkung nicht der Deckel, die **Zielgüte**
ist es: die Trommel tötet bei einem mittleren Fehlabstand ≤ 2,38 m, gemessen wurden 8,72 m. Und
`missM ≈ RangeM·tan(GunAimErrorDeg)` auf den weiten Bündeln beweist, warum: der Zielfehler **an der
Mündung** IST der Fehlabstand.

**Der Abzug sagte etwas voraus, das die Geschosse nichts anging.** `pred = err + rate·(Latenz +
Flugzeit)` extrapolierte eine EIN-TICK-Ableitung eine volle Sekunde weit. `FBGunSolveLead` beantwortet
aber „wohin muss das Rohr zeigen, damit eine JETZT abgefeuerte Runde später trifft" — die Zielbewegung
während der Flugzeit steckt bereits in der Lösung, eine abgeflogene Runde hat von einer sich danach
bessernden Zielung nichts. **11 von 13 Feuerstößen wurden außerhalb des eigenen Trichtertors
kommandiert, der erste 14,6-fach.** Der Horizont ist Latenz + halber Feuerstoß. Ergebnis über die
120-Lauf-Merge-Arena: Schuss auf dem Ziel **139,8 → 449,1** bei **6 318 → 5 850** verfeuerten (2,21 %
→ 7,68 %); `gun-turning` behält seinen Abschuss auf **279 → 209** Schuss.

**Der zweite Befund war ein fehlender Pfad, kein fehlender Wert.** `Phase::Bfm` endete im Kanonenfeuer;
AIM-9 und R-73 hingen ohne Einsatzweg an den Schienen. Erst spezifiziert (`pilot.md` §5.11), dann gebaut:
fünf Tore, jedes ein Instrumentenwert, keine neue Rechnung — gewählter Store ist Infrarot, Lock, in der
Startzone, im **Cueing**-Winkel der ZELLE (`BfmWvrCueDeg`, MiG 60° vom Schtschel-3UM, F-16 der Kardan der
Runde), und die vorige Runde hatte ihre Flugzeit. **Aspekt ist kein Tor** (beide Runden sind dokumentiert
allaspektfähig) und **eigene Last auch nicht** — dafür gibt es keinen Mechanismus, und eine Zahl ohne
Mechanismus wäre erfunden. Nach dem Start bindet nichts: `FBSeekerHandoverS(Infrared) = 0`.

**Der Kurvenkampf wird jetzt entschieden, und beide Zellen können sterben.** `duel-merge` Exit 3 → 0:
die AIM-9 kommt 1,93 m heran, 218 781 J/m², Flugsteuerung ausgefallen, Abschuss bei t = 10,5 s. Die neue
`duel-merge-stern.fbm` ist der Gegenbeweis — die R-73 auf der dokumentierten Heckviertel-Geometrie, 590
m/s Annäherung statt 1 050, kommt 1,86 m heran und tötet die F-16 bei t = 21,3 s. Frontal verliert die
MiG **auch wenn sie zuerst schießt**: die Abschussradien gegen diese Zelle sind 2,32 m (AIM-9M, 9,4 kg)
und 2,08 m (R-73, 7,4 kg). Über die Arena: Ergebnisklassen 60/60 (2,1) auf allen drei Zellen →
`xmerge` **30 (3,2) + 30 (1,0)**, jeder Lauf entschieden, alle 40 Abschüsse durch Flugkörper, keiner
durch die Kanone.

**Und die Energieregel bewegt jetzt die Ergebnisklasse — auf `merge`, mit allen drei Allelen, und der
Beweger ist ein CFIT.** Drei von 43 Monitor-K.O.s sind gesunde Zellen, und genau diese drei sind es; die
anderen 40 sind bereits abgeschossene Jets, die fallen. E-15s Regel gilt unverändert: eine Geometrie,
deren Klasse ein CFIT bewegt, misst den CFIT. G4 wird nicht veröffentlicht, `kMoversMin` nicht gesenkt,
`fb_arena_check.py` unverändert REFUSED.

**Die Versuchung, benannt und abgelehnt.** Die schnellste Art, die Kanone tödlich zu machen, war die
Fragilitätsleiter in `FBF16Damage` — vier `[SET]`-Zahlen, die auch jeden Gefechtskopf im Baum bepreisen.
Sie zu heben hätte AIM-120 und R-27R neu bewertet, um ein Ergebnis zu kaufen. Die Messung sagt, dass sie
nicht der Deckel ist. Ebenso abgelehnt: Rtr als WVR-Tor (die gespeicherte Tabelle gibt der AIM-9 22 740 m
Rtr auf 3,2 km Startentfernung — ein Tor, das nichts misst) und ein g-Limit ohne Trennmodell.

**Tore.** `core-lib gym native wasm` warnungsfrei, `verify-layers` (297 Dateien, 12 Schichten, 3
restricted headers, 6 Registry-Leser) und `verify-models` grün, acht Harnesses rc=0, Determinismus
`--threads 1/2/4` je ein Fingerabdruck, `git status --porcelain sim/assets` leer, **130 von 139
Missionen byte-identisch** — die neun, die sich bewegen, je einzeln begründet, plus eine neue
(`duel-merge-stern`). Ein Exit-Code bewegt sich: `duel-merge` 3 → 0.

## 2026-07-29 — Kampagne O4 gebaut und geflogen: die Zehn-Meilen-Behauptung, vermessen

**Schritt 4 des Eigner-Ziels beginnt, und O4 ist die erste**, weil sie als einzige der zehn Kampagnen
beide FlightBox-Zellen zeigt, die historisch wirklich gegeneinander flogen (JG 73 "Steinhoff", Laage,
~450 Einsätze gegen F-16 [T4]). Zehn `sim/missions/o4-*.fbm` plus `sim/campaigns/o4-gaf-mig29g-dact.fbc`,
Arena über der Ostsee (55,20 N 13,60 E [SET], `--elev const`, weil 0 m dort die Wahrheit ist), Bodenziele
in jeder Mission, jede Mission mit ihrer verbindlichen Leseregel im Kopf.

**Das Ergebnis ist eine Aussage, keine Zahl.** Die berühmte Behauptung des Fulcrum-Piloten — *"inside ten
nautical miles I'm hard to defeat"* — hält an ihrer eigenen Außenkante und stirbt im Messerkampf:
**10 nm MiG (R-73 Raero 20 km und ±60° Helmvisier bieten den Schuss 11,0 s vor der AIM-9), 5 nm
gegenseitiger Abschuss 0,1 s auseinander, 2 nm F-16 (9,4 kg gegen 7,4 kg Gefechtskopf, Ankunft 1,40 m
gegen 2,61 m).** Zwei der drei Gründe, die das Zitat nennt, sind modelliert und entscheiden; der dritte,
das IRST, trägt bei KEINER Entfernung etwas bei — kein Konsument im Piloten UND bei Frontalanflug 10 km
Reichweite statt 25 km von hinten (`irst_contacts` = 0 in beiden Läufen).

**Was die Kampagne findet und der Anker nicht nennt: das Magazin.** Die F-16 trägt einen
Drei-Einsatz-BVR-Vorrat, die MiG-29 einen Ein-Einsatz-Vorrat. Einsatz 3 fliegt mit dem, was 1 und 2
übrig ließen, und das Gefecht KIPPT: allein geflogen schießt die MiG zweimal und die F-16 nie
(R-27R 10,16 m), in der Kampagne schießt die F-16 (AIM-120 5,13 m) und die MiG hat keinen Radarflugkörper
mehr. Beide unentschieden, aus entgegengesetzten Gründen.

**Die zwei blockierten Missionen, gegen den heutigen Baum geprüft statt geglaubt.** Mission 6 (Merge)
läuft und ENTSCHEIDET — der Blocker war Wiedererfassung, und die Frage stellt sich nicht mehr, weil der
Kampf auf dem ersten Pass fällt. Mission 9 (Nacht) läuft, `C2` und `C3` sind gebaut — und kann ihre Frage
weiterhin nicht beantworten: von **184 Telemetriespalten unterscheiden sich sechs**, alle visuell, und
beide Läufe töten denselben Jet im selben Tick. Das Loch hat jetzt eine Zahl statt einer Behauptung.

**Kein Ergebnis mit zwei möglichen Ursachen ohne Kontrolllauf.** Das Wetter-Fixture ändert Wolke UND
Wind; der Wind-ohne-Wolke-Kontrolllauf reproduziert den Wetterlauf auf drei Dezimalstellen
(3,033/1,615/2,026 gegen 3,007/1,608/2,027 m). **Der Umschwung ist zu 100 % der Wind.** Was die Wolke
nimmt, ist gemessen und folgenlos: das AUGE der MiG, 50 Kontaktframes und RECOGNISED auf 0 Kontakte und
7 `vis MASKED` bei Transmission 0,011.

**Der Fingerabdruck fand einen echten Defekt in der Kampagnenschicht.** Kriterium 2 war beim ersten
Versuch **9 von 10 DIVERGED** — `fb-gym --mission --state` konnte die KAMPAGNENUHR nicht empfangen, und
nur Mission 9 (eigene Uhr) passte. `viper-attrition` deklariert keine `time` und konnte das nie zeigen.
Geschlossen wie §5 den Boden schließt: die Uhr wird AUFGEZEICHNET (`campaign-summary.txt: time`) und
GELESEN, Empfänger ist `fb-gym --campaign-time ISO` — Kampagnendaten, nie eine Client-Uhr.

**Tore.** `core-lib gym native wasm` warnungsfrei, `verify-layers` (297 Dateien, 12 Schichten) und
`verify-models` grün, acht Harnesses rc=0, `git status --porcelain sim/assets` leer. Determinismus:
**9 Läufe, 1 Kampagnen-Fingerabdruck** `461e0ff5299d83d03b…`, und **10/10 Schritte** einzeln aus dem
Vorzustand nachgespielt. Konservierung: **515/515 `telemetry*.csv` und 150/150 `events.log`
byte-identisch** gegen ein Binary mit den zwei berührten Quellen zurückgesetzt, 0 Unterschiede über
`--threads 1/2/4`. `viper-attrition` unverändert (9 Läufe 1 Fingerabdruck, 4/4 Replays).

## 2026-07-29 — Kampagne O1 gebaut und geflogen: die kanonische Niederlage hängt an 3,5 Sekunden

**Zehn `.fbm` + eine `.fbc`, ohne eine Zeile C++.** `sim/missions/o1-*.fbm` +
`sim/campaigns/o1-bekaa-1982.fbc` — Bekaa-Tal, 9. Juni 1982, die syrische Seite. `git status
--porcelain` listet elf unverfolgte und **null geänderte** Dateien: die Kampagne ist reiner
Missionstext, genau wie ihr eigenes Spec es vorhergesagt hatte. Konservierung ist damit keine Messung,
sondern eine Konstruktion — das Binary, das O1 flog, ist das Binary, das alles davor flog.

**Der Aufbau: ein Baseline, sechs Ein-Hebel-Varianten, ein Kontrollpaar, eine Zwei-Schritt-Kette.**
Jede Mission nennt ihre Kontrolle im Kopf. Die Regel, die dabei herausfiel und die die übrigen acht
Kampagnen erben: **ein Kettenschritt kann nicht zugleich eine kontrollierte Variante sein** — der
Übertrag ist auf den Rufnamen geschlüsselt, also unterscheidet sich ein erbender Schritt von jedem
Geschwister in zwei Dingen. Zehn Plätze fassen deshalb genau das obige, und O1 musste ein Spec-Paar
(gestaffelt gegen massiert) STREICHEN. Es sagt im `.fbc`-Kopf welches und warum.

**Das Spec irrte sich über seinen eigenen Mechanismus.** Es sagte, „zuversichtlich blind" brauche einen
Leitoffizier, der mitten im Lauf verstummen kann (`C6`). Falsch: `set brief_gci <atS> …` trägt seit
jeher seine eigene Zeit, also IST ein abgeschnittener Brief genau dieses Experiment. Die Fähigkeit
wurde gelesen, nicht gebaut.

**Die Hebel-Tabelle, gegen die Baseline (2 MiG kampfunfähig, 0 F-16):**

| Hebel | Rot verliert | Blau verliert | bewegt |
|---|---:|---:|---|
| — Baseline `o1-01` | 2 | 0 | — |
| GCI gelöscht, frontal | 1 | 0 | Rot-Kontakte 8→4, Schüsse 2→1; die Ausgangsänderung ist ein **Sort-Artefakt** (beide AMRAAM auf dieselbe MiG) |
| GCI abgeschnitten, frontal | 2 | 0 | **nichts** |
| Eintritt 45° | 0 | 0 | Blaus ganzen Schuss |
| GCI gelöscht bei 45° | 0 | 0 | **die ganze Begegnung**: 9→0 Kontakte, 2→0 Schüsse |
| RWR aus | 2 | 0 | **nichts** |
| `pilot_shot_rtr 1.4`, `lock_nm 16` | **0** | **2** | **die ganze Schlacht**, um 3,5 s Tempo |
| Kommunikationsstörung 0→90 km | — | — | `site LAUNCH` **5→0**, Cues 79→32 — **Bodenschaden auf den Meter und den Tick identisch** |

**Was übrig bleibt, wenn nichts es bewegt.** Der gesamte Ausgang sitzt in einer 3,5-Sekunden-
Abschussentscheidung. Alles andere — Leitoffizier frontal, Warnempfänger, Gürtel, Netz, die vom Anker
als entscheidend benannte Störung, der Kampagnen-Übertrag — bewegt Mechanismen und **keinen Ausgang**.
Zwei dieser Nullen sind Modelleigenschaften (der SPO-15 warnt 13 s vor dem Einschlag, weniger als
Reaktion + erste Abwehr + Düppelprogramm; frontal zeigt der gebriefte Wegpunkt die Nase schon auf den
Gegner), zwei sind Defekte.

**Der größte Fund ist vorbestehend und stand in einer eingecheckten Mission.** Die 3M9 des 2K12 und die
V-601 der S-125 kippen nach vorn und erreichen den Boden **an den eigenen Startkoordinaten**, 0,8–1,6 s
nach dem Abschuss; der 59-kg-Gefechtskopf der 3M9 wird dann als `damage KILL` auf der eigenen Batterie
verbucht. Sichtbar in `net-cue.fbm`, t = 172.8 s. Fünf Starts in Sortie 08, **null Ankünfte**, zwei
Selbsttötungen. Das macht vier geschlossene Lücken (`C1`/`C22`/`C23`/`C24`) und eine ganze Themendatei
unfähig, irgendeinen Ausgang zu bewegen. **Nicht behoben** — die Decks liegen unter
`sim/assets/aircraft/`, das diese Runde nicht anfassen darf.

**Zweiter Fund: eine Batterie hat kein IFF.** `FBSiteModule` setzt `SetIffInterrogator(false)`, ein
`FBRadarContact` trägt keine Identität — also beschießt eine Stellung die nächste feste Spur in ihrer
Hülle, egal wem sie gehört. Gemessen auf dem ersten Layout: drei V-750 in die eigene Kampfpatrouille
binnen 7 s. O1 weicht geometrisch aus und **kann damit die eigentliche syrische Taktik des Ankers
(„zurück unter den Schirm") gar nicht fliegen.**

**Dritter Fund: `FirstFlightKo` beendet den ganzen Lauf.** Ein Jagdduell und ein SEAD-Anflug in einer
Datei messen das Duell zweimal — die erste Fassung von Sortie 08 endete bei t = 237.0 s, mit den
Angreifern 130 km vor dem Ziel. Konsequenz: die SEAD-Paarung fliegt ohne Jäger, und die große Sortie
setzt ihre Angreifer hinter die Patrouillenlinie.

**Tore.** `core-lib gym native wasm` warnungsfrei, `verify-layers` (297 Dateien, 12 Schichten, 3
restricted, 6 Registry-Leser) und `verify-models` grün, sechs Harnesses rc=0, `git status --porcelain
sim/assets` leer. Determinismus: **9 Läufe, 1 Kampagnen-Fingerabdruck**
`81b549fd04c4591987b9dadf233deffdabbbfb01f9dc89f4f7f0d4486d7bba8e`, und **10/10 Schritte** beim ersten
Versuch einzeln aus dem Vorzustand nachgespielt — das Uhrenloch, das O4 gefunden hat, blieb geschlossen.

---

## 2026-07-29 — Der Bodenstart: drei Defekte auf unserer Seite der Naht, kein Deck angefasst

**Der größte Fund der O1-Runde ist behoben, und die Prognose dieser Runde war falsch.** „Das Beheben
heißt `sim/assets/aircraft/` anfassen" stand in `o1-bekaa-1982.md` und in diesem Journal — es hieß
nichts dergleichen. Sieben Dateien unter `sim/src/`, kein Deck, `verify-models` unverändert grün
(1 deklariertes Delta, 34 FlightBox-eigen).

**Defekt 1: die „kein Boden"-Regel der Waffe griff einen Aufruf zu spät.** `FBFdm::LoadUnguarded` rief
`RunIC()`, während JSBSims Gelände noch auf seinem eigenen Vorgabewert stand; die Geländehöhe kam erst
danach. `FGLGear` löst seine Kontakte **innerhalb** von `RunIC()` auf. Eine 6,09 m lange V-601 auf einer
70°-Schiene hat ihren Heckpunkt damit (6,09/2)·sin 70° = **2,86 m unter** dem Datum, die Feder antwortet
mit einem Drehimpuls, und der Integrator trägt ihn aus Schritt 1 heraus. Belegt mit einer **rohen
JSBSim-Sonde ohne jede FlightBox-Lenkung** — die Zahl gehört der Zelle, nicht dem Regler:

| Runde | Schiene | q bei Schritt 1 |
|---|---:|---:|
| `v601` | 90° | **0,000 °/s** |
| `v601` | 70° | **−79,284 °/s** |
| `v601` | 45° | **−114,76 °/s** |
| `3m9` | 45° | **−179,81 °/s** |

Behoben durch ein **eigenes** Feld `FBFdmSpawn::TerrainElevM`, vor der Anfangsbedingung angewandt und
getrennt von `GroundElevM`, das nur den Spawn platziert. Vorgabe ist JSBSims eigenes Datum — also genau
das, wogegen jeder Spawn in diesem Baum seine IC immer schon gerechnet hat. Die Konstante zog von zwei
anonymen Namensräumen in **eine** Klasse: `FBFdm::kNoGroundElevM`.

**Defekt 2: der Motor war 0,55 s kalt.** Bei t = 0,51 s war die Runde noch im freien Fall
(**4,98 m/s = 9,81·0,51**); aus 0,5 m Starthöhe sind das ½·9,81·0,55² = **1,48 m Sinken durch den Boden,
bevor überhaupt Schub anliegt**. Ein luftgestarteter Flugkörper fällt frei und zündet dann — genau das
ist die Schubrampe. Ein **Schienenstart** trennt sich, *weil* der Motor ihn von der Schiene geschoben
hat. Neues `FBFdmSpawn::MotorRunning`, gesetzt aus `FBStoreRelease::HaveRail`, also falsch für jeden
Speicher, der je einen Pylon verlassen hat.

**Defekt 3: `FBStoreSpec::GatherS` wurde von keiner Zeile Code gelesen.** Deklariert, für alle sechs
Bodenrunden mit Wert belegt, in zwei Doku-Dateien spezifiziert, nie gebaut — die Sammelphase existierte
nur auf dem Papier. Jetzt der frühe Rücksprung in `FBMissileGuidance::FlyCommand`: die Ruder **schleppen**,
das Gesetz darüber läuft weiter, und `msl_nz_cmd` ungleich null neben `msl_fin_pitch` gleich null **ist**
die Phase in der Spur. **Keine neue Zahl:** die sechs Werte standen bereits belegt im Katalog. Neben den
gerechneten Brenndauern `t = P·Isp/T` (4,499 / 2,498 / 3,995 / 1,992 / 1,975 / 1,982 s) fällt nur beim
V-601 beides zusammen; die fünf anderen bleiben bewusst unangeglichen — „Sammelphase endet bei
Boosterabwurf" ist bei einer Schulterwaffe schlicht falsch.

**Gemessen.**

| Prüfung | Ergebnis |
|---|---|
| V-601 im Flug, vorher/nachher | +70° → **−41°** in 1,6 s, Einschlag 7 m unter Grund **gegen** 70,00 / 69,97 / 69,95° gehalten und **868,8 kt = 447 m/s** am Ende der 2,5-s-Sammelphase |
| Selbstzerstörung | **null** — keine Stellung zerstört sich mehr, in keiner Mission |
| `o1-08` Detonationen am Angreifer | **9,15 / 8,57 / 4,87 m** (V-601, Zünder 10 m) und **0,28 / 4,09 m** (3M9, Zünder 8 m) |
| Der Störsender kostet jetzt alles | `o1-08` ungestört: 8 Starts, 7 Detonationen, `bolt1` abgeschossen, **alle fünf Bodenziele intakt**. `o1-09` gestört: **0 Starts, 0 Detonationen**, beide Angreifer „objectives met", **zwei Stellungen zerstört**. Vorher war der Bodenschaden beider auf den Meter und den Tick identisch — die Messung, die O1 als Defekt gebucht hatte, ist aufgelöst |
| Konservierung | **10 von 160** Missionen geändert, **150 byte-identisch**, alle zehn mit Bodenstart. Zwei Exit-Codes bewegen sich: `o1-08` 3→2 (der Angreifer wird abgeschossen und trifft *dann* den Boden), `sam-sa2-command` 0→1 |
| Determinismus | `--threads 1/2/4` auf vier bewegten Missionen: je ein Hash. Beide Kampagnen bestehen weiterhin beide Kriterien (O1: 9 Läufe ein Fingerabdruck, 10/10 Schritte; O4: 10/10) |
| Tore | `verify-layers` (297 Dateien, 6 Registry-Leser), `verify-models` grün, Bau warnungsfrei |

**Drei Defekte werden JETZT ERST SICHTBAR, und keiner davon ist der behobene.** Der Bodenkontakt hat sie
verdeckt — die Runde starb, bevor sie sie zeigen konnte. Alle drei stehen als Lücken B4/B5/B6 in
`modules/ground/module.md`:

1. **Die V-750 kann ihr eigenes Überkippen nicht ausführen.** Flach fordert die Lenkung nie mehr als
   **0,53 g** — eine 80°-Schiene wird gegen eine 2,5°-Sichtlinie nie herumgeholt; steil erreicht sie
   −1,23 g, geht von 80° auf 42° und trifft. Reine Proportionalnavigation plus Schwerkraftvorspannung hat
   **keinen Mechanismus** für ein großes kommandiertes Überkippen; eine echte S-75 fliegt ein
   **programmiertes**.
2. **Eine tragbare Runde mit ungültigem Feuerleitzustand beim Start entkäfigt ihren Sucher nie.** Der
   frühe Rücksprung `if (!HaveTarget_)` liegt **über** dem Block, der den Infrarotkopf entkäfigt; Sucher
   und Querbeschleunigung bleiben den ganzen Flug null. Das ist die genaue Ursache der älteren Lücke
   „MANPADS ohne Sucherton" (B1), die damit **offen bleibt**.
3. **Die geteilten Flugkörper-Reglerbeiwerte lassen die 9,8-kg-Schulterwaffe departieren** —
   Kontrollverlust bei 5,1 s Flugzeit, Ruder an den Anschlägen, Anstellwinkel ±4°. Ein Beiwertsatz über
   drei Größenordnungen Masse.

**Die Lehre, und sie korrigiert eine Regel im Kampagnen-Index:** nicht *ein* Fund pro Kampagne, sondern
ein **Stapel** — der erste Defekt verdeckt den nächsten. Und: ein deklariertes, belegtes, zweimal
spezifiziertes Feld, das niemand liest, ist keine Spezifikation, sondern eine Lüge mit Herleitung.

## 2026-07-29 — Kampagne O5 gebaut und geflogen: das Vokabular war die leichte Hälfte

Zehn `sim/missions/o5-*.fbm` + `sim/campaigns/o5-airfield-defence.fbc`, Batajnica 24.–26. März 1999,
`--elev const`, drei erklärte Nächte. Kein `sim/src/`, kein Deck, keine Zeile an den 160 bestehenden
Missionen — die Binärdatei, die O5 geflogen hat, ist die, die alles vorher geflogen hat. Beide
Determinismus-Kriterien beim ersten Versuch: 9 Läufe eine Kampagnen-Signatur
(`f59fc642c86ccecd2691…`), 10/10 Schritte einzeln reproduziert. Vollständige Messung in
[`campaigns/o5-airfield-defence.md`](../mods/f16/mods/f16/doc/campaigns/o5-airfield-defence.md) `## State`.

**Die Antwort auf die eigene Frage — was hält den Flugplatz?** Eine Rotte, die schon oben steht. Sie
verweigert einem Zweierpack die Hälfte seiner Bomben, und nichts sonst in diesem Baum kommt in die
Nähe. **Was umsonst ist:** der Lotse (6 s auf den ersten Blick des Flügelmanns, kein Ergebnis — die
dritte Kampagne, die das sagt), die Nacht, die innere Rohrwaffe, die gehärteten Shelter und die Bahn.
Der Gürtel verweigert einem von drei Angreifern die Freigabe — und **eine Mk 84 auf seinen P-18-Knoten
in Nacht eins kostet ihn zwei Nächte lang jeden Start** (7 → 0 und 6 → 0, standalone gegen Kampagne
gemessen). Genau die `C22`-Vorhersage der eigenen Spec, angekommen als Zahl.

**Drei Defekte, alle auf unserer Seite der Naht, keiner hier repariert:**

1. **Der Alarmstart ist nicht ausdrückbar.** `set task` setzt die Phase BEIM SPAWN und `FBPilot` hat
   keinen Übergang `Route` → `Intercept`. Ein Bodenstart mit Kampfauftrag rollt von der Bahn (FAIL bei
   t = 11,1 s) oder überschlägt sich (`ATTITUDE_CONTACT`, t = 35,4 s). Das ist der Parameter, den die
   Spec selbst den wichtigsten der Kampagne nennt.
2. **Keine Katalog-Zelle kann eine Waffe einsetzen.** `modules/air/FBAirModule` komponiert *keine*
   Feuerleitung, also wird `FBState::FireControl` nie geschrieben und alle drei Einsatztore in
   `FBPilot` bleiben zu. Eine `f15c` mit vier AIM-120 hält 28 s einen festen Lock von 18,6 auf 8,8 nm
   und drückt nie. Vier Zeilen sind `ACCEPTED` — als **Flugmodelle**, nicht als Kämpfer.
3. **Die GCI-Suchelevation ist ein Weltwinkel in einem Körperkommando.** `FBMig29Pilot` postet
   `atan2(Δh, R)` direkt auf `RadarSlewEl`; das eigene Suchgesetz in `FBPilot` zieht dort ausdrücklich
   `st.pitch` ab. Im waagerechten CAP fällt der Unterschied in die Kommando-Totzone — bei einem
   steigenden Abfangjäger ist er der ganze ±6°-Balken: **null Kontakte in 700 s über 726 m minimalen
   Abstand.**

**Und die Messung, die O5 als einzige Geometrie erzwingen konnte:** eine FlightBox-Stellung hat keinen
IFF-Abfrager, und ein Flugplatz ist der eine Ort, an dem das eigene Flugzeug die nächste feste Spur
ist. Der Gürtel schießt seine **ersten drei Runden nach Osten** (`brgDeg` 116,5 / 90,6 / 91,0) — auf die
eigenen MiG-29. Dass die Kette keine Maschine an eigene Flugkörper verliert, liegt einzig daran, dass
der Angreifer in Nacht eins den Knoten zerstört hat, der die Starts freigegeben hätte.

**Die Lehre:** das Zielvokabular (`C12`) war die leichte Hälfte und ist geflogen — `deny release`,
`protect`, `avoid zone`, `no_fire`, alle vier. Was es ersetzt hat, ist kleiner und härter: die Kampagne
konnte ihre eigene Hauptfrage nicht stellen, weil das Format den Alarmstart nicht kennt. Erwarte den
Defekt in der Naht, in die du nicht geschaut hast.

## 2026-07-29 — Ferne Berge waren nicht vernebelt: eine Luft für Decke und Gelände

Der Eigner hat es gesehen, die Messung hat es bestätigt: **`FBTilesStage` hatte null Referenzen auf
Sichtweite, Dunst oder Extinktion.** Die drei Stellen, an denen der Renderer die gemeldete Sichtweite
las, lagen alle in `FBCloudLayerStage.cpp`. Zwei Bilder derselben Kamera bei **5 km und bei 80 km
Sicht waren sha256-gleich** — null von 921 600 Pixeln verschieden. Genauso zwei Bilder bei 0 % und
100 % Bewölkung: der Geländeausschnitt **byte-identisch**, weil der Boden nicht wusste, dass eine
geschlossene Decke zwischen ihm und der Sonne stand.

An der Stelle lag `FB_AP`: eine vollständige, seit dem 23.07. abgeschaltete Luftperspektive. Sie ist
gelöscht, Schalter und Block. Der Grund ist nicht, dass sie kaputt war — sie war ein **Klarluftmodell**
aus der Rayleigh/Mie-Tabelle und konnte das Wetter prinzipiell nicht sehen. Ersetzt durch
`render/stages/FBAtmoHaze.h`: σ₀ = 3,912/Sichtweite (Koschmieder), ausgedünnt mit 8 km Skalenhöhe,
Einstreufarbe aus derselben Himmelstabelle — **eine Funktion, von beiden Shadern gespliced**, C++-Hälfte
danebengelegt und von `--cloudcheck AIR_RESULT` gegen ihren Shader-Zwilling gemessen (max |Δ| 1,19·10⁻⁷).
Danach: 100 % der Pixel verschieden, +57,6 % Helligkeit im Nahband zwischen 5 und 80 km.

Die Beleuchtung unter der Decke ist **keine Schattenkarte**, sondern der Anteil der Sonnenstrahlen, der
die Decke verfehlt: `(1−cover) + cover·exp(−τ·frac)`, pro Deck und pro Fragment. `frac` ist der Anteil
des Decks über dem Fragment — und genau der macht die Messung, die den Ansatz belegt: unter der Decke
**−30,8 %** Helligkeit, auf den Gipfeln über ihrer Obergrenze **+0,8 %**. Der richtungsabhängige
Direktanteil der Bodenstrahlung fällt von 0,882 auf **3,6·10⁻⁶**. Kosten: +0,36 ms fürs Gelände,
+0,29 ms zahlt der Wolkenpass für die geteilte Einstreufarbe — +4,1 % auf den Frame.

**Zwei Funde, die teurer sind als der Umbau.** Erstens: ein WGSL-Übersetzungsfehler
(`step(f32, vec3f)`) hat die Terrain-Pipeline still ungültig gemacht — das Bild sah plausibel aus, es
war nur der Himmel ohne Gelände. Seitdem bricht das Aufnahmeskript bei jedem `gpu_error` ab, statt ihn
zu protokollieren. Zweitens, und das ist die eigentliche Rechnung: **8 km ist die ISA-*Dichte*-Skalenhöhe,
Aerosol dünnt rund sechsmal schneller aus.** Mit den 24,1 km der Fixture überträgt Gelände 13,9 km unter
dem Flugzeug **0,275** bei H = 8000 m gegen **0,946** bei H = 1200 m. Das ist der Unterschied zwischen
„Neuenburgersee durch die Lücken" und Milchglas — und `p1` ist jetzt Milchglas. Die Konstante war
vorgegeben und wird geteilt wie verlangt; die Zahl steht als Lücke 5.7, nicht als Kommentar.

## 2026-07-29 — Zwei Skalenhöhen: der Dunst hört auf, ein Milchglas zu sein

Die Vorgabe der Runde davor war falsch, und zwar meine: **8 km ist die Dichte-Skalenhöhe der Luft, nicht
die des Aerosols**, das bei 24,1 km gemeldeter Sicht 92 % der Extinktion stellt. Der Mechanismus blieb —
eine Datei, beide Shader spleißen sie —, nur das Gesetz darin ist jetzt eine **Summe zweier Terme**:
molekular mit 8 000 m, Aerosol mit 1 200 m. Beide Zahlen sind veröffentlicht und stehen ohnehin schon im
Renderer: `FBAtmoCommon.h` baut seine Himmelstabelle mit `exp(-h/8.0)` und `exp(-h/1.2)` (Bruneton &
Neyret 2008; die 8 km zusätzlich Bucholtz 1995, die 1,2 km zusätzlich Elterman 1968).

**Die Aufteilung ist hergeleitet, nicht gestellt.** Die molekulare Extinktion sauberer Luft ist eine
Naturkonstante — dieselbe, mit der die Himmelstabelle rechnet, 1,3558·10⁻⁵ /m bei 550 nm, also 288 km
Rayleigh-Sichtweite. Also ist der molekulare Anteil fest und das **Aerosol trägt den Rest**: 8,4 % zu
91,6 % bei 24,1 km, 27,7 % bei 80 km, bei über 288 km ist der Aerosolterm exakt null. Bei z = 0 und
550 nm summieren sich beide **genau** auf σ₀ — die gemeldete Sichtweite bleibt unangetastet, gemessen:
T = 0,0200 auf 24,1 km horizontal, vorher wie nachher.

Zahlen am `p1`-Kamerastand: Gelände 13,9 km entfernt **0,274 → 0,853**, die Decke 9 km entfernt
**0,499 → 0,935**. Der Neuenburgersee steht wieder in der Lücke; die Luminanz-Struktur in den drei
Lückenausschnitten stieg um Faktor 2,4 / 6,8 / **26,5** (der letzte war vorher ein toter Wisch, σ = 0,34).

**Und die Kanaltrennung fiel als Nebenprodukt an.** Ein getrennt geführter molekularer Term kann sein
eigenes λ⁻⁴ tragen — die Koeffizienten stehen schon da, ihre Verhältnisse SIND das Gesetz. Am
`p1`-Standort transmittiert Rot 0,908 gegen Blau 0,730. Lücke 5.8 ist damit zu, aber mit einer Korrektur
ihrer eigenen Prämisse: die Extinktion rötet, das **Bild wird trotzdem blauer** mit der Entfernung, weil
der Kanal mit dem größten Transmittanzverlust am meisten von einer himmelblauen Einstreuung dazugewinnt,
die heller ist als das Gelände dahinter. Deshalb sind ferne Berge blau. Gegen ein Kontrollbinary, das
sich NUR im grau erzwungenen Kanal unterscheidet: 99,5–100 % der Pixel verschieden, max 35/255.

Kosten des zweiten Terms: **+0,050 ± 0,027 ms** auf den reinen Geländeframe, auf dem vollen 17-ms-Frame
nicht vom Rauschen zu trennen (|Δ| < 0,4 ms). Drei zusätzliche `exp` pro Fragment, mehr ist es nicht.

Der eigene Vorschlag der Vorrunde ist mitgezogen: `capture_cloud_proofs.sh` **bricht** bei jedem
`gpu_error` ab statt zu protokollieren, und `VERIFY=1` nimmt den Satz zweimal auf und scheitert an jedem
Frame, der sich bewegt hat — 12/12 byte-gleich. Beim Bauen dieser Sperre gleich der nächste Fund
derselben Sorte: `grep | grep -q` liefert unter `pipefail` das SIGPIPE des ersten greps, ein `if` liest
einen echten Fehler dann als „kein Fehler". Die erste Fassung der Abbruchprüfung ließ deshalb sieben
kaputte Frames durch. Prüfvorrichtungen brauchen ihre eigene Prüfvorrichtung.

## 2026-07-29 — Achtzehn Katalogzeilen flogen, keine konnte schießen: die grobe Feuerleitung

Der Befund kam aus O5 und war schärfer als er aussah: eine `f15c` mit vier AIM-120 erfasst bei 18,64 sm,
hält `eng_locked=1` über 28 Sekunden bis auf 8,8 sm herunter — und drückt nie. Ursache eine Ebene tiefer
als die Mission vermutete: `FBAirModule` komponierte **keine** Feuerleitung, `FBState::FireControl` wurde
für keine der achtzehn Zeilen je geschrieben, und alle drei Freigabetore von `FBPilot` lesen genau diesen
Block. Vier Zeilen waren `ACCEPTED` — als **Flugmodelle**. Kein Eintrag war ein Kämpfer.

`modules/air/FBAirFireControl` ist bewusst kleiner als die der F-16. Drei Produkte mit je einem
benannten Leser: die Startzone (Freigabesperre + Schusstor), die Zielschätzung (der Midcourse-Uplink —
**und damit die Bindung**), die Kanonenlösung. Weggelassen und begründet: die ganze Luft-Boden-Hälfte
(keine Katalogstufe akzeptiert `attack`, kein Katalogradar sieht den Boden), die Steuerpunkt-Entfernung
(ein Anzeigeprodukt für ein Cockpit, das eine Katalogzelle nicht hat). Der Trichter wurde von zwei
Entfernungen auf zwei **Flugzeiten** verallgemeinert — 600/3000 ft durch die 1030 m/s der M61A1 sind
0,178 s / 0,888 s, und dieselben 0,888 s ergeben an der GSh-301 764 m gegen deren belegte 800-m-Grenze.
Eine Zeile bekommt die Feuerleitung genau dann, wenn sie eine Waffe deklariert: zehn Decks ja, alle acht
Mover nein. Der Tanker hat keinen Rechner, und das ist in einer Spalte prüfbar.

**Ob eine Runde ihren Schützen bindet, ist eine Ladungs- und keine Zelleneigenschaft.** Dieselbe F-15C,
dieselbe Geometrie, vier `store`-Zeilen Unterschied: AIM-7 `ttaS = −1`, 39,2 s an das Ziel gefesselt, bis
zum Einschlag; AIM-120 `ttaS = +8,25 s`, nach 8,6 s frei. Beide töten (Fehlabstand 1,72 m / 0,905 m). Was
ein **Abbruch** der Führung kostet, zeigt dieselbe Mission, die den Defekt aufzeichnete: die MiG-23
schießt bei 20,78 km, verliert die Beleuchtung 16,9 s in einen 26,8-s-Flug hinein — die Runde kommt bei
nichts an. Und zwar aus einem belegten Grund: das ±6°-Elevationsfeld des N003E kann einen 1000 m höher
fliegenden Bomber im Anflug nicht halten.

Auf dem Weg dorthin ein zweiter Fund derselben Sorte wie `pilot.md` 2.15, nur in der anderen Achse:
`set brief_gci 90` ist eine **rechtweisende** Peilung und wurde als körperbezogener Azimut gepostet. Die
±30°-Antenne der MiG-23 stand damit 90° neben der eigenen Nase, und die Abfangmaschine kommandiert nur
Elevation — sie kam nie zurück. Vorher null Radarkontakte, nachher erster Kontakt bei 27,37 sm, an einer
unveränderten Missionsdatei. Ein gebriefter Anruf sind zwei Zahlen im Rahmen des LOTSEN, und beide müssen
gegen die eigenen Instrumente umgerechnet werden.

Was **nicht** geht, gemessen statt behauptet: `mig17` und `su7` — die zwei reinen Kanonenzeilen — haben
kein Radar, eine Kanonenlösung braucht eine Entfernung, und das Auge veröffentlicht eine Winkelgröße und
keine (A14; der Mechanismus heißt Kreiselvisier mit eingestellter Spannweite und ist benannt, nicht
gebaut). Und mit dem per Rezept-Schritt 8 eingetragenen, gemessenen Rollwerk feuert eine `mig21` zwar
ihre GSh-23L (vier Treffer, `structure degraded`) — danach greift sie nie wieder an, braucht 76 s für
3,0 → 0,9 sm und sinkt aus 5000 m in den Boden (A15). Eine Katalogzeile kann ihre Kanone **abfeuern** und
nicht mit ihr **kämpfen**. Keine Kampagne darf ein Kanonenduell werten.

## 2026-07-29 — Kampagne O2 gebaut und geflogen: der Lotse ist alles wert, wenn der Jet still startet

Zehn `sim/missions/o2-*.fbm` plus `sim/campaigns/o2-pvo-intercept.fbc`, vierte der zehn Kampagnen, kein
`sim/src/`, kein Werkzeug, kein Asset angefasst — 11 neue Dateien, 0 geänderte, also sind die 173
bestehenden Missionen byte-identisch **per Konstruktion**. Beide Determinismus-Kriterien beim ersten
Versuch: 9 Läufe ein Fingerabdruck (`93b5869298b6b8a5924…`, `--elev const`), 10/10 Schritte replayen
allein. Kampagnen-Exit 3, Schritt-Exits `3 0 3 3 3 0 0 0 3 3`.

**Die Schleife ist 11,0 s und sie teilt sich 8,0 + 3,0.** `gci BRAA` → drei getippte Eingaben →
`n019 EMISSION` = 8,0 s, und das reproduziert `mig29-intercept.fbm` auf einer anderen Geometrie, ist also
eine Eigenschaft der Eingabekette. Von dort bis zum festen Track: 3,0 s, ein RAD-Rahmen. Der Preis steht
auf dem anderen Jet: dessen RWR meldet `kind=fire-control` **0,1 s** nach der Emission.

**Und damit dreht sich der Befund dreier Vorkampagnen um.** O1 und O5 haben den Lotsen dreimal bei null
gemessen — weil in allen dreien `set n019_emission illum` beim Spawn stand: die gelöschte Peilung ließ
die Antenne **falsch gerichtet** zurück. O2 fliegt die dokumentierte Einschaltstellung `off`, und dort ist
die dritte getippte Eingabe das Einzige im Baum, das das Radar überhaupt anschaltet. Falscher
Azimut-Sektor: **0 Kontakte in 400 s**. Lotse gelöscht: 0 Emissionen, 0 Kontakte, und der Eindringling
erfährt nie, dass jemand da war. Später Einsatzbefehl: **45,3 s Schweigen gekauft, 77 % der
Erfassungsreichweite bezahlt**, null Schüsse gegen zwei Treffer. Die vergleichbare Größe über alle vier
Kampagnen ist nicht *"was der Lotse wert ist"*, sondern *"was er bei gegebener Emissionspolitik wert
ist"* — drei Dateien maßen die Richtung, diese misst die Existenz.

**Die Identifizierungs-Gegenprobe hält in der starken Form.** `o2-06` gegen `o2-08`, ein Token
Unterschied (`team neutral` → `team friendly`): **5 von 5 `telemetry*.csv` byte-identisch**, `events.log`
in **genau einer Zeile von 53** verschieden — `mission UNIT_RESULT … team=`, geschrieben vom Runner,
lesbar von keinem simulierten System. Vier Wahrnehmungskanäle liefen 300 s lang (N019 mit Abfrager, KOLS,
Auge, SPO-15), keiner bewegte sich. Es gibt keinen ersten Diskriminator, bis zu dem man identisch sein
könnte: IFF Mode 4 ist zweiwertig, ein Fremder und ein Feind sind dasselbe Schweigen. **Ein solches Paar
braucht eine dritte Datei** — `o2-07`, derselbe Anflug mit einem Kontakt, der ANTWORTET: zwei Logzeilen
Unterschied, null Telemetriebytes auf dem Abfangjäger. Ohne sie hätte "identisch" zwei mögliche Ursachen.

**Zwei CIA-Dokumente, seit Lauf 1 als „höchstwertige ungelesene Quelle des Verzeichnisses" geführt,
gelesen.** Der `cia.gov`-Pfad ist Akamai-geblockt (302 → *Access Denied*); die Wayback-Aufnahmen
derselben URLs sind es nicht. Mitronin (Warschauer-Pakt-Journal 12/1976) trägt die Kampagne: zwei Formen
der Zusammenarbeit, fünf Zuteilungswährungen, die **Identifizierung als das zentrale Problem** ("sonst
müsste die Feuerfähigkeit der Flaraketenverbände wegen der Gefahr, eigene Flugzeuge zu treffen,
eingeschränkt werden"), die Korridore für eigene Flugzeuge — und **10 bis 15 Minuten** (DRUŽBA-76), um
die eigene Luftlage über eine Codetabelle bis zum Richtschützen zu bringen. Damit hat `C6` eine Zahl:
FlightBox misst eine **Cockpit**-Schleife mit der Stoppuhr und einen Gefechtsstand mit gar nichts.

Drei Funde, keiner behoben. (1) **Ein falscher Brief hat keine absichtliche Korrektur** — was danach
aussieht, ist das 2,0°-Totband von `FBPilot`s eigenem Suchgesetz, das driftet; es rettete eine von zwei
Maschinen, 28 s zu spät, die andere schaute 400 s lang 7,5° über ihr Ziel. (2) **D3 als Byte-Differenz
bepreist**: `set kols_mode ir` über fünf Einsätze ändert **4 von 184 Telemetriespalten und sonst nichts**
— während genau dieser Sensor 90 s lang einen Kontakt hielt, den niemand las. (3) **Eine R-27R innerhalb
ihres eigenen Zünders ist kein Abschuss**: 4,85 m und 4,75 m in die Vorderzone ließen das Ziel
kampffähig, 2,48 m anderswo töteten. Eine Ergebnisachse nach Abschüssen liest Sprengkopfgeometrie.

Und der Elevations-Defekt aus `pilot.md` 2.15 wurde auf der **anderen** Seite seiner Schwelle vermessen:
der Fehler ist exakt `st.pitch`, das Steigflug-Nickband liegt bei **5,36…5,89°**, der RAD-Balken bei
±6,0° — Rand **0,11–0,64°**. Beißbedingung: `|pitch| + |Zielelevation im Körperrahmen| > 6,0°`.

## 2026-07-29 — Kampagne W5 gebaut und geflogen: die Aufgabe hing an einer Besetzungszeile, nicht am Auge

**W5 Baltic Air Policing / QRA** ist die fünfte gebaute Kampagne, die erste, in der die **F-16** fliegt,
und die einzige der zehn, deren Siegbedingung **keine Waffe** enthält. Zehn `sim/missions/w5-*.fbm` plus
`sim/campaigns/w5-baltic-qra.fbc`; **keine Datei unter `sim/src/`, `sim/tools/` oder `sim/assets/`
angefasst**, elf neue Dateien, null geänderte. Kampagnen-Exit 3, Schritt-Exits `0 0 0 0 0 0 0 0 0 3` —
die erste Kampagne, deren Einsätze überwiegend ein **echtes Urteil** liefern statt eines Messstands, weil
`identify` + `no_fire` (Runde `C12`) für genau diese Aufgabe gebaut wurden.

**Die eigene Überschrift der Spec war falsch, und die Messung sagt warum.** Sie führte W5 als „die
Kampagne, deren Gegenstand FlightBox nicht simulieren kann", weil es kein Auge gab. Das Auge existiert
seit dem 28.07. — und es war trotzdem nicht das Entscheidende. Entscheidend ist die **Spannweite des
Gegenstands**: eine An-26 wird mit dem Auge bei **1 086 m** identifiziert, eine Tu-95 bei **2 049 m**,
während zwei MiG-29 im selben Verband bei 1 600 m nicht einmal *erkannt* werden. Ein Auflösungsgesetz,
zwei veröffentlichte Maße. Der Blocker war eine **Katalogzeile** (`an26`, ein MOVER — und ein Mover hat
kein generiertes Deck, also greift `C7`s `ALPHA`-Urteil nicht), kein Sensor.

**Die Gegenprobe, dreiläufig von Anfang an geplant.** `w5-02` gegen `w5-03`, ein Token Unterschied:
**6 von 6 `telemetry*.csv` byte-identisch, 1 abweichende `events.log`-Zeile von 75** — das `team=`-Feld
des Runners. Der Kontrolllauf `w5-01` (der Gegenstand ANTWORTET) bewegt **5 von 184 Telemetriespalten
und null Meter**. Das ist ein **Widerspruch zu O2**, wo derselbe Versuch null Spalten bewegte, und
Regel 11 löst ihn auf: die F-16 fliegt im Verband, `FBFlightPicture` sortiert über Tracks mit
IFF-Feld, ein `friendly` antwortender Track wird nie zugewiesen. Gemessen wird also *„was eine Identität
einem VERBAND wert ist"*. Auf beiden Flugzeugen gilt: **kein Meter Flugweg bewegt sich.**

**Was eine Identifizierung kostet** (40-km-Nachlauf, 900 s): **412,9 s und 243,5 lb** bis zur visuellen
Identifizierung, **494,3 m** Annäherung, **null Risiko** — ein Nachflug wird in genau die Richtung
geflogen, in die ein vorwärtsblickendes Radar nicht zeigt: der Gegenstand strahlt ab t=0, seine Keule
erreicht den Abfangjäger erst bei t=201,3, **30 s NACH** dem gerasteten Urteil. Der Anflug vom Platz
steht nicht in der Rechnung (`C6`).

**Vier Funde, keiner behoben.** (1) **Im Spawn-Tick meldet jeder Radarwarnempfänger die WAHRE statt der
relativen Peilung** — die Lage, gegen die transformiert wird, ist noch nicht publiziert; isoliert
gemessen **180° gegen −95,5°** bei identischer Geometrie, Fehler **275,5°**, 2,0 s gehalten,
vorbestehend und im eingecheckten `pair-2v2-f16.fbm` sichtbar. (2) **`FBPilot` hat für diese Aufgabe
kein Verhalten**: `FBPilot.cpp:1040` nennt *innerhalb 5,0 nm und nie geschossen* einen ABBRUCH — also
genau die Identifizierungsgeometrie; mit `set task intercept` dreht der Abfangjäger bei t=5,1 s weg.
Jeder Anflug in allen zehn Dateien ist eine von Hand geschriebene Route. (3) **Ein Verband kann zwei
Ziele nicht sortieren, die jeweils nur ein Mitglied sieht** — `FBFlightPicture::Assign` matcht alle
Mitglieder gegen die Kontaktliste des *rechnenden* Flugzeugs, also melden beide Jets `dup=1` über zwei
Maschinen in 17,8 km Abstand. (4) **Die Führung krabbt nicht**: ein 176-km-Bein biegt **3,4 km** nach Lee
und verfehlt eine 2-km-Box um 4 038 m, wo dieselbe Maschine auf 34-km-Vektoren bei 677,7 m identifiziert.

**Und der akzeptierte Preis wurde öffentlich bezahlt.** `doc/missions/verdict.md` urteilt über die
GEOMETRIE statt über das Sensorereignis und nennt den Preis: „wer die Box mit geschlossenen Augen
fliegt, punktet". Der Nachteinsatz hat **0** `vis`-Zeilen gegen 9, unterscheidet sich in **6 von 184**
Spalten — und liefert `mission IDENTIFIED` beim **identischen Tick, Bereich und Verweilwert**. Genau der
Preis, kein Cent mehr.

Beide Determinismus-Kriterien beim ersten Versuch: 9 Läufe eine Kampagnen-Signatur
`49d3320f5e9761db2f1df85a12d9008e0d8559395c141c31e2e06903b9fe0200`, 10/10 Schritte standalone
bit-identisch — **und der `replay` lief diesmal nach der ERSTEN Mission**, auf einer
Wegwerf-Ein-Schritt-`.fbc`, was zwei Vorrunden eingestanden hatten zu versäumen.

## 2026-07-29 — Kampagne W3 gebaut und geflogen: der Wert eines Hebels ist eine Eigenschaft der Topologie

Zehn `sim/missions/w3-*.fbm` plus `sim/campaigns/w3-desert-storm.fbc` — die sechste der zehn Kampagnen,
und die erste, deren Gegner ein **System** ist statt eines Flugzeugs. **Keine Datei unter `sim/src/`,
`sim/tools/` oder `sim/assets/` angefasst**: elf neue unversionierte Dateien, null geänderte, also sind
die 195 bestehenden Missionen bit-identisch per Konstruktion.

**Die eigene Schlagzeile der Spec ist widerlegt.** Sie sagte: *„Von den drei Dingen, die an Package Q
schiefgingen, kann FlightBox heute NULL messen."* Regel 7 — jeder Blocker gegen den **Baum** geprüft,
nicht gegen eine Statuszeile: `C1` `C8` `C22` `C23` `C24` `C26` `C27` `C2` `C0` sind seither geschlossen.
**Zehn von zehn Missionen liefen**, und von den drei Fehlerarten kann FlightBox **genau eine** stellen —
die Kampagne sagt welche und warum. Fehlerart 2 (die Weasels gingen früh) ist vollständig stellbar.
Fehlerart 1 (Tanker/Sprit) ist **doppelt** blockiert: `C5` blockiert die Ursache, und darunter blockiert
ein unerreichbarer Zweig die Wirkung. Fehlerart 3 (der gesättigte Funkkanal) ist gar nicht stellbar, weil
es keinen Kanal mit Kapazität gibt (`C18`); Einsatz 09 sagt das und misst stattdessen das einzige
sättigbare Kommandoobjekt im Baum.

**Was ein Unterdrückungselement wert ist — vier Läufe auf einer Geometrie.** Drängt es auf 20 km vor,
trifft die AGM-88 die S-125 auf **2,8 Millimeter** und alle Bomber kommen durch. Schießt es aus 42 km,
fällt die Runde **10,5 km zu kurz** — und die Batterie feuert trotzdem ihr ganzes Magazin auf die
**abdrehenden** Weasels, also kommen die Bomber ebenfalls durch. Erst ohne SEAD (Zuordnungslauf A2)
kippt es: vier V-601 zwischen 5,86 und 7,99 m auf einen Bomber, **1 von 2** erreicht den Auslösepunkt,
das Ziel bleibt stehen. **Das Element ist eine Auslösung und das Ziel wert — auch dann, wenn seine
Rakete 10 km zu kurz fällt**, denn eine FlightBox-Batterie hat keinen Freund-Feind-Abfrager und keine
Bedrohungsrangfolge und verteilt ein endliches Magazin auf das, was in Reichweite ist.

**Was Emissionsdisziplin wert ist.** Bei **57,4 %** der 52,08 s Flugzeit dunkel: die AGM-88 rollt aus und
schlägt **214 m** vor der Stellung ein, die Stellung lebt. Sie kommt bei t = 200 zurück und verschießt
ihr Magazin auf 25,7–34,3 km — wieder auf die abdrehenden Weasels, null Ankünfte. **Sie ist die Stellung
wert und sonst nichts**: wer einer HARM ausweicht, hat sich selbst 170 s lang unterdrückt.

**Und derselbe Hebel, zweimal gezogen, ist zweimal etwas anderes wert.** Eine Mk 84 auf denselben
Frühwarnradar: gegen ein Netz mit EINEM Knoten die ganze Operation (0 Starts gegen 10; 2 von 2 Bomber am
Auslösepunkt gegen 0 von 2), gegen ein Netz mit einem zweiten Knoten **9 von 25 Einweisungen und sonst
nichts** — dieselbe Datei als Kampagnenschritt und standalone, 30 von 58 Telemetriedateien bit-identisch,
keine einzige Bahnspalte bewegt. Das ist Regel 11 eine Schicht tiefer und die direkte Einschränkung von
O5s *„eine Mk 84 auf die P-18 kostet die Flugkörperschicht zwei Nächte"*: **O5s Platz hatte einen Knoten.**

**Regel 11 diesmal auf beiden Seiten gemessen.** `w3-07`/`w3-08` unter der vorab erklärten Politik
`n019_emission off`: **50 rote Radarkontakte und 5 Startlösungen gegen 0 und 0**. Zuordnungslauf A3,
dieselbe Datei mit `illum`: 50 Kontakte, 7 Lösungen, Lauf endet beim **identischen** t = 272,8 s wie die
gebriefte Kontrolle. Der Fünf-Theater-Widerspruch ist damit gemessen statt geerbt.

**Vier Funde, keiner behoben.** (1) **`FBPilot::CanPressOn` ist unerreichbar** — die einzige Zeile im
Piloten, die die BINGO-Warnung liest, hängt an `EngState_ == Defend && elapsed >= DefendHoldS`, und der
allgemeine Zweig `else if (EngState_ != Abort)` nimmt den Zustand im ersten Takt weg, nachdem `defendDue`
fällt. Gemessen: das Bit über **5 200 von 5 200** Zeilen gesetzt, `eng_state` bit-identisch zum Lauf ohne
Brief, sieben von 184 Spalten Unterschied und null Meter. (2) **Ein Näherungszünder hat keinen
Team-Test** — bei 24 Flugzeugen detonierte `qamia1`s R-27R **11,74 m** neben einer MiG-29 der anderen
roten Rotte und tötete sie; 1 von 3 Verlusten des Schlusseinsatzes ist eigenes Feuer. (3) **`C15` hat
jetzt einen Preis**: drei von vier AGM-88 einer Viererrotte gingen in dieselbe Batterie, zwei davon,
nachdem sie schon tot war. (4) **Eine Batterie hat keine Bedrohungsrangfolge** — viermal gemessen, in
drei verschiedene Richtungen.

**Und die Zahl, die eine Schlagkampagne wirklich braucht:** ein Bomber wird in diesem Baum häufiger von
**Systemschaden** gestoppt als von Zerstörung. `w3-02`s zweite Welle nimmt vier V-601 innerhalb 8,4 m,
überlebt alle vier und steht 56 s später über ihrem Ziel mit elf ausgefallenen Systemen, `stores`
darunter. Als Verlustliste gelesen: 0–0. Als Paketergebnis gelesen: Totalausfall.

Beide Determinismus-Kriterien beim ersten Versuch: 9 Läufe eine Kampagnen-Signatur
`3490c4fab3f25f533ead565e393cc23d234067e827e5ea7ba733408988f1fa1a`, 10/10 Schritte standalone
bit-identisch — und der `replay` lief nach der **ersten** Mission, auf einer Wegwerf-Ein-Schritt-`.fbc`.
Der Schlusseinsatz fliegt **24 Flugzeuge + 8 Bodenobjekte + 34 Waffen** in 11,7 s Wanduhr, **8 von 8**
Bombern am Auslösepunkt und **15 von 16** zurück.

## 2026-07-29 — Kampagne W4 gebaut und geflogen: die Höhenuntergrenze steht über der Decke ihrer eigenen Waffe

Siebte von zehn, zehn `.fbm` + eine `.fbc`, nichts unter `sim/src/`, `sim/tools/` oder `sim/assets/`
angefasst (`git status --porcelain`: elf neue, null geänderte Dateien), also sind die 205 vorhandenen
Missionen bauartbedingt byte-identisch. Die Spec nannte **vier** von zehn flugfähig und vier Missionen
auf `C1` blockiert; nach Regel 7 gegen den heutigen Baum geprüft liefen **zehn von zehn**. Zwei
Spec-Missionen wurden nicht blockiert, sondern **verworfen** — das Gebirgstal (`C4` + `--elev const`:
es gibt kein Tal) und der Wetterabbruch (`FBPilot` hat keinen Zweig, der die Auslösung verweigert, dieselbe
Form, die W3 an der BINGO-Warnung gemessen hat) — beides mit Begründung im `.fbc`-Kopf.

**Der zentrale Fund ist eine Kollision zweier Ankertatsachen.** Beide sind belegt: NATO flog mit einer
harten Untergrenze von 15 000 ft, und die F-16CJ trug die AGM-88. Auf einer Neun-Punkte-Leiter (ein
`p18`, eine Runde, 20,0 km, Abschusshöhe die einzige Variable) trifft die Runde von 3 000 bis 4 150 m
(0,009–4,47 m), verfehlt bei **4 200 m um 74,8 m** und bei **4 572 m um 2 484 m** — und der letzte
FRISCHE Blick jedes fehlschlagenden Schusses liegt bei exakt **15,00°**, gemessen an der Stellung: der
publizierten Elevationsabdeckung des P-18 (`SearchElCenterDeg 5 + SearchElHalfDeg 10`). 15 000 ft =
4 572 m. **Die Untergrenze liegt 372–422 m über der Decke ihrer eigenen SEAD-Waffe**, also fliegt jeder
Weasel dieser Kampagne auf 3 000 m und schreibt es in seinen Kopf.

**Was ein Verteidiger gewinnt, der grundsätzlich nicht strahlt — eine Geometrie, ein Hebel, drei
Stellungen.** Knoten strahlt: nach 66,5 s tot. Alles auf `emcon hold`:
`site RADIATE`/`TRACK`/`LAUNCH`/`net CUE` **3/2/4/4 → 0/0/0/0**, Stellungsverluste 1 → 0. Knoten plus
drei `p18`-Attrappen: beide AGM-88 sterben auf einer Attrappe (0,019 m und 4,75 m), der Knoten lebt,
`net CUE` **26 gegen 4**, der Gürtel verschießt **7 statt 4** Runden. **Und keine der drei Politiken
bewegt den Angriff** — dieselben zwei Bomber, dieselben Takte, dasselbe `aimErrM`. Die Doktrin ist die
Stellung wert und sonst nichts; die Attrappe kauft dieselbe Überlebensfähigkeit und behält das Gefecht.

**Der Radarköder funktioniert und kostet nichts.** `cast.md` veranschlagte ihn als *"`p18` mit
`rounds 0` und kleinem Tor"* — beides falsch: ein `p18` hat ohnehin `Channels 0`, ein Suchreichweiten-Key
existiert nicht, und was ihn wirken lässt, ist gerade, dass er dieselbe Zeile ist wie der Knoten
(`1 − (r/2R)²`, also gewinnt schlicht der nächste). Zwei Grenzen sind gemessen: die `arm_class`-Sortierung
ist binär, und unterhalb von `Höhe/tan(15°)` — **17,1 km von der Untergrenze aus** — ist eine Attrappe
überhaupt nicht hörbar.

**Vom Schlechtwetter ist der WIND gemessen, die Decke ist Kulisse.** Sechs-Punkte-Leiter auf einer Datei:
**5,014 m Bombenfehler pro Knoten** auf 4 572 m; die 46 kt der Fixture kosten 216 m, und jeder
`wx fixture`-Angriff dieser Kampagne verfehlt; 20 kt Seitenwind machen aus **3 von 6** Treffern **0 von
6**. Die Wolke wird genau **einmal** gemessen, am Auge: `vis MASKED … transmittance=8,00571e-13`.
`irst_masked` ist in allen zehn Dateien 0, und eine reine Angriffsdatei protokolliert überhaupt keine
`vis`-Zeile.

**Drei Funde, keiner behoben.** (1) Eine halbaktive Batterie, die einen **Schienen**-Nachladevorgang
beginnt, verwaist jede Runde in der Luft — beim dritten Start geht die 2K12 in `RELOAD`, der Beleuchter
schweigt, 0,2 s später melden alle drei 3M9 `ILLUMINATION_LOST`, Runde 2 **1 776,6 m vor dem Ziel nach
27,1 s Flug**; mit einem `set rounds 4`-Kontrolllauf zugeordnet, der beim identischen Takt verliert. (2)
`objective suppress … emitting <s>` liest **MET mit `emittingS=0`** — es unterscheidet nicht, ob wir sie
niedergehalten haben oder ob sie nie an war. (3) Der „erstes zulässiges Symbol"-Rastvorgang der
Antiradarwaffe hat kein Gedächtnis für ihr Startziel: gegen einen verteilten Gürtel rastete sie
**sechsmal in 11 s** auf vier Symbole, das letzte 36,7° neben der Nase.

Beide Determinismus-Kriterien beim ersten Versuch: 9 Läufe eine Kampagnen-Signatur
`6185addc27ec3ef896cd1aed4750d7a6bdf8555f9a3a1e2c6b12971533b8d80a`, 10/10 Schritte standalone
bit-identisch — und der `replay` lief nach der **ersten** Mission. Der Übertrag ist eine Rufkennung
(`kosnod`, 08 → 10) und **25 % des Meldeverkehrs** wert (60 gegen 80 `net CUE`), sonst nichts: 21 von 41
Telemetriedateien byte-identisch, die übrigen 2–6 von 184–202 Spalten, alle sieben RWR- oder
Datenlink-Buchhaltung, **keine Bahnspalte bewegt sich**. Und `--elev tiles` über das echte Kosovo
(Boden 547,88 m) verschiebt `predErrM` 58,08 → 46,50 m und erzeugt **null Maskierungen** — die fehlende
Hälfte von `C4` ist eine Rechnung, keine Datenlage.

## 2026-07-30 — Der Direktor der MiG-29: ungenauer als der Rechner, und genau das war der Nachweis

`C9` war die einzige Lücke, die eine ganze Kampagne auf null setzte, und sie hieß nicht „CCIP fehlt"
sondern etwas Genaueres: das Abwurfverfahren der MiG-29 ist ein **Direktor** und kein Auslösecue —
das Flugzeug wählt den Moment, der Pilot fliegt eine Anweisung. Gebaut als `core/FBDirector.h` plus
`modules/mig29/FBMig29Director.*`.

Die Abnahme war deshalb nicht „es wirft ab", sondern die eine Messung, die eine Abkürzung entlarven
kann: **dieselbe Geometrie, dieselbe `fab500` auf beiden Seiten** (damit die Ballistik keine Variable
ist), F-16 im Rechnerverfahren **34,02 m** gegen MiG-29 im Direktor **65,65 m** — Faktor 1,93. Ein
Direktor, der *besser* abschneidet als ein Rechner, wäre das Verfahren der F-16 mit kyrillischen
Beschriftungen gewesen. Er tut es nicht.

Die Verweigerung ist ein eigener, belegter Fall und keine geschriebene Regel: der Abzug darf erst 1–10 s
nach der Entfernungsmessung gedrückt werden, der Ablauf danach muss lang genug sein, um den Abwurf zu
fliegen — also muss die Messung mehrere Kilometer vor dem Abwurfpunkt liegen, und der einzige
Entfernungsmesser dieses Flugzeugs reicht 6 km. Ein Anflug, der dafür keinen Raum lässt, wird
abgewiesen: `mig29-opt-refused.fbm` fliegt den Grenzfall (löst aus, 102,9 m) und den verweigerten Fall
(löst **gar nicht** aus) in einer Datei.

Byte-Identität hält: eine MiG-29 ohne Angriffsauftrag bewegt sich nicht, Sammelhash über zwölf
bestehende Missionen gleich, Spaltenzahl unverändert bei 184. `verify-layers` 304 Dateien, sechs
Registry-Leser, eine Antennenbefehls-Stelle.

Zwei Ehrlichkeiten zum Zustand: die drei Beweismissionen brauchen `--elev const`, weil sie auf 300 m
spawnen und das Schweizer Geländemodell dort 492 m hoch ist — die Runde starb zweimal an Serverfehlern,
bevor sie das prüfen konnte, und es ist beim Nachfahren aufgefallen. Und O3 ist damit **am Modul** nicht
mehr blockiert, wohl aber an ihrer Besetzung: die Zeitgenossen sind ALPHA und dürfen keine
Kampagnenfrage beantworten. Die Zahl der lauffähigen O3-Missionen ist bewusst **nicht** neu gezählt
worden — das ist die erste Aufgabe der O3-Runde selbst.

## 2026-07-30 — Kampagne O3: ein befreundeter Flugkörperschirm ist keine Deckung, er kostet ein Flugzeug

O3 war die einzige der zehn Kampagnen mit **null** lauffähigen Missionen — blockiert nicht an einer
Mission, sondern am Modul (`C9`). Mit dem Direktor von heute Morgen ist sie gebaut: zehn `.fbm`, eine
`.fbc`, beide Determinismus-Kriterien im ersten Versuch (**9 Läufe, ein Fingerabdruck**
`01e4f956…`; **10/10** Schritte reproduzieren standalone), Replay nach der **ersten** Mission, kein
Byte unter `sim/src/` angefasst. Die Zahl lauffähiger O3-Missionen ist damit **10 von 10**, und alle
zehn sind auch beantwortbar.

**Die Frage, die keine andere der zehn stellen kann, hat eine Zahl.** Der Schirm über dem Kanalübergang
gehört *uns*. Über zehn Einsätze: **28 Boden-Luft-Starts, 28 von 28 auf die eigenen Flugzeuge gerichtet**
— zugeordnet über jede `sms LAUNCH_SOLUTION`-Zielkoordinate gegen die Telemetrie beider Seiten, 22–95 m
zur gemeinten MiG-29 und 1,1–10,8 km zum Gegner —, **null** je auf einen Gegner, **null** Gegner je in
einer festen Spur, **eine eigene MiG-29 abgeschossen** (3M9 auf 4,74 m gegen 8 m Zünderradius) und **ein
gegnerischer F-16 mit zehn ausgefallenen Systemen** — getroffen von einer V-601, die auf eine MiG-29
geschossen wurde. Der Mechanismus sind zwei nachprüfbare Sätze: `FBSiteFireControl` enthält überhaupt
keinen IFF-Pfad, und jede SAM-Zeile hat `Channels = 1`. Ein Freund im Bereich **verschlechtert** die
Bekämpfung nicht, er **löscht** sie: die 44 Entscheidungszeilen von `o3-06` sind byte-identisch mit denen
von `o3-04`, das keinen Gegner enthält.

Regel 11 beidseitig geflogen: `wcs hold` (o3-05, ein Token gegen o3-04) kostet **nichts** und behält das
Magazin — und gibt die Fähigkeit auf, überhaupt jemanden zu bekämpfen. Und die zweite Hälfte derselben
Regel: „der Schirm ist harmlos" ist nur um **1,1–5,2 m** wahr. Eine Leiter über 300/1 000/3 000/5 000 m
misst `closestM` 9,07 m gegen 8 m Zünder und 11,4–15,2 m gegen 10 m — dieselbe Batterie, die 27-mal
vorbeischoss, tötete beim 28. Mal, und die Entscheidung darüber lag bei 500 kg unabgeworfener Bombe.

**Der zweite Befund ist nicht der Schirm, sondern das Flugzeug: dieses Muster kann die Operation seines
eigenen Ankers nicht fliegen.** Querabweichung gegen 68,4 m Wirkradius: **+48,3 m** auf 6 km geradem
Endanflug, +87,2 m auf 12 km, +90,9 m auf 24 km — und **48,3 m + 33,9 m je Grad Knick** [abgeleitet,
Drei-Punkt-Leiter]. Größter zulässiger Kurswechsel im Endanflug: **0,59°**. Der Eröffnungsschlag des
6. Oktober waren 220 Flugzeuge auf koordinierten Anflugwegen; so etwas trifft hier nichts. Der Direktor
selbst wird dabei *besser* (`openLoopAlongM` −70,3 → −1,0 m) — es gewinnt die Achse, die gegen einen
Wirkradius gemessen wird.

Drei Funde, keiner behoben: (1) eine bodengestartete kommandogelenkte Runde verfehlt einen tief
fliegenden, nicht manövrierenden Querflieger um **knapp mehr als ihren eigenen Zünderradius** — dritte
sichtbare Schicht der Bodenstart-Familie aus O1, jetzt mit Zahlen auf **beiden** Seiten der Schwelle;
(2) **eine unabgeworfene Bombe auf einer Innenstation kehrt den stehenden Querversatz des Flugzeugs um**,
+48,3 → −39,2 m, ein Schwung von 87,5 m gegen 68,4 m Wirkradius — deshalb ist dieser `carry` der
wirksamste der acht gebauten Kampagnen (**0 von 48** Telemetriedateien byte-identisch, gegen W3s 30 von
58) und **eine unabgeworfene Bombe ist ein Flugzeug wert**; (3) der FAB-Kommentar in `core/FBStore.h`
behauptet weiterhin, die MiG-29 könne `set task attack` nicht fliegen — dreißig Abwürfe später.

Und eine Ehrlichkeit zum Gelände, die gestern zwei Missionen stilllegte und heute Voraussetzung war:
alle O3-Angreifer spawnen auf 300 m, weil ein 6-km-Entfernungsmesser die Wurfhöhe dieses Flugzeugs auf
2,0–2,2 km deckelt. `fb-gym`s **eigener** Standard ist `--elev swiss`, und der prüft eine explizite
Spawnhöhe gegen den aufgelösten Boden. `--elev const` ist für diese Kampagne keine
Vergleichbarkeitskonvention, sondern Bedingung — und steht in jedem Befehl ihres Protokolls.

## 2026-07-30 — Kampagne W1: die Übungsleiter steigt, aber nur ihre Bodenhälfte lässt sich noch benoten

W1 galt als blockiert, weil die Nellis-Aggressoren im Katalog **ALPHA** sind und `A15` kein
Katalog-Kanonengefecht wertet. W1s eigene Pointe löst das: in Nellis ist die „MiG-29" ein verkleideter
F-16 — bei uns ist sie das echte Modul. **Keine der elf Dateien fliegt eine Katalogzeile**, `A15` bleibt
unberührt, und die Richtung der Ersetzung steht in jedem Kopf: dieser Aggressor hat R-73, KOLS und
Helmvisier, die der echte nie hatte, ist also **stärker** als die Vorlage. Zehn `.fbm`, eine `.fbc`, beide
Determinismus-Kriterien im ersten Versuch (**9 Läufe, ein Fingerabdruck** `5de43dd5…`; **10/10** Schritte
reproduzieren standalone), Replay nach der **ersten** Mission, kein Byte unter `sim/src/` angefasst.

**Der seit Lauf 1 mit HTTP 403 vermerkte Faktenzettel ist gelesen** — über die Wayback-Kopie derselben
URL, dieselbe Lehre wie bei O2s zwei CIA-Dokumenten, jetzt auf einem zweiten Host bestätigt. Sechs
Aussagen steigen auf **[T1]**, darunter die Zehn-Missionen-Begründung im Wortlaut: die Missionszahl dieser
Kampagne ist damit die Zahl des Ankers und keine `[SET]`-Wahl. Paketgrößen stehen nicht darin — nur
Summen seit 1975 — und bleiben `[SET]`.

**Die eine Messung, die W1 den anderen neun voraushat, ist ein Fehlschlag mit drei Zahlen.** Gegen das
Sättigungstor aus `doctrine-evolution.md` §4.2, mit den neun deklarierten Doktrinhebeln auf jeder der
zehn Sprossen und auf beiden Sitzen (180 Läufe, zweimal, byte-identisch): `S4` 10 Geometrien ≥ 6 **ok**,
`S6` sauber, `S3` n/a — aber `S5` **2 informative gegen 3 gefordert: VERWEIGERT**. Modale Ergebnisklasse
je Sprosse 100 / 66,7 / 55,6 / 88,9 / 100 / 88,9 / 88,9 / 88,9 / 88,9 / 55,6 %, Hebel, die die Klasse
bewegen, 0/3/4/1/0/1/1/1/1/5 von 9 auf dem F-16-Sitz und 0/1/1/3/0/4/0/2/4/2 auf dem MiG-Sitz. **Die Hebel
beißen in gegenläufigen Sitzen auf gegenläufigen Sprossen** — das ist die Asymmetrie aus `duels.md` auf
handgeschriebenen Geometrien. Und die drei Sprossen, die überhaupt etwas entscheiden, sind genau die drei
mit höchstens zwei Flugzeugen: **ein Luftkampfergebnis über 2v2 ist in diesem Baum ein Fixpunkt**, die
Bodenhälfte entscheidet auf jeder Größe. Damit sind die 4v4-Höhepunkte der acht früheren Kampagnen
ergebnisblind gebaut, und das ist die übertragbare Zeile dieses Laufs.

Was die Leiter sonst gemessen hat: der Abzug fällt auf **0,978 × Rtr** (F-16) und **0,977 × Rtr** (MiG)
und ist **blind gegen ein 1,26-fach längeres Raero** — `duels.md` Zeile 1 auf zwei Dezimalen reproduziert,
auf einer Bahn, die kürzer ist als die eigene Radarreichweite (93,9 km gegen 100,0 km Gate, also gar keine
Suchphase); die CAP der Luftverteidigungssprosse **verhindert nichts** — 11 von 184 Telemetriespalten,
**null Bahnzellen**, und der Kontrolllauf ohne CAP wiederholt den 101,05-m-Fehlwurf auf fünf Dezimalen;
totale Funkstille kostet Blau jeden Schuss und Rot **10 Starts bei 0 Treffern**; ein Flügelmann ohne
eigenes Radar bekommt **gar kein Ziel zugewiesen**; der 18-Sekunden-Schussvorsprung der Aggressoren macht
aus **vier blauen Abzügen einen**; und der Übertrag — Kill-Removal, die Verfahrensweise des Ankers selbst,
diesmal also andersherum als bei O4 — ist **ein F-16 wert**, sauber attribuiert (`units` allein: ein
Verlust, `stores` allein: ein Verlust, keins von beiden: zwei).

Drei Befunde, keiner hier behoben. Der teuerste ist der billigste zum Hineinlaufen: **eine Kampfphase kann
durch einen fehlenden Navigationswegpunkt lautlos entwaffnet sein.** `FBF16FireControl` verwirft den ganzen
Block, wenn `state.Nav` unlesbar ist, und `FBNavSystem` publiziert ohne Steuerpunkt nichts — also hat ein
`set task bfm`-Jet ohne `wp` keine Kanonenlösung, keine DLZ und kein Raketentor. Gemessen am ersten
Zuschnitt dieser Kampagne: `blk_firecontrol` 0 über 3 001 Zeilen, 0 Schüsse, Exit 3 nach **14,8 s
ununterbrochenem Lock von 7,5 km bis 185 m** — eine `wp`-Zeile je Jet, sonst nichts, und der Lauf endet
0 bei t = 1,0 s. **Vorbestehend:** die Schützen von `bfm-basic`, `bfm-merge`, `bfm-offset` und `bfm-blind`
deklarieren alle keinen `wp`, `gun-bfm` und `gun-turning` — die beiden, die schießen — beide.

## 2026-07-30 — Kampagne W2 Osirak: die zehnte, und ihr Ergebnis ist eine Subtraktion

Die letzte der zehn. Zuvor aber die Nachprüfung, die die vorige Runde ausdrücklich offen gebucht hatte:
**beide Determinismus-Kriterien auf allen neun gebauten Kampagnen**, unter der Zweig-Umkehrung von
`b433950`. **81 Kampagnenläufe, 90 Einzelnachspiele, null Abweichungen**; jeder neue Fingerabdruck steht
jetzt im jeweiligen `## State` **neben** dem alten, der mit Datum stehen bleibt.

Und die Nachprüfung widerlegt eine Zeile der vorigen Runde. Sie hatte behauptet, die Schrittmuster aller
neun stimmten weiter; **zwei stimmen nicht**: `o3-07-top-cover` geht von Exit 1 auf 3 und
`w4-10-allied-force` von 3 auf 2. Beides war eine Ebene tiefer längst hergeleitet (`pilot.md` §7.4b, Zeile
für Zeile), nur nirgends in den Kampagnen gebucht. Jetzt steht es dort, mit Ursache. **Acht von neun
Fingerabdrücken haben sich bewegt, W5s nicht — byte-identisch** — und der Grund ist W5s eigene publizierte
Eigenschaft: null verschossene Waffen über zehn Einsätze, also kein Jet, der je in `Defend` geht, den
Zustand also, den der umgestellte Zweig besitzt. Dritter Befund: **Kampagnen-Fingerabdruck und
Missions-Regression messen nicht denselben Lauf.** `pilot.md` nennt fünf W1-Dateien als Bewegte, der
Fingerabdruck bewegt **acht von zehn** — `fb_regress.sh` fährt jede Mission standalone und **ohne Uhr**,
neun der zehn W1-Dateien deklarieren keine `time`, und die Kampagnenuhr allein bewegt 2 bzw. 7 Spalten
(`blk_env`, `vis_*`) und **null Bahnspalten**. Eine Missionsliste aus dem einen Instrument sagt über das
andere nichts.

**Dann W2.** Die Kampagne, deren eigene Spezifikation über sie schrieb, sie sei *„die, von der FlightBox
am weitesten entfernt ist"*, und deren erste Lieferung *„keine Missionsdatei, sondern ein
Zusatztank-Eintrag und ein Betankungsausleger"* sei. Die Hälfte davon ist am Vortag gelandet, und die
Kampagne fliegt: zehn `.fbm`, eine `.fbc`, Schritt-Exits `3 0 0 2 0 1 0 3 0 1`, beide Kriterien im ersten
Versuch (**9 Läufe, ein Fingerabdruck** `bdf58c2e…`; **10/10** Schritte reproduzieren standalone), kein
Byte unter `sim/src/` angefasst. Vier der zehn galten als baubar, zehn liefen, zehn antworteten.

**Das zentrale Ergebnis ist negativ und es ist eine Subtraktion.** Fünf Konfigurationen, ein und dieselbe
Strecke auf 240 m bei 400 kt bis zum Verlöschen: sauber **1 492,6 km**, mit zwei Mk-84 **1 162,3 km**, mit
zwei Tanks **2 173,4 km**, mit abgeworfenen leeren Tanks **2 327,9 km**, mit der vollen Kriegslast
**1 748,8 km**. Halbiert, ohne jede Reserve, ergibt das einen Kampfradius von **874,4 km gegen die 982,9 km,
die der Anker je Richtung braucht — 108,5 km zu wenig, 11,0 %**, und zwar bei Luftstart ohne Rollen,
Starten und Steigen, ohne Reserve, ohne Gefechtszuschlag und auf gerader Linie statt auf dem Dogleg, das
der Verband wirklich flog. Der Einsatz ist in diesem Baum nicht fliegbar, und das Loch hat exakt die Größe
der ungebauten Hälfte von `C5`.

**Der größte Hebel ist nicht der, nach dem die Kampagne gebaut wurde.** Die Tanks bringen +45,6 % sauber
und +50,5 % unter Kriegslast; sie fallenzulassen, wenn sie leer sind, noch einmal 7,1 % — und die Außentanks
sind nach **675,8 km** trocken gegen die *„etwa 1 000 km"* des Ankers, also dieselbe Größenordnung aus
völlig unabhängiger Richtung. Aber **die Anflughöhe allein kostet 43,2 % der Reichweite** (1 492,6 gegen
2 627,4 km auf 8 000 m). Die eigene taktische Entscheidung des Einsatzes ist das Teuerste an ihm, und keine
seiner Quellen sagt das.

**Der strittige Wert wurde in beiden Hälften geflogen, nicht gemittelt.** 30 m gegen 240 m, Faktor acht.
Über der Ebene der Kampagne hält die Lenkung 240 m auf **0,85 m über 300 km** und 30 m ebenso — und der
30-m-Fall ist dort kein Geländefolgeproblem, sondern ein **Zünderproblem**: `armMarginS` **0,486 s** von
2,0 s Schärfzeit. Über dem Boden, den der Einsatz wirklich überflog, ist **keine der beiden Höhen
fliegbar**: unter `--elev tiles` scheitert die Mission vor dem ersten Takt
(`spawn altitude is below ground, altM=240 groundM=487.48`), und die Strecke erreicht **1 599,22 m**.

Die Bodenhälfte ist eine Herleitung, die aufgeht: `target_hard` fällt innerhalb **17,7 m** einer Mk-84
(`2,81e7/r²` gegen 9,0e4 J/m²). Über siebzehn Abwürfe liegt `aimErrM` in einem Band von **6,36 bis
50,83 m**, zu 96–99 % längs, und `predErrM` ist Bodengeschwindigkeit mal konstant **0,228–0,241 s**, also
eine Latenz. Die Kuppel ist damit mit einer **Rate** zu töten: der Schlussangriff legt **fünf von acht**
Bomben innerhalb 17,7 m und die Kuppel fällt, bei acht von acht zurückgekehrten Angreifern. Die vier Pärchen
lösen auf **290,8 / 295,8 / 300,8 / 305,9 s** aus — die vom Autor gerechneten 1 029 m Abstand ergeben
**5,00 s, viermal, auf den Takt**; genau das heißt `C15`.

Die Mindestsprit-Entscheidung, tags zuvor erreichbar gemacht, ist jetzt in ihrer schärfsten Form gemessen:
`BINGO_ABORT … from=closing haveTgt=1` — der Pilot bricht **aus dem Anflug auf ein Ziel** ab. Seine
Kontrolle eine Zeile daneben schießt und fliegt heim; der Abbrecher endet **199,2 km von seinem eigenen Heimatwegpunkt**, 87,1 km jenseits des Ziels,
das es gerade verließ und spart über das Fenster nicht einmal Sprit.

Vier Befunde, keiner behoben: ein Abwurf an einem bombentragenden Jet wirft **die Bombe** (Stationsordnung,
`station=3 mk84` vor `TANK_JETTISON station=4`), sodass der selektive Abwurf des Ankers unausdrückbar ist;
die Zielerfassung einer Kanone hält **eine fallende Bombe für ein Flugzeug** (`rangeM=1250 closureMs=0
altM=111.256`); ein Frühwarnknoten weist eine Feuereinheit auf ein Ziel ein, das er selbst 200 km außerhalb
ihrer Reichweite misst; und die Angriffsphase löst **einmal je Anflug** aus, womit aus den sechzehn Bomben
des Ankers acht werden.

Und der Riss, der nur dieser Kampagne gehört: **die Übertragsschicht trägt genau das nicht, worum es hier
geht.** `campaign.md` verweigert Sprit als übertragene Tatsache, mit gutem und genanntem Grund — die Folge
bleibt, dass die eine Kampagne, deren Gegner der Sprit ist, von der Schicht über ihren Missionen blind
gesehen wird. Was der Übertrag kann, hat er scharf gezeigt: eine `action=drop`-Zeile, und **das Streichen
des Begleiters, der gestorben wäre, tötet den, der überlebt hätte** (standalone 1 von 2, in der Kampagne
0 von 1) — bei **8 von 29** byte-identischen Telemetriedateien, und die acht sind genau die acht Bomben.

## 2026-07-30 — Doktrin-Evolution `E5`: die Kampagnenbreite als Arena, und das Tor verweigert sie

**Schritt 5 des Eigner-Ziels, und sein Ergebnis ist eine begründete Verweigerung mit Zahlen.** Gebaut
wurde das Instrument, das `w1-red-flag.md` als fehlend benannt hatte: `tools/fb_campaign_arena.py`
spleißt ein Genom in eine **Kopie** einer committeten Mission, fliegt sie, liest sie im Worker und
löscht sie wieder. Eine Zelle ist `(Mission, Team, Modul)`; die Liste entsteht aus einer genannten
Regel und nicht aus einer Auswahl — **154 Zellen aus den 100 Missionen der zehn Kampagnen**.

**Zuerst gemessen, welche Gene überhaupt wirken können — 2 464 Läufe, je Gen sein veröffentlichter
Kanal.** G2 (`pilot_cover_frac`): `flt_defer_s` ist **0,0 in allen 2 464 Läufen auf allen 154 Zellen** —
das netzfähige Element trägt die AIM-120, deren Bindung 0,3 s dauert. G7 (`pilot_attack_ccip_m`):
strukturell tot, weil **keine der 54 Angriffsmissionen CCIP fliegt** (42 Dateien `ccrp`, 12 `opt`, 9 `arm`) und
`FBPilot.cpp:1368` den Schlüssel nur im CCIP-Zweig liest. G4 wirkt auf genau den zwölf Zellen, die
`set task bfm` erklären. G6 ist F-16-only: `FBMig29Pilot` überschreibt den Angriffsdurchgang mit
eigenem `ATTACK_CONSENT` und liest `AttackBiasS` nie. Bleibt G3, und nur seine Vertragshälfte auf der
**MiG-29** — 75 Kanal-, 33 Klassenbewegungen.

**Das Sättigungstor, mit dem festen Maßstab als S1-Population wie §4.2 es definiert (924 weitere
Läufe): 0 informative Zellen von 154.** In drei Lesarten geprüft, damit die Zahl nicht am Hebelfile
hängt: mit E2s eigenem `levers-genome.txt` 0, mit dieser Runde 15 Punkten 0, und in der lockersten
Lesart, die das Tor zulässt, **2** — genau das Urteil, das W1 auf seinen zehn Sprossen erreichte. Die
Verteilung ist die ehrliche Form: **89 Zellen bewegt kein Hebel, 46 einer, 15 zwei, 3 drei, 1 vier.**
Das Tor wurde nicht gelockert; `fb_arena_check.py` ist byte-identisch, und ein bodentauglicher
Maßstab, der 46 Zellen auf dem Papier informativ gemacht hätte, wurde aus E2s Grund nicht geschrieben.

**Keine Doktrinverschiebung wird veröffentlicht.** Die bindende Regel gilt: was auf einer Zelle
gemessen ist, die S1–S3 nicht besteht, ist ausdrücklich kein Befund.

**Was die Selektion trotzdem fand, und es ist die zweite Pflichtlieferung.** Vier Einträge, jeder mit
Kanal und Zahl. Der schärfste ist ein Exploit **unserer eigenen Fitness**: `FBMissionRunner` endet am
ersten Flugmonitor-K.O., und `ExpectedLoss` verzeiht nur einem bereits kampfunfähigen Flugzeug — ein
gesunder Strömungsabriss der **Gegenseite** beendet den Lauf, bevor irgendein Monitor abschließt, es
wird **keine einzige `mission OBJECTIVE`-Zeile** veröffentlicht, und Stufe M liest 0. In
`w4-10-allied-force` fällt `kamig4` bei t = 695,3 von 700 s: acht F-16 stehen bei `V = 16, M = 0`. Drei
unabhängige Hebel halten die MiG in der Luft — darunter einer, der die Bomben **2 794 m** danebenwirft
— und dieselben acht Jets stehen bei `V = 18, M = 8`. **17 von 154 Zellen** haben einen Hebel, der
diese Grenze überschreitet.

**Und zwei Defekte der Abwurfkette, gemessen als konstante ZEIT und nicht als Strecke.** Über acht
Angriffszellen, vier Kampagnen, zwei Waffen und vier Höhen liegt das Minimum von `aimErrM(bias)` bei
**−0,20 ± 0,05 s** — das ist eine Latenz, und es ist dieselbe, die W2 als `predErrM = Grundgeschwindigkeit
× 0,228…0,241 s` gemessen hat. Der Hook des Moduls (`AttackReleaseBiasS()`) steht auf **0,0 s**. Mit
−0,20 s fällt die gehärtete Kuppel von W2 (36,38 → **10,06 m**, Klasse (2,1) → (3,2)); X1 besser auf 2
von 8 Zellen und **auf keiner schlechter**, X4a acht Spawn-Störungen über ±3 m ohne Klassenwechsel,
X4b mit +50 % Zeitlimit gehalten. Darunter liegt eine Quantisierung: der Abwurf wird einmal je
Entscheidungstakt geprüft, `aimErrM(bias)` ist eine **Treppe** mit einer Stufe je 0,1 s, und eine Stufe
ist bei 231 m/s **23,1 m** — breiter als der 17,7-m-Radius, den eine Mk-84 gegen ein gehärtetes Ziel
braucht.

**Vierter Befund, unbequem und mit Kanal:** auf der EMCON-Sprosse `w1-07` kostet das kooperative
Datenlink einen F-16 — bei `flt_assign` = `sort_assign` = `eng_shots` = **0 in beiden Varianten**, also
nicht über die Zielaufteilung. Die Divergenzkette ist veröffentlicht und beginnt bei t = 0,1
(`dl_on` → `dl_tracks`/`flt_mates` → `rwr_brg` → Bahn), das Datenlink ist nicht hörbar, also bewegt
sich die **Verbandsgeometrie**. Determinismus über `--threads 1/2/4` an beiden Messpunkten: identische
Telemetrie-Prüfsumme.

`sim/src/` wurde nicht angefasst, `sim/assets` und `sim/missions` sind vor und nach jedem Lauf
byte-identisch, `verify-models` und `verify-layers` grün, sieben Harnesses rc = 0,
`core-lib`/`gym`/`native`/`wasm` warnungsfrei. Was einen Lauf möglich machen würde, steht als E-17 in
den Gaps: eine Fitness, die zwei Angriffsdoktrinen ordnen kann (Stufe C ist auf **32 der 46**
bombenwerfenden Zellen `GATE`), ein fester Maßstab, der auf der Zelle wirkt, die er beurteilt, und für
G2/G7 eine Arena, die es in den Kampagnen nicht gibt.

---

## 2026-07-30 — Doktrin-Evolution `E6`: die Handwerksstufe lernt den Boden, und der Richter schließt immer ab

`E5` hat zwei Defekte an unserer eigenen Fitness gemessen und beide stehen als Zahl da. Diese Runde
repariert sie und veröffentlicht wieder keine Doktrinverschiebung — sie fliegt gar keinen
Evolutionslauf. Bewegt haben sich genau zwei Dateien: `sim/tools/fb_fitness.py` und **ein Block** in
`sim/src/missions/FBMissionRunner.cpp`.

**Erstens: Stufe C hatte am Boden keinen Gradienten.** Jeder Posten war Luft-Luft, also war der
Schlüssel einer Angriffszelle `(V, M, GATE)` und eine Bombe 20 m daneben exakt so viel wert wie eine
2 km daneben — auf **32 der 46** bombenwerfenden Kampagnenzellen. Der neue Posten kommt aus `aimErrM`,
das der Richter ohnehin in jede `stores DELIVERY`-Zeile schreibt: `100 · Mittel über die Abwürfe von
1/(1 + e/10 m)`. Mittel und nicht Summe — eine Summe zahlte pro Abwurf, also pro Waffe, die der
Missionsautor an den Jet gehängt hat, und das ist Exhibit C in einer zweiten Währung.

**Und die zwei Währungen werden nicht addiert.** Ein sechster Summand hätte einen Meter Zielfehler in
Abschussgeometrie-Punkten bepreist — genau das stehende Angebot, gegen das §1.2 argumentiert. `C` ist
jetzt das Paar `(air, aim)`, verglichen über **Dominanz**: besser in einem und nicht schlechter im
anderen gewinnt, besser in einem und schlechter im anderen ist **unvergleichbar** und ist ein
Gleichstand. Es gibt keinen Wechselkurs, den eine Suche annehmen könnte, in keine Richtung. Preis:
Auflösung, nie Ordnung — und die Zellen, auf denen niemand schießt, sind mit demselben Argument frei,
mit dem das Tor sie vorher sperrte, denn die Torbedingung bekommt denselben Bodenarm (eine
veröffentlichte Lieferung).

Gemessen, ein Lauf je Zelle, gelesen von BEIDEN Fitness-Modulen: `C = GATE` fällt von **74 auf 42 von
154** Zellen und von **32 auf 0 von 46** liefernden; **(V, M) bewegt sich auf 0 von 154**. Auf
`w2-01-dome` liegen vier Hebel bei identischem `(V, M) = (2,1)` mit 36,4 / 59,5 / 82,6 / 61 294 m
Zielfehler — vorher vier Mal `GATE`, also exakt gleich, jetzt streng geordnet 21,6 > 14,4 > 10,8 > 0,0.

**Zweitens: X-1, der Exploit, den die Evolution an unserer Fitness gefunden hat.** Der Lauf endet am
ersten Flugmonitor-K.O., und wer dann noch offen war, schloss nie ab — also null `mission
OBJECTIVE`-Zeilen und Stufe M null für alle. **Geändert wurde nicht, WANN ein Lauf endet, sondern dass
der Richter trotzdem abschließt:** `FirstFlightKo` ist bis auf den Takt unangetastet, und die
Abschluss-Schleife läuft **nach** der Urteilskombination, kann also weder `ko` noch `failed` noch
`judged` noch das Ergebnis verschieben. `w4-10-allied-force` liest jetzt in der Grundlinie
`V = 18, M = 8` — genau das, was die drei Hebel liefern, die die MiG am Leben halten; die Beweger auf
dieser Zelle fallen von 3 auf 0, und der Lauf endet unverändert bei t = 695,3 mit `LOC`.

**Erhaltung, gemessen statt angenommen.** Über alle **251** `sim/missions/*.fbm`: **0 bewegte
Telemetriewerte, 0 bewegte Exit-Codes**, 27 `events.log` mit neuen Zeilen (80 `OBJECTIVE`, 68
`RESULT`, 58 `UNIT_RESULT`), Determinismus über `--threads 1/2/4` identisch. Drei Zeilen sagen etwas
anderes statt mehr — `net-belt-high`, `o1-08-belt-netted`, `o3-10-october-six` —, und alle drei sind
die **bestehende** Regel `ShotDownFirst`, die endlich greift: wer kampfunfähig geschossen wurde und
danach den Boden traf, wird vom Missionsrichter gemeldet, nicht vom Physikrichter. Ein gesunder
Abriss meldet weiter `LOC`. Die drei veröffentlichten Turnierergebnisse (`duels.md`, `formation.md`)
wurden auf **beiden** Instrumenten neu geflogen — altes Binary + alte Fitness gegen neues + neues — und
sind identisch bis auf die Ziffer, weil dort keine Bombe fällt und die Zielwährung in allen 70 Läufen
+0,0 ist. Elf Kampagnen: 99 Läufe, 11 Fingerabdrücke, 0 Divergenzen; 104 Einzelwiederholungen, 0
Divergenzen. Fünf Fingerabdrücke halten byte-genau, fünf bewegen sich — und die **acht** bewegten
Schritt-Fingerabdrücke sind exakt die acht Kampagnenmissionen aus der 27er-Liste.

Was diese Runde NICHT getan hat, mit Zahl: das 154-Zellen-Tor wurde **nicht vollständig neu geflogen**.
Die Handwerksstufe kann es nicht bewegen (S1/S2 rechnen auf `(V, M)`), die X-1-Reparatur schon — sie
verschiebt die Klasse jeder Zelle, deren Grundlinie oder Hebel die K.O.-Grenze kreuzte. Geflogen sind
**29 von 154** vollständigen Zellen, Beweger-Verteilung **24 × 0 · 4 × 1 · 1 × 2 von 15**; keine Zelle
erreicht `kMoversMin`. Das steht als Schuld in E-17 und nicht als Argument. Das Tor wurde nicht
gelockert, kein Genom-Schlüssel bewegt, kein Modell angefasst; `verify-models` und `verify-layers` grün,
sieben Harnesses rc = 0, `core-lib`/`gym`/`native`/`wasm` warnungsfrei.

## 2026-07-31 — `E7`: die Schuld ist bezahlt, und S1 hat das falsche Genom gemessen

Zwei Dinge waren offen, beide sind geliefert: das 154-Zellen-Tor **vollständig** neu gefahren nach dem
X-1-Fix, und S1s festes Feld mit dem Genom kommensurabel gemacht. **4.158 Läufe**, `sim/src/`
unangetastet, kein Torkonstante gelockert.

Der Befund der Runde brauchte **null Läufe**: das feste Feld unterscheidet seine sechs Mitglieder in
`pilot_shot_rtr`, `pilot_lock_nm`, `pilot_react_s` — das Genom besteht aus fünf ganz anderen Schlüsseln,
und alle sechs tragen dieselbe `sort`-Allele. Die Schnittmenge ist **leer**. „Informativ = S1 ∧ S2" war
also die Konjunktion zweier Fragen über verschiedene Dinge, und drei Runden Verweigerung gehörten zuerst
dem Instrument. Spec §9 (E15 Kommensurabilität, E16 Erweiterung statt Neuschrift, E17 die gebuchten
Kosten) und zwei Prüfer, die sich weigern statt zu behaupten — Beleg für E16 ist die **Commit-Reihenfolge**
(`f0d8115` vor jedem Lauf, der das Feld liest).

**Die Schuld, bezahlt — und sie widerlegt die Vorhersage, mit der sie gebucht wurde.** `E6` hatte
geschrieben, der X-1-Fix bewege die Klasse jeder der 17 Zellen an der K.O.-Grenze. Gemessen bewegt er die
Beweger-Verteilung um **zwei Zellen** (89·46·15·3·1 → 89·46·16·2·1). Eine Beweger-Zahl ist eine
*Differenz*, und der Fix hat Basis und Hebel meist gemeinsam über die Grenze geschoben.

**S1: 13 → 0, und der Mechanismus ist Arithmetik.** Das kommensurable Feld spaltet **mehr** Zellen (61
statt 52) und besteht S1 auf **keiner**. Alle dreizehn früheren Bestehen sind Zeile für Zeile verfolgt:
die Klassenzahl bleibt exakt gleich (2→2, 3→3, 4→4), der Modalanteil geht 50,0 % → 72,7 % bzw. 33,3 % →
63,6 % — also genau `(alt + 5)/11`. Kein einziges der fünf neuen Mitglieder erzeugt irgendwo eine neue
Klasse. Ein auf einer Zelle inertes Mitglied ist eine **Stimme für den Status quo**. Die Schranke ist
allgemein: die Basisklasse hält auf **allen 154** Zellen ≥ 2 der sechs Mitglieder, also ist der
Modalanteil bei inertem Genom mindestens 63,6 % — über S1s 60 %. Die dreizehn waren **Falschpositive,
jedes einzelne**. `E4`s ungeklärte Beobachtung „S1 und S2 bestehen auf verschiedenen Zellen" ist damit
aufgelöst statt gemildert.

**Die bindende Schranke ist S2, und kein Feld erreicht sie**: S2 zählt über die Hebel, nie über das Feld.
0 von 154 Zellen erreichen die geforderten 5 Beweger, die beste der ganzen Kampagnenbreite hat 4 — und
**5 der 15 Hebel sind auf allen 154 Zellen strukturell tot** (G2 dreimal, G7 zweimal). Selbst die
großzügigste ehrliche Rechnung lässt **eine** Zelle bestehen, gegen S5s drei.

Die Evolution ist **nicht** gelaufen, und das ist kein Versäumnis, sondern §6: auf einer Zelle, die das
Tor nicht besteht, ist nichts ein Befund. `tools/arena-informative.txt` wird vom Tor selbst geschrieben
und enthält null Zellen.

Die fehlende Arena ist jetzt datiert statt beschrieben: **keine** der 100 Kampagnenmissionen wirft in
CCIP (102 × `ccrp`, 32 × `opt`, 20 × `arm`), obwohl Modus und Rig existieren und kein C++ fehlt. Das ist
zuerst eine **Realismuslücke** der Kampagnen und darum baubar, ohne die Arena um ein Gen herum zu bauen.
Neu gebucht: E-19 — S1s Schwelle ist ein Anteil, und ein Anteil ist nicht invariant unter der Größe des
Feldes, in dem er genommen wird. Nicht hier repariert, mit Absicht.

**Nachtrag `E7`, gleicher Tag.** Die Runde hat auch die *andere* Arena gefragt, und sie ist ebenfalls
verweigert: die generierten Geometrien haben bei `--flight 1` noch **1 informative von 12**, gegen die
**4**, die E-12 verzeichnet. Die drei fehlenden sind an den eigenen Reparaturen des Baums verloren
gegangen (E-15s FLCS-Dämpfer, X-1s Richter) — E-15s eigener Satz, auf E-15 angewandt: eine Geometrie,
deren Informativität daher kommt, dass eine Seite an einem Bug stirbt, ist eine Messung des Bugs.

Sind **beide** Arenen zu, kann die Ursache keine Eigenschaft einer Arena sein. Sie ist das Genom, und
die Zählung ist exakt: von den fünf Erweiterungen, die der Eigner-Auftrag nennt, sind **zwei überhaupt
keine Schlüssel** — `set pilot_flight_shape` und `set pilot_emcon_frac` werden bei t = 0.0 abgelehnt und
der Lauf endet mit exit 1, blockiert von `formation.md` F5 und `duels.md` D3. G2 ist mangels
Waffenbindung inert (0 Beweger auf 154 Zellen), G4 lebt nur in `Phase::Bfm` (9 Zellen), und G3 allein
bewegt die Breite (30 Zellen). Das erklärt vier Runden ohne veröffentlichbare Doktrinverschiebung
vollständig — und es ist ein Baurückstand, kein Torproblem. Als E-20 gebucht, mit einer nach
Freischaltwirkung geordneten Liste; der erste Posten ist F5, der zweite D3.

## 2026-07-31 — `E8`: die Arena besteht, die Evolution läuft, und X4 verweigert das Ergebnis

`E-20` hatte gemessen, dass der Blocker das Genom ist, und F5 als ersten Posten benannt. F5 ist gebaut,
G1 ist ein Schlüssel — und alles Weitere folgt in einer Kette, bis zur Verweigerung am Ende, die der
schärfste Befund der Runde ist.

**Ein Gen freizuschalten hat die Kampagnenbreite von 0 auf 3 informative Zellen gehoben** und die erste
Evolution dieser Linie vollständig laufen lassen. G1 bewegt die Ergebnisklasse auf **13 Zellen**; bei 21
Hebeln liegt S2s Schwelle bei 7, und **vier Zellen** erreichen sie — die ersten S2-Bestehen überhaupt.
Das Tor: `S4 154 ≥ 6 ok · S5 3 ≥ 3 ok · S6 0 ok · ARENA: PASSED`. Alle drei informativen Zellen sind der
MiG-29-Sitz; `w3-09-saturation:f16` besteht S2 mit 7 Bewegern und fällt an S1. Diese Runde evolviert
also eine Doktrin, nicht zwei.

**Die Evolution:** 723 Läufe, sechs Generationen, Population 40, acht lebende Gene. Der Champion stammt
aus Generation 0 und hat sich nie bewegt — sechs Generationen Gitterabtastung über sieben numerische
Gene bewegen nichts, entschieden hat allein `sort=near`. Der feste Maßstab bleibt über alle sechs
Generationen bei 0,556, also **flach**; mit T = 0,0000 ist das nach E-16 ein Fixpunkt und kein Kreisen.
Gesättigt ist die Arena nicht: Stufe V entscheidet in Generation 5 **612** Vergleiche.

**X3 besteht.** Die Kette ist mit Zahlen benennbar: auf `o3-10` geht `flt_src` 0 → 2, `flt_assign` 0 → 4,
`SORT_ASSIGN` 0 → 20 — ohne den gebrieften Vertrag hat der Verband **gar keine Zuweisungsquelle**, denn
die MiG-29 hat kein kooperatives Terminal. E2s Satz „ein Vertrag neben einem lebenden Netz ist toter
Text" ist hier umgekehrt: der Vertrag ist der einzige Text. Auf `w3-09` sortiert er zusätzlich
**stabiler**, `flt_switch` 12 → 9.

**Und X4 verweigert.** Die Bahnstörung über ±3 m kippt die Ergebnisklasse auf `o3-10` in 3 von 8 und auf
`w1-09-lfe-four` in 3 von 8 Proben; §5s Rauschboden ist 2 von 8, und darüber gilt „no claim may be made
on that geometry at all". Die zweite Messung klärt, wem das Chaos gehört: mit einem Genom, das ein
einziges unbeteiligtes Gen setzt, kippt `w1-09-lfe-four` in **8 von 8**. Es ist die Zelle, nicht der
Champion. Damit bleibt X1 eine Geometrie, und §6 ist bindend: **es wird kein §1 veröffentlicht.**

Das ist das Produkt der Runde, und es qualifiziert das Tor selbst: S1 und S2 können **einen Hebel nicht
von einer Münze unterscheiden**. Ausgerechnet die Zelle mit den meisten Bewegern der ganzen Breite (8 von
21) kippt bei jeder 0,8-m-Störung. Als E-21 gebucht, mit dem Vertrag, den es verlangt — ein siebtes
Kriterium S7: informativ nur, wenn die Ergebnisklasse der Basis dasselbe 0,8-m-Gitter überlebt, acht
Läufe je Zelle. Auf diese Runde angewandt bliebe **eine** Zelle, und die Arena wäre verweigert — ehrlich.

**Korrektur zu `E8`, noch am selben Tag und aus eigener Prüfung.** E8s Kernzahl — „von 0 auf 3
informative Zellen" — ist **kontaminiert und keine Messung**. Der Lauf hat Basis und die 15
Vertragshebel aus einem Kanalindex übernommen, der **vor** F5s Änderung an `FormationTrailM`
geschrieben wurde, und nur die 6 Formhebel mit dem neuen Binary geflogen. Damit wurden die sechs gegen
eine Basis aus dem *alten* Simulator verglichen — und sie sehen genau auf den Zellen wie Beweger aus,
die der Vorgabenwechsel bewegt: den Vierer-Verbänden.

Gefunden habe ich es an einem Widerspruch, den ich nicht wegerklären konnte: S7 las
`w3-09-saturation` als 8-von-8-Kipper, während das eigenständige Audit dieselbe Zelle als 0 von 8
gelesen hatte. Die Ursache war die Vergleichsbasis, nicht die Zelle — zwischengespeichert `(11,5)`,
frisch geflogen `(10,4)`. In einem Codepfad nachgemessen ist `w3-09-saturation` bei **0 von 8** robust,
`o3-10` bei 3, `w1-09-lfe-four` bei 8.

Der Fix ist strukturell und nicht eine Gewohnheit: ein Kanalindex trägt jetzt den SHA-256 des
Simulators, der ihn geschrieben hat, und **verweigert** die Wiederaufnahme unter einem anderen.
Negativtest grün. Der komplette Hebel- und Feldpass fliegt neu (3.388 + 2.156 Läufe); die korrigierten
Zahlen kommen als `E9`.

## 2026-07-31 — `E9`: die korrigierte Zahl, und warum ein wachsendes Genom dieses Tor nicht öffnen kann

E8s Kernzahl ist zurückgezogen; hier ist dieselbe Messung sauber geflogen — **3.388 Hebelläufe, jeder
unter Simulator `4b10f951`**, und der Kanalindex trägt jetzt dessen SHA-256 und verweigert die
Wiederaufnahme unter einem anderen.

| | `E8`, kontaminiert | `E9`, sauber |
|---|---:|---:|
| beste Zelle, Beweger von 21 | 8 | **6** |
| Zellen über S2s Schwelle 7 | 4 | **0** |
| informativ | 3 | **0** |
| Urteil | PASSED | **REFUSED** |

G1s echte Reichweite je Hebel: 5 / 5 / 5 / 4 / 1 / 1 Zellen — gegen 9 / 9 / 8 / 8 / 3 / 3 im
kontaminierten Lauf. Der S7-Schirm lief gar nicht, weil er S1∧S2-Kandidaten schirmt und es keine gab.

**Der Befund ist eine Arithmetik.** S2s Schwelle ist ein Verhältnis, also hebt ein Gen mit *k* Hebeln
die Schranke um *k/3*. G1 brachte sechs Hebel, lieferte auf der besten Zelle **drei** Beweger, und die
Schranke stieg um **zwei**: von *beste 4 von 15, Schwelle 5* auf *beste 6 von 21, Schwelle 7* — das
**Defizit bleibt 1**. Das ist E10 genau wie geschrieben; ungeschrieben war die Folge: **ein Gen hilft
nur dort, wo seine eigene Reichweite JE ZELLE k/3 schlägt.** Ein Gen, das viele Zellen um je einen
Hebel bewegt (G1: 13 Zellen), kann dieses Tor nicht öffnen — nur eines, das EINE Zelle in dreien seiner
eigenen Hebel bewegt. Als E-22 gebucht, und für das nächste Gen vorab falsifizierbar: G5s
EMCON-Hebel müssen auf einer einzelnen Zelle ≥ 3 der eigenen bewegen, sonst bewegen sie die Schranke
und nicht das Urteil.

Die beste Zelle der Breite ist jetzt `w3-09-saturation:f16` mit sechs Bewegern aus **drei Familien
gleichzeitig** — und damit ein F-16-Sitz. Die drei MiG-29-Zellen, die E8 zertifiziert hatte, waren ein
Artefakt.

Nebenbei und ohne ein Ergebnis zu ändern: der feste Maßstab fliegt nur noch auf Zellen, die S2 schon
bestanden haben. Diese Runde sparte damit 2.156 Läufe, weil S2 nirgends hielt.

**Nachtrag `E9`: D3 ist lokalisiert, und es ist eine Zeile.** `pilot/FBPilot.cpp:765` — `CanPressOn`
liest `state.Radar.Radiating`, also entscheidet ein Pilot, der die Emission abschaltet, im selben Takt,
dass er den Auftrag nicht fortsetzen kann. Der Befehlsweg, den er bräuchte, ist vollständig da und wird
von ihm nicht benutzt: `FBCommandTarget::RadarEmission` existiert, `FBMig29Emission::Off` existiert,
beide Module ehren ihn, und `FBMig29Pilot` schaltet auf GCI-Stichwort bereits auf `Illum`. Worauf jede
Zelle still fliegen würde, ist ebenfalls schon veröffentlicht: die MiG-29 auf dem IRST-Block (Winkel,
keine Entfernung, keine Identität), die F-16 auf Datalink und NetLink. Die Abnahme steht nach E-22
vorab fest: G5s Hebel müssen auf EINER Zelle ≥ 3 der eigenen bewegen. Beste Zelle heute
`w3-09-saturation:f16`, 6 von 21 gegen Schwelle 7 — Defizit 1.

## 2026-07-31 — `E10`: jede Reparatur nimmt dem Tor Doktrinsignal weg

`duels.md` D3a ist gebaut — `CanPressOn` fragt nach einem **Bild** statt nach einem Sender — und das
154-Zellen-Tor ist unter dem neuen Simulator neu geflogen (3.388 Läufe, frischer Index; den alten hat
der Wächter verweigert). Die Arena ist wieder verweigert, und *wie* sie verweigert ist, ist der Befund.

`w1-07-emcon:f16` fällt von **5 Bewegern auf 0**, Basis (3,1) → (4,2). Alle fünf waren der Defekt: die
Hebel haben umgeschaltet, **ob der Jet abbricht**, nicht wie er kämpft. Die beste Zelle der Breite bleibt
`w3-09-saturation:f16` mit 6 von 21 gegen Schwelle 7 — Defizit weiterhin 1.

**Das Muster ist jetzt vier unabhängige Fälle:** E-15s Flugregler nahm `xmerge`/`xmergesplit` ihren
S1-Pass (2 Klassen bei 50 % → 1 bei 100 %); X-1s Richter nahm `w4-10-allied-force:f16` seine 3 Beweger;
beide zusammen nahmen der generierten Arena 3 ihrer 4 informativen Geometrien; und D3a nimmt der
EMCON-Sprosse alle fünf. E-15 hatte die Regel für eine Geometrie geschrieben — eine Geometrie, deren
Informativität daher kommt, dass eine Seite an einem Bug stirbt, ist eine Messung des Bugs. Vier Fälle
später ist es eine Eigenschaft dieser Arena: **die scheinbare Doktrinempfindlichkeit der Kampagnenbreite
war überwiegend defektgetrieben, und jede Reparatur senkt sie.**

Daraus folgt keine Ausrede. Jede der vier Reparaturen hat den Simulator korrekter gemacht, und
`w1-07-emcon` gelingt jetzt, wo es zweimal scheiterte. Es folgt eine Aussage über das **Instrument**:
ein Kriterium auf „bewegt sich die Ergebnisklasse" misst eine Mischung aus Doktrin und Defekt, und in
diesem Baum war die Mischung überwiegend Defekt. Was nach den Reparaturen übrig bleibt, ist das echte
Signal — und das ist heute **einen Beweger von S2 entfernt, auf einer Zelle von 154**. Als E-23 gebucht.

Und noch eine eigene Bedingung ist gefallen und bleibt mit ihrer Messung stehen: D3as Spec verlangte ein
NO-OP über alle 251 Missionen, gemessen bewegen sich vier. Es ist eine Verhaltensänderung, keine reine
Vorbedingung — alle 251 Exit-Codes stehen, Determinismus über 1/2/4 Threads hält.

## 2026-07-31 — `E11`: das Genom ist vollständig, S2 fällt zum ersten Mal, und S7 hält

G5 ist gebaut. **Kein Gen der Auftragsliste ist mehr blockiert** — neun lebende Gene, null Blocker, wo
vor zwei Runden zwei von fünf nicht einmal Schlüssel waren. Das 154-Zellen-Tor ist mit 24 Hebeln neu
geflogen (3.850 Läufe, frischer Index).

**`w3-09-saturation:f16` besteht S2 — die erste Zelle überhaupt in dieser Datei**, und nicht knapp:
**11 Beweger von 24** gegen Schwelle 8, drei Ergebnisklassen bei 53,3 % Modalanteil (S1 ok), und **vier
Genfamilien wirken gleichzeitig** — Netz, Abwurfvorhalt, Form und Emission.

E-22 hatte die Abnahme für G5 **vorher** festgelegt: seine Hebel müssen auf einer Zelle ≥ 3 der eigenen
bewegen. Zwölf Probeläufe vor dem Sweep zeigten 2 von 3, und der Sweep hob die Zelle von 6 von 21 auf 11
von 24. Das ist die erste quantitative Vorhersage dieser Datei, die vor dem Lauf stand und eintrat.

**Und S7 verweigert sie trotzdem: 1 Kipper von 8.** Die Schwelle bleibt bei null — sie wurde in §10 mit
der Begründung gesetzt, dass Zulassung strenger prüft als §5s 2-von-8-Boden für das Lesen eines
Champions, und ausdrücklich mit der Erwartung geschrieben, E8s Arena verweigern zu müssen. Sie jetzt zu
lockern, wo ich weiß, dass es das Tor öffnete, ist genau der Griff, den diese Runde dreimal abgelehnt hat.

Legitim ist, das **Instrument** zu schärfen statt des Kriteriums. Auf einem 0,25-m-Gitter mit 24 Proben:
**3 Kipper von 24 = 12,5 %** — exakt derselbe Anteil wie 1 von 8. Die Zelle ist wirklich zu einem Achtel
eine Münze, und S7 hat recht.

Damit hat sich der Grund der Verweigerung zum ersten Mal verschoben: nicht mehr „das Genom kann nicht
wirken" — es wirkt mit vier Familien gleichzeitig —, sondern **die Kampagnenbreite hat keine Sprosse, die
zugleich benotbar und robust ist**. Das ist eine Aussage über die Missionen, und sie ist jetzt beziffert:
gebraucht werden ≥ 3 Sprossen mit ≥ 8 Bewegern von 24 **und 0 Kippern von 24**.

**Nachtrag `E11`: benotbar und robust stehen NICHT im Widerspruch.** Die naheliegende Sorge nach S7 war,
dass eine Zelle nur dadurch doktrinempfindlich wird, dass sie auf einer Messerschneide sitzt. Gemessen an
den zwölf beweglichsten Zellen, jede über S7s eigenes 0,8-m-Gitter: **zehn von zwölf sind sauber.**
Chaotisch sind nur die zwei beweglichsten — und selbst das ist kein Gesetz, denn `o5-09-night-two:f16`
trägt fünf Beweger bei null Kippern.

Das Ziel ist damit nicht „das Chaos beheben", sondern **robuste Zellen beweglicher machen**. Die zwei
besten Kandidaten scheitern aus verschiedenen, strukturellen Gründen: `o5-09-night-two` ist eine ROTTE,
also können die Trail-Hebel gar nicht wirken (`aftM = element × trail`, und eine Rotte hat kein zweites
Element) — zwei von G1s sechs Hebeln sind auf ihr unerreichbar. `w3-10-package-q` ist ein Vierer mit Netz
und sechzehn `datalink on`, und trotzdem bewegen ihn weder Form noch Emission; das ist das Nächste zu
messen, nicht das Nächste anzunehmen.

Und was daraus NICHT werden darf: eine committete Sprosse zu ändern, WEIL es die Beweger höbe, wählt die
Arena nach dem Ergebnis aus. Ob eine Rotte ein Vierer wird, ist eine Doktrinfrage dieser Kampagne, und
das Tor ist die Prüfung darauf — nie der Grund dafür.

**Nachtrag `E11`: die Kampagnenschicht ist unter dem aktuellen Simulator frisch nachgewiesen.** Nach F5,
D3a und G5 stand die Determinismus-Abnahme aus — sie ist gefahren, nicht angenommen:

| | |
|---|---|
| Kampagnenläufe | **99** = 11 Kampagnen × 3 Wiederholungen × `--threads 1/2/4` |
| Fingerabdrücke je Kampagne | **genau einer**, 11 von 11, rc = 0 |
| Einzelnachspiele | **104** Schritte, jeder standalone aus dem Zustandsfile des vorigen |
| Divergenzen | **0** in beiden Kriterien |

Das ist rund 1.100 Missionsläufe. Beide Kriterien aus `missions/campaign.md` §5 halten: dieselbe
Kampagne gibt über drei Threadzahlen und drei Wiederholungen denselben Fingerabdruck, und jeder Schritt
ist aus dem Zustand seines Vorgängers einzeln reproduzierbar — die Kampagnenschicht fügt also keinen
verborgenen Zustand hinzu. Damit ist „mehrfach und deterministisch durchgespielt" für den heutigen Stand
belegt, nicht für einen von gestern.

**Nachtrag `E11`, letzter: was die zwei robusten Kandidaten publizieren.** `w3-10-package-q:f16` ist ein
Vierer mit drei Kameraden, dessen Sortierung **nie greift** — `flt_src = 0` und `flt_assign = 0` auf der
Basis und auf jedem Form- und Emissionshebel gleichermaßen. Ob das ein Defekt der Verbandslogik im
Vierer ist oder eine Eigenschaft einer Geometrie, in der es nichts zu teilen gibt, ist als Nächstes zu
**messen** — geraten wird es hier nicht.

`o5-09-night-two:f16` dagegen trägt eine **Kette**: `emcon-tight` hebt `releases` von 0 auf 2,
`deliveries` von 0 auf 2 und damit `M` von 1 auf 4. Der Jet, der nicht strahlt, wird nicht gewarnt,
überlebt bis zum Abwurfpunkt und drückt. Jedes Glied ist eine publizierte Spalte, und **die Zelle ist
robust: 0 Kipper von 8.** Das ist das Nächste an einer Doktrinverschiebung, was diese Datei je gemessen
hat — und §6 verbietet weiterhin, es zu veröffentlichen, weil die Arena eine solche Zelle hat und drei
braucht. Es steht hier als Messung, nicht als §1.

**Nachtrag `E11`, und er widerlegt meine eigene Vermutung von einer Stunde vorher.** Ich hatte aus
`w3-10-package-q:f16`s `flt_src = 0` geschlossen, dass sechzehn summierte Einheiten einen Doktrineffekt
**verdünnen**. Über alle 154 Zellen gemessen ist das falsch, und die Wahrheit läuft andersherum:

| Einheiten der benoteten Seite | Zellen | Ø Beweger von 24 | max |
|---:|---:|---:|---:|
| 1 | 41 | 0,44 | 3 |
| 2 | 66 | 0,45 | 2 |
| 4 | 30 | 1,37 | 6 |
| 8 | 6 | **2,00** | **11** |
| 16 | 1 | **4,00** | 4 |

**Benotbarkeit steigt mit der Größe der benoteten Seite.** Und damit liegt die Masse der Arena am
falschen Ort: **107 der 154 Zellen sind ein einzelnes Flugzeug oder eine Rotte**, im Mittel bei 0,44
Bewegern. Ein einzelner Jet hat keinen Verband zu formen, keinen Kameraden zum Sortieren und niemanden,
hinter dem er schweigen könnte. Diese Datei hat vier Runden lang im Genom und im Tor gesucht, was eine
Eigenschaft der **Seitengröße** ist.

Damit ist das Ziel aus §4 in seiner schärfsten Form da: die drei Sprossen, die das Tor braucht, sind
nicht irgendwelche drei — es sind drei mit **vier Flugzeugen oder mehr**, die zugleich robust sind. Von
denen gibt es in der Breite 38, und genau eine erreicht heute acht Beweger. Es ist die chaotische.

**Nachtrag `E11`, Abstand statt Richtung.** Von 154 Zellen haben 44 mindestens vier Einheiten. Davon
erreicht **genau eine** S2s Schwelle von 8 Bewegern — `w3-09-saturation:f16` mit 11 —, und die kippt 1
von 8. Die zweitbeweglichste (`o5-08-night-one:f16`, 6) kippt 2 von 8. Die beste **robuste** Zelle ist
`o5-09-night-two:f16` mit **5 Bewegern und 0 Kippern**: drei zu wenig.

Die drei Hebelsätze, die diese Datei geflogen hat, sagen dasselbe von der anderen Seite:

| Hebel | Schwelle | beste Zelle | bestehen S2 |
|---:|---:|---:|---:|
| 15 | 5 | 4 | 0 |
| 21 | 7 | 6 | 0 |
| 24 | 8 | **11** | **1** |

Das Genom wachsen zu lassen hat also sehr wohl irgendwann einen Pass erzeugt — den ersten überhaupt —,
aber mit etwa einer Zelle je zwei Gene gegen eine Schranke, die mit jedem Gen mitwächst (E-22). **Und es
ist kein Gen mehr übrig**: der Auftrag nennt fünf, alle fünf leben. Übrig ist die Arena selbst, und wo,
steht gemessen da: 107 von 154 Zellen sind ein Flugzeug oder zwei.

**Nachtrag `E11`, Korrektur an mir selbst.** Ich hatte geschrieben, die Masse der Arena liege „am
falschen Ort". Das ist über die Messung richtig und über die Ursache falsch. Gemessen an der Zahl
befreundeter F-16/MiG-29 je Sprosse: **jede Kampagne ist eine LEITER** — w1 hat sechs Sprossen mit ein
bis zwei Flugzeugen und steigt auf sechs, w3 hat zwei kleine und steigt auf **sechzehn**, o5 sechs kleine
und steigt auf sechs. Die 107 kleinen Zellen sind die unteren Sprossen und für eine Leiter genau richtig:
`w1-01-merge` ist einer gegen einen, weil ein Lehrplan damit anfängt. Sie „falsch platziert" zu nennen
war mein Fehler.

Die wirkliche Grenze ist enger und betrifft den **Satz**, nicht die einzelne Kampagne: zehn Leitern
ergeben zehn Spitzen, und **nur zwei Sprossen der ganzen Breite stellen acht oder mehr Flugzeuge auf die
benotete Seite** (`w3-09-saturation` mit 8, `w3-10-package-q` mit 16). Benotbarkeit steigt mit der
Seitengröße, S5 will drei informative Zellen — und die Breite bietet zwei Sprossen der Größe, in der
Doktrin überhaupt messbar ist. Eine davon ist chaotisch.

Die Entscheidung, auf die das Tor wartet, ist damit eine **Kampagnen-Entwurfsentscheidung**: ob der Satz
aus zehn Leitern mehr Spitzen tragen soll. Aus einer Beweger-Zahl lässt sie sich nicht ableiten, ohne die
Arena nach dem Ergebnis auszuwählen — und das hat diese Datei fünfmal abgelehnt.

**Nachtrag `E11`, Abschluss der Diagnose.** `w2-10-opera:f16` stellt **zehn** Flugzeuge und trägt **2**
Beweger — der Vertrag seiner Kampagne sagt warum: *„eight of the ten missions have no air opposition at
all — the subject is reach, not combat."* Teilt man die 44 großen Zellen danach, ob die committete Datei
einen gegnerischen Jäger enthält:

| große Zellen (≥ 4 eigene) | Zellen | Ø Beweger | **max** |
|---|---:|---:|---:|
| bekämpft | 36 | 1,47 | **11** |
| unbekämpft | 8 | 1,50 | **2** |

**Die Mittelwerte sind gleich, die Decken nicht.** Gegnerschaft macht eine Zelle nicht im Schnitt
benotbar — sie macht eine *hohe* Beweger-Zahl überhaupt erst möglich. Acht unbekämpfte Zellen kommen nie
über zwei, weil eine Luftdoktrin niemanden hat, gegen den sie eine wäre.

Damit steht die Anforderung vollständig, und keine ihrer vier Zeilen betrifft Genom oder Tor: **groß**
(44 von 154), **bekämpft** (36 davon), **robust** (zehn der zwölf beweglichsten, aber nicht die zwei
beweglichsten) und dann S2s acht Beweger — **1 von 36, und das ist eine der beiden chaotischen.**

**Nachtrag `E11`, fünfte und letzte Zeile der Diagnose.** Die größte symmetrische Begegnung der Breite
ist `o1-10-mole-cricket`, acht gegen acht — und sie trägt `(16, 0)` auf **beiden** Seiten bei null
Bewegern. Kein Defekt: die Datei sagt selbst *„NO AIRCRAFT IN THIS SORTIE DECLARES `objective survive`,
AND NO FIGHTER FLIES IN IT AT ALL"*, und ihre Leseregel nennt fünf Kanäle — `campaign CARRY`,
`site LAUNCH`, `net LOST`, das `eng_*`-Debriefing je Jet, die ATTRITION-Zeile. **Die Fitness liest keinen
davon.**

Gemessen: **13 von 154 Zellen tragen `M = 0` auf allen 24 Hebeln, und keine einzige hat einen Beweger.**
Eine Sprosse, deren Produkt ein Verschleißverlauf ist statt einer Zielzahl, ist für eine Ergebnisklasse
aus `(V, M)` strukturell unsichtbar — und unsichtbar kann sie nie informativ sein, gleich wie groß und
wie bekämpft.

Das ist die einzige der fünf Zeilen, die sich nicht durch Missionsbau beheben lässt: sie ist eine Aussage
darüber, was die **Fitness** liest. Level M zu verbreitern, nachdem ich gemessen habe, welche Zellen es
nicht sieht, wäre das Instrument nach dem Ergebnis auszuwählen. Als E-24 gebucht, mit der Zahl.

**Nachtrag `E11`, das Audit der 20 bewegten Missionen — und es findet mehr als eine Rechtfertigung.**
Die gebuchte Schuld ist bezahlt: von den 20 Missionen, die G5s Vorgabe bewegt hat, tragen **fünf** eine
identische Ergebnisbilanz (nur Telemetrie), **fünfzehn** bewegen ihr Ergebnis, **einer** seinen
Exit-Code. Und keine einzige ihrer Leseregeln nennt eine `FAIL`-Zahl oder einen Exit-Code als Urteil —
mehrere schließen es in Großbuchstaben aus: *„Read the DETECTION TIMES, not the kills"* (w3-07),
*„A reader who reports a kill count has read a warhead's arrival geometry"* (w3-09), *„NEITHER CODE IS
THE VERDICT"* (o3-10), *„Do NOT read a kill as a lever's effect"* (w4-10).

Das allein wäre eine Ausrede. Die ehrliche Prüfung ist, ob sich die Werte bewegt haben, die jede Regel
**benennt** — und bei `w1-09-lfe-four` tun sie es hart: seine Regel sagt *„the question is the CHURN,
not the kill count"*, und `flt_switch` fällt von **179 auf 17**, `flt_dup` von 3 auf 0.

Über fünf unabhängige Missionen gemessen ist das systematisch: `flt_switch` 117→15, 179→17, 178→60,
48→3, 42→9 — **65 bis 94 Prozent** —, und `flt_dup` geht auf **vier von fünf** auf null. Das ist
`formation.md` **F2**, seit der Formationsrunde offen, und der Mechanismus ist F2s eigene zweite Ursache
vorwärts gelesen: F2 nannte „einen Per-Frame-Jitter in der Kontaktliste, den die Hysterese nicht fängt" —
und EMCON schaltet das Radar dort ab, wo das Bild des Rottenkameraden ohnehin trägt, also entsteht die
zitternde Liste gar nicht erst.

**Das ist keine Doktrinverschiebung** in §6s Sinn — keine Arena hat bestanden. Es ist die Churn, die in
dem Kanal fällt, den diese Lücke selbst als ihr Maß deklariert hat, auf fünf Missionen, mit einer
benennbaren Ursache. Offen bleibt: die Hysterese ist unangetastet, und auf `w3-09-saturation` fällt die
Churn um 66 % ohne dass `flt_dup` sich bewegt.

**Nachtrag `E11`, letzte Verschärfung.** S7 urteilt auf acht Proben; damit ist auch ein „0 von 8" nur
mäßig belastbar. Die drei entscheidenden Zellen sind deshalb auf dem 0,25-m-Gitter mit **24** Proben
nachgemessen:

| Zelle | Beweger von 24 | Kipper von 24 |
|---|---:|---:|
| `w3-09-saturation:f16` — die einzige, die S2 besteht | 11 | **3 = 12,5 %** |
| `o5-09-night-two:f16` — trägt die stärkste Kette | 5 | **0 = 0,0 %** |
| `w3-10-package-q:f16` | 4 | **0 = 0,0 %** |

Beide Robustheits-Urteile halten bei dreifacher Probenzahl, und das chaotische ebenso — 3 von 24 ist
exakt der Anteil, den 1 von 8 angezeigt hatte. Die Lage der Arena ist damit auf ihren drei
entscheidenden Zellen mit dreifacher Strenge vermessen, und keines der Urteile hat sich gedreht.

**Sonde (`E11`), und sie beantwortet die Frage der ganzen Linie.** Auf `w3-09-saturation:f16` — der einen
Zelle, die S1 und S2 besteht und an S7 fällt — ist die Evolution als **Sonde** gelaufen, ausdrücklich
gekennzeichnet und ohne etwas zu veröffentlichen (Präzedenz: E5s Zwei-Zellen-Sonde, „*neither publishes
anything*"). 227 Läufe, fünf Generationen.

| | Ergebnis |
|---|---|
| Champion | **bewegt sich**: g0 → `g1_40` → `g2_53` |
| Stufe M entscheidet | **32 Vergleiche in Gen 3, 6 in Gen 4** — zum ersten Mal in dieser Datei überhaupt |
| Doktrin-Trajektorie (c) | 0,5154 / 0,1250 statt der 0,0000 von `E8` |
| fester Maßstab | 1,000 über alle fünf Generationen |
| gegen die Basis | **V=20 M=12 gegen V=16 M=10**, C +787,9 gegen +65,1 |

**Der Suchoperator war nie das Problem — er war ausgehungert.** Auf einer Zelle, die ihn trägt, bewegt er
sich, verbessert beide entscheidenden Stufen und schlägt das feste Feld. Der Maßstab steht allerdings ab
Generation 0 bei 1,000, also ist daran kein Anstieg messbar, und die Zelle fällt an S7 (3 Kipper von 24).
**Es wird nichts veröffentlicht.** Was die Sonde zeigt, ist eine Eigenschaft des SUCHOPERATORS und nicht
der Doktrin: die Maschinerie trägt, sobald die Arena sie trägt.

## 2026-07-31 — Die Spieler-Schicht, erste spielbare Runde: drei Bildschirme, die nur lesen

`doc/player-layer.md` §§1–6 sind gebaut, §§8–10 nicht. Die WASM-App startet jetzt in ein Menü:
**Kampagnen-Select** (die elf `sim/campaigns/*.fbc` mit ihrer eigenen Kopfzeile und Sprossenzahl),
**Mission-Select** (die `.fbc`-Reihenfolge IST die Leiter, Sprosse *k+1* öffnet, wenn *k* zu einem
Urteil geflogen wurde) und **Debriefing** (welche Ziele erfüllt sind, ob es für einen Abschluss reicht —
**kein Score, keine Punkte, keine Sterne**). Alles davon liegt in `sim/web/`; unter der Schicht wurden
zwei Zeilen im Client angefasst und keine im Kern.

**Die Schicht rechnet nichts.** „Erfüllt" ist der Zustand, den `FBMissionMonitor::Conclude` selbst in
seine `mission OBJECTIVE`-Zeile schreibt; „abgeschlossen" ist §3.1 über genau diese Zeilen. Jede
angezeigte Zeile trägt ihren Herkunftsschlüssel als **Konstruktorargument** — eine Zeile ohne Quelle
lässt sich nicht bauen — und landet als `data-src` im DOM. Der Zielblock ist per Assert exakt die
`OBJECTIVE`-Menge der Sitzposition: keine Menge wächst.

**Gemessen.** §3.2s Abnahme über den Baum: `COMPLETED == (exit == 0)`, **36 Missionen, 36 Treffer, 0
Abweichungen**. Und die Abnahme selbst war zu locker formuliert — „Ziele auf genau einer Einheit" ist
nicht dasselbe wie „genau eine GERICHTETE Einheit": ein zweites Flugzeug, das nur eine Route erklärt,
trägt einen Monitor und entscheidet den Exit-Code, während die Sitzposition SUCCESS hat. Vier Missionen
haben genau diese Form (`f16-aim9`, `mig29-defend`, `mig29-r27`, `mig29-r73`); das Kriterium ist
korrigiert, seine eigene Prosa sagte es zwei Sätze später bereits richtig.

**Im Browser geflogen, nicht simuliert:** `viper-attrition` aus dem Menü heraus, Sprosse 1
(`intercept-aim120`), 291 s bis zum Urteil des Richters — Leiter `oLLL` vor dem Lauf, `ooLL` danach,
Speicherstand `step viper-attrition 1 attempts=1 verdicts=1 completed=0 last=TIMEOUT`.

**Zwei Zeilen unter der Schicht, beide begründet.** `window.FB_MISSION` sagt dem Client, WELCHE Datei
er fliegt (gelesen wie `FB_TILES_URL`, auf einen Dateinamen gesäubert) — ohne sie kann ein
Mission-Select nichts auswählen. Und der Missions-Puffer des Browsers stand auf 8 KB, während
`fb_fetch_text` still abschneidet: sechs Missionen des Baums wurden gekürzt, und ein Schnitt auf einer
Zeilengrenze parst zu einer KLEINEREN BESETZUNG — ein Lauf gegen eine Datei, die niemand geschrieben
hat. 64 KB, und ein voller Puffer wird jetzt verweigert.

**Was fehlt, steht als Lücke da statt als Erfindung im Frontend:** die Trennung Primär/Sekundär (das
`.fbp` aus §2.2 ist nicht gebaut, also ist per Vorgabe alles primär — und genau dadurch fällt die
Spielregel mit dem Urteil des Richters zusammen), Schwierigkeitsstufen mit Referenzlauf, der
Kampagnen-Carry im Client (jede Sprosse fliegt STANDALONE), `UNIT_RESULT` im Browser (§3.1 (c) liest
ersatzweise `monitor KO` — strikt strenger, nie großzügiger) und der ganze eigene Halbteil des
Debriefings (§5.1s Telemetriezeilen: der Browser schreibt keine Telemetriedatei).

## 2026-07-31 — der deklarierte Messplatz: von 0 auf 1 informative Zelle, und die Entwurfsregel trägt

`E11` hatte die vier Eigenschaften gemessen, die eine Zelle informativ machen können — groß, bekämpft,
für Stufe M sichtbar, robust — und festgestellt, dass die Kampagnenbreite genau **eine** solche Zelle
hat, wo S5 drei verlangt. Die zehn Kampagnen sind Leitern und bleiben unangetastet; sie zu ändern,
damit das Tor aufgeht, wäre die Arena nach dem Ergebnis auszuwählen.

Also entsteht der Messplatz getrennt und **deklariert sich als solcher**: zehn `ar-*`-Sprossen, je 8
eigene Flugzeuge in Vierern gegen 16 Gegner, 32 `mission OBJECTIVE`-Zeilen je Lauf, Tageszeiten von
00:30 bis 22:00, Determinismus 10 von 10 über `--threads 1/2/4`. Gebaut nach den vier Eigenschaften und
nach nichts sonst — Robustheit ist **nicht** hineingebaut, sondern wird vom Tor geprüft.

| | Kampagnenbreite | Messplatz |
|---|---:|---:|
| Zellen | 154 | 20 |
| informativ | 1 | **1** |
| Ausbeute | 0,6 % | **5 %** |

Die eine ist `ar-08-close-day:f16`: **9 Beweger von 24**, S1 mit vier Ergebnisklassen bei 60,0 %
Modalanteil, S2 bestanden, und **0 Kipper von 8** im Chaos-Schirm. `ar-06-beam-afternoon:f16` steht bei
8 Bewegern und fällt an S1 mit 66,7 % — knapp. `ar-05-beam-dawn:f16` bei 7 an S2.

**Das Tor verweigert weiter**, denn S5 will drei. Aber die Entwurfsregel ist damit belegt statt vermutet:
Zellen, die nach den vier gemessenen Eigenschaften gebaut sind, werden benotbar — mit siebenfach höherer
Ausbeute als die Leitern, die für etwas anderes gebaut wurden.

**Nachtrag, und er qualifiziert das eigene Ergebnis:** die eine informative Zelle des Messplatzes
besteht S1 mit **null Spielraum**. Über die 15 Feldmitglieder verteilt sich `ar-08-close-day:f16` auf
9 / 3 / 2 / 1 Ergebnisklassen — Modalanteil **exakt 60,0 %**, und S1s Schranke ist „≤ 60 %".
`ar-06-beam-afternoon:f16` verteilt sich 10 / 3 / 2 und verfehlt sie um **ein einziges Mitglied**.

Bei 15 Mitgliedern ist 9/15 der einzige erreichbare Wert, der die Schwelle exakt trifft. Das ist
dasselbe Zusammenspiel von Konstante und Feldgröße, das als **D11** gebucht ist: `kModalMax = 0.60` ist
gegen ein Feld von sechs geeicht, und keine Konstante im Tor weiß, wie groß ihr Feld ist. Ein
Bestehen auf der Schranke ist kein Bestehen mit Reserve, und es wird hier so genannt statt in einer
Fußnote zu verschwinden.

## 2026-07-31 — Der Spieler bekommt Hände: gebundener Knüppel, Auslöser am selben Bus, sichtbarer Schaden

`doc/player-layer.md` §10 ist gebaut. Der Browser fliegt nicht mehr nur zu, er wird **geflogen** — und
zwar über exakt die Wege, die die KI benutzt. Drei Stücke:

**1. `systems/FBInputSystem` ist kein NoOp mehr.** Der Slot, den `FBSystemSlots.h` seit Anfang als
Platzhalter führte, ist eine echte Klasse mit zwei Hälften, und die Trennung IST die Klasse: die
**analoge** (Stick, Schub, Bremsklappe, Fahrwerk) verlässt sie als schlichtes `FBStickInput`, das *das
Modul* in dieselben `FBPilotCommands{Manual}` übersetzt, die sein Pilot zurückgibt — eine einzige
`ApplyPilotCommands`, zwei Sitzinsassen. Die **diskrete** (Master Arm, Stationswahl, Pickle, Abzug)
verlässt sie **ausschließlich** als `FBCommandBus::Post`. Es gibt keinen dritten Ausgang, und deshalb
ist „der Mensch bekommt kein Recht, das die KI nicht hat" hier eine Eigenschaft des Codes und kein
Versprechen. `FBModule::HumanInput()` reicht den Slot heraus und ist **null** für jedes Modul ohne
Cockpit — eine Rakete, ein Store und eine Bodenstellung haben nichts, was eine Hand halten könnte.
Sitzt ein Mensch drin, läuft der KI-Pilot **gar nicht** (nicht daneben und überschrieben): zwei Hände
an einem Knüppel wären zwei Schreiber derselben Guidance gewesen.

**2. `missions/FBOrdnance` — ein Apparat, zwei Besitzer.** Alles, was eine abgeworfene Waffe und ein
Feuerstoß TUN, nachdem sie den Jet verlassen haben, lag in `FBMissionRunner.cpp` und liegt jetzt in
einer Klasse, die der Runner und die Browser-Schleife identisch fahren: `Resolve` → `Launch` →
`SnapPoses`, in dieser Reihenfolge, weil die Reihenfolge die Semantik ist. Der Runner behält genau eine
eigene Sache, die Telemetrie-CSV je Store, über einen Haken. **Reiner Umzug, gemessen:** 12 Missionen
über Kanone/Lenkflugkörper/CCIP/CCRP/Cluster/ARM/Netz/Duell, jede `events.log` und jede
`telemetry*.csv` **bytegleich**, Exit-Codes unverändert.

**3. Gemessen im echten Browser** (Chrome for Testing über CDP, `?mission=attack-ccip`), ein Lauf:
`hotas STICK state=taken` → `gun TRIGGER burstS=0.6 rounds=510` → `sms RELEASE station=3 store=mk82` →
**`cmd CMD_REJECT seq=6 target=weapon_release reason=channel_busy`** — der Bus lehnt den Menschen ab wie
jeden anderen — → `sms RELEASE_REJECTED reason=hardware_precedence detail="master arm not in ARM"` →
`CMD_ACK gun_trigger outcome=rejected reason=hardware_precedence` → `stores IMPACT tofS=10.17`. Und
ohne eine einzige Taste: `damage DAMAGE unit=bunker zone=center fluxJm2=1962.7` + `damage SYSTEM
system=structure state=degraded`. `FBDamageModel` und `FBSystemHealth` wirken im Browser und stehen auf
dem Schirm des Spielers.

**Und der Fund, der nicht weggeräumt wurde.** Sobald Waffen im Browser existieren, wird sichtbar, dass
seine Schleife **rAF-getaktet** ist und die des Runners **fix 0,1 s** — und dass daraus aus derselben
Datei ein anderes Ergebnis wird. `cbu87-footprint`: der CCIP-Abwurf löst im Browser bei `aimMissM =
122,7 m` aus, in fb-gym bei `22,5 m`; die Kassette landet ~100 m kurz, beide Ziele liegen außerhalb des
400 × 200 m-Fußabdrucks, der Lauf endet SUCCESS **ohne** die zwei Abschüsse, die das Gym erzielt. Ein
gehaltener Abzug füllt `FBGunProjectiles`' 64 Bündel in etwa einer Sekunde, weil der Kanonen-Slot je
`Run()` ein Bündel erzeugt — also je FRAME, 60/s gegen 10/s. Das ist **Prinzip 4s eigener Fall**
(„gibt das Tempo das Ergebnis … ein Bug") und wird als `doc/clients/clients.md` 5.5 gebucht statt
umgangen: ein fester 0,1-s-Takt im Browser setzt die Kamera auf 10 Hz und ist eine eigene Runde.

Offen und benannt: kein Gamepad, keine Stick-Kraftkennlinie (die Rampe ist `kHotasLatencyS`, also die
Geschwindigkeit einer Hand, nicht das Gesetz dieser Zelle), `eng_*` bleibt für einen handgeflogenen
Sitz leer, und `FBFdmBoot` ist weiter Disziplin statt Compiler.

**Messplatz auf zwanzig Sprossen: 2 informative Zellen von 40.** Die zehn neuen spannen die drei Achsen,
die §4.2 fordert und die ersten zehn nicht hatten (Energieasymmetrie, Kräfteverhältnis, Waffenbindung),
jede allein und zusätzlich gekreuzt. Das Tor über alle 40 Zellen:

| | informativ | von |
|---|---:|---:|
| Kampagnenbreite | 1 | 154 |
| Messplatz, erste zehn | 1 | 20 |
| Messplatz, zwanzig | **2** | 40 |

Es sind `ar-08-close-day:f16` (9 Beweger, 4 Klassen bei 60,0 %, **0 Kipper von 8**) und
`ar-15-ratio-one-two:f16` (8 Beweger, 3 Klassen bei 60,0 %, **0 Kipper von 8**). S5 will drei.

**Die dritte fehlt um ein einziges Feldmitglied.** `ar-06-beam-afternoon:f16` hat ebenfalls 8 Beweger
und verteilt sich über die 15 Feldmitglieder auf 10/3/2 — Modalanteil 66,7 % gegen S1s 60,0 %. Beide
bestandenen Zellen liegen mit 9/15 **exakt auf der Schranke**; bei 15 Mitgliedern ist das der einzige
erreichbare Wert, der sie trifft (D11: `kModalMax` ist gegen ein Feld von sechs geeicht).

Die Ausbeute steht damit als Zahl da: **5 % der Zellen**, und sie ist über beide Zehnerblöcke stabil.
Der Weg zur dritten ist deshalb kein Argument, sondern eine Rechnung — und die Sicherung, die ihn von
Würfeln unterscheidet, ist hingeschrieben: **keine Sprosse wird entfernt, weil sie durchfällt.** Der
Messplatz wächst monoton, S4 zählt jede Sprosse mit, und welche informativ sind, ist seine Ausgabe und
nicht seine Eingabe.

## 2026-07-31 — Der Browser bekommt den Takt des Gyms: ein fester Takt, eine Kamera, die trotzdem läuft

`doc/clients/clients.md` 5.5 ist geschlossen, und zwar erst, nachdem der Mechanismus **überführt**
war. Zwei Kandidaten standen benannt und ungeprüft im Fund: das äußere `dt` und `FBF16Module::Due`.

**`Due` ist unschuldig.** Es entscheidet nur, WANN ein Slot läuft, und das tut es bei jedem dt richtig:
bei 1/60 s feuert ein 10-Hz-Slot jeden 6. Frame — also weiter mit 10 Hz. Schuldig ist das `dt`, das
**durchgereicht** wird. `FBPilot::Run` rechnet `TimeS_ += dt` und wird mit 10 Hz aufgerufen; bekommt es
den Frame-dt, läuft seine eigene Uhr mit `dt / 0,1` der Simulationsgeschwindigkeit. Zwei Sonden im Gym
(beide zurückgebaut) haben das gezeigt:

| Sonde | Messung |
|---|---|
| Takt im **Gym** auf 1/60 s gezwungen | `aimMissM 22,5 → 117,9 m`, `leadS 0,6 → 0,5167` — der Browser-Fehler entsteht ohne einen einzigen Browser |
| dieselbe Sonde, Pilotenuhr mitgeloggt | **Pilot bei 11,92 s, Welt bei 71,5 s** — Faktor 6,0. Der Pickle trägt einen 60 s alten Stempel, der Bus findet ihn sofort fällig, die 0,5 s HOTAS-Latenz, auf die der Pilot vorhält, passiert nie: 0,42 s zu früh × 231 m/s = 97 m kurz |
| jedem gedrosselten Slot seine EIGENE Periode statt des Frame-dt | bei 1/60 s: `aimMissM = 12,1 m`, beide Ziele tot — der Rest ist Abtastphase, kein Bias |

**Die Reparatur ist die des Clients, nicht des Moduls.** `missions/FBSimTick.h` hält die eine Zahl
(`kSimTickS = 0,1 s`); `FBMissionRunner` und die Browser-Schleife schreiten sie beide ab. Die
rAF-Schleife sammelt Wandzeit und ruft `SimTick()` — den Tick-Rumpf des Runners, Phase für Phase, als
EINE Funktion — in ganzen Vielfachen auf. Die Kamera bleibt bei der Bildrate: `EyeAt(alpha)`
extrapoliert die Augenpose zwischen den letzten zwei Ticks, und **nichts, was die Simulation liest,
hängt daran**. Der Preis steht dabei: das HUD wird vom Display-Slot mit 10 Hz erzeugt, im Manöver
stehen seine Symbole also bis zu einen Tick neben dem Gelände.

**Gemessen, gleiche Datei, gleiche Luft (`wx calm`), gleiche Maschine, headless Chrome:**

| `cbu87-footprint` | fb-gym | Browser VORHER | Browser NACHHER |
|---|---:|---:|---:|
| `aimMissM` | 22,54 m | **114,12 m** | **20,77 m** |
| `leadS` | 0,6 | 0,5167 | 0,6 |
| Ziele zerstört | 2 von 3 | **0** | 2 von 3 |

Und die Bildrate entscheidet nichts mehr: derselbe Lauf bei 60 fps und bei ~600 fps
(`--disable-frame-rate-limit`) gibt `aimMissM = 20,7715`, `alongM = 97,7188 / 157,968` — Ziffer für
Ziffer identisch, bei 10,0 Ticks/s in beiden Fällen. Der gehaltene Abzug liefert Bündel im 0,1-s-Raster
mit `1, 5, 9, 10, 10 …` Schuss — **exakt die Folge, die `gun-bfm` im Gym schreibt** — 53 Bündel in
5,3 s und **0 `BURST_DROPPED`**; vorher 128 Bündel à 1 Schuss und **185** verworfene. Gegenprobe
`attack-ccrp`: `aimLongM` Gym +38,56 m, Browser nachher +41,98 m, Browser vorher **−72,63 m** und ohne
eine einzige `damage DAMAGE`-Zeile.

**Die Regression bewegt sich nicht, und das ist beweisbar statt gemessen:** `build/fb-gym` ist vor und
nach der Änderung **bytegleich** (`909655e6…`) — die einzige Core-Änderung ersetzt das Literal `0.1`
durch die Konstante gleichen Wertes, der Rest liegt im wasm-Client, der nicht dazugelinkt wird. Die
Stichprobe aus 37 Missionen (Kanone/CCIP/CCRP/Cluster/ARM/BVR/Duell/Arena + jede 12.) über beide
Binaries: alle Artefakte identisch, alle Exit-Codes identisch.

Offen und gebucht: **5.8** — im Modul ist die Falle nur entschärft, nicht entfernt (der Slot bekommt
weiter den äußeren dt; das geradezuziehen verschiebt die 20-Hz-Slots und ist eine eigene Runde) — und
**5.9** — zwischen Browser und Gym bleiben 1,8 m Zielfehler, und die sind Gelände: zwei Abtaster
desselben DEM (Kachelraster gegen gebackenes Swiss-DEM; `--elev tiles` schließt sie bis auf 0,08 m
Aufschlagebene).

## 2026-08-01 — `E12`: die Arena besteht, die Evolution läuft, und der feste Maßstab fällt

Die erste bestandene Arena dieser Datei, die erste Evolution darauf, und eine Verweigerung durch genau
das Instrument, das dafür gebaut wurde.

**Das Tor:** `S4 60 Zellen ≥ 6 ok · S5 3 informative ≥ 3 ok · S6 0 ok · ARENA: PASSED`. Die drei sind
`ar-08-close-day:f16`, `ar-15-ratio-one-two:f16`, `ar-27-close-blue-high:f16` — je 8–9 Beweger von 24,
je ≥ 3 Ergebnisklassen bei ≤ 60 % Modalanteil, **alle drei 0 Kipper von 8**. Und S7 hat einen vierten
abgefangen: `ar-26-beam-three-two:f16` bestand S1 und S2 und wurde bei **2 von 8** verworfen — die
Falschzertifizierung, für die er geschrieben wurde, bevor er ein Ergebnis kannte.

**Keine Konstante wurde bewegt.** Die Ausbeute der Regel wurde zuerst gemessen (~5 % der Zellen), und die
Sprossenzahl folgt daraus: dreißig Sprossen, sechzig Zellen, drei informative.

**Die Evolution:** 813 Läufe, sechs Generationen, Population 45, **neun Gene und kein Blocker**. Und
**Stufe M entscheidet in jeder Generation 239 bis 308 Vergleiche** — in `E8` waren es null. Die Fitness
benotet endlich das, wofür sie gebaut ist.

**Und dann verweigern die Instrumente.** Drei verschiedene Champions, die sich in **genau einem Gen**
unterscheiden — dem Abwurfvorhalt: 0 → −0,625 → −0,3125, alles andere identisch. Der feste Maßstab dazu:
0,611 · 0,611 · 0,611 · 0,611 · 0,611 · **0,444**. Die mitlaufende Fitness des letzten Champions ist mit
0,652 die höchste, sein Wert gegen das eingefrorene Feld der niedrigste. Das ist die Signatur, vor der §6
warnt, und es wird **kein §1 veröffentlicht**.

Die Schwäche von (b) steht dabei, statt dass ich mich darauf stütze: bei drei verschiedenen Champions gibt
es genau **ein** auswertbares Tripel, T = 1,0000 ruht also auf n = 1. Tragend ist (a) — das feste Feld ist
eingefrorener Text, den kein Genom beeinflussen kann, und es ist gefallen.

Und die Falle, in die diese Runde nicht getreten ist: bei Generation 4 aufzuhören hätte über das ganze
Fenster flache 0,611 gezeigt und (a) bestanden. Das Fenster zu wählen, nachdem man die Kurve kennt, ist
derselbe Griff wie eine Schwelle nachzuziehen — abgelehnt, zum neunten Mal.

## 2026-08-01 — `E13`: die erste veröffentlichte Doktrinverschiebung

`E12` hatte das Tor bestanden und den eigenen Champion verworfen, weil der Läufer nach der mitlaufenden
Population auswählte — einem Maß, das §6 zu veröffentlichen verbietet. **E19** hat die Auswahl auf das
feste Feld umgestellt, geprüft *vorher*, dass es nichts kauft. Derselbe Lauf, dasselbe Genom, dieselben
813 Läufe:

| Instrument | Wert | |
|---|---|---|
| (a) fester Maßstab | 0,722 → **0,889** → 0,889 … | steigt, nicht fallend — **ok** |
| (b) zyklische Tripel | T = 0,0000 (0 von 10) | **ok** |
| (c) Trajektorie | 0,2795 / 0,1288 / 0,0442 | bewegt sich |
| X1 / X2 / X3 / X4 | PASS / n.z. / PASS / PASS | |

**Die Verschiebung: ein vernetzter Vierer halbiert seine Höhenstaffelung — `pilot_flight_stack_frac`
1,5 → 0,75.** Kein Gen liegt auf einer Schiene.

**Der Mechanismus, in publizierten Kanälen:** der Wert skaliert `FormationStackM` in
`FBPilot::FormationStation` (`altM = lead.AltM + k · stackM`), die Verbandsmitglieder sitzen enger
beieinander in der Höhe, und **`flt_switch` — die Zahl der Umsortierungen — fällt auf allen drei Zellen**
(83→77, 66→39, 11→7). Auf zwei von dreien wird daraus eine höhere Zielzahl.

**Und das Datenlink-Bit ist NICHT der Mechanismus:** `dl=on` allein lässt jeden Kanal und beide
entscheidenden Stufen identisch. Die Verschiebung ist die Staffelung.

**Die ehrliche Grenze steht beim Befund:** auf `ar-15` kostet die halbierte Staffelung allein ein Ziel
(M 13 → 12) und hebt `eng_shots` von 1 auf 6; erst die zwei anderen Gene des Champions holen es zurück.
Der Kanal bewegt sich auf allen dreien in eine Richtung, **das Ergebnis nicht** — „weniger Churn ist
besser" wird deshalb *nicht* als Gesetz veröffentlicht.

Zwei Offenlegungen gehören zum Audit: `ar-08` liest 1 von 8 grob und **4 von 24 = 16,7 %** fein — unter
§5s 25-%-Boden, aber näher dran als die grobe Zahl vermuten lässt, weshalb die Behauptung auf den zwei
Zellen ruht, die null Mal kippen. Und `fb_champion_audit.py` hat bis heute S7s **Zulassungsschwelle**
(null Kipper) auf einen Champion angewandt; §10/E18 hatte die Unterscheidung im Voraus aufgeschrieben,
und das Werkzeug benutzt jetzt §5s Boden mit diesem Zitat im Quelltext.

**Nachtrag `E13`: die Validierung widerlegt die Übertragung.** §1 wurde unter §5s Tests verdient, und
X1 verlangt nur, dass der Vorteil auf *„the arena's other informative geometries"* hält — davon gibt es
drei. Also wurde die Verschiebung auf den Satz getragen, auf dem sie **nicht** ausgewählt wurde: die 154
Kampagnenzellen.

| Champion gegen seine Saat, Kampagnenbreite | |
|---|---:|
| besser | **17** |
| schlechter | **38** |
| gleich oder unvergleichbar | 99 |
| `flt_switch` fällt / steigt / gleich | 6 / 9 / 139 |

**Die Verschiebung überträgt sich nicht, und auf den Kampagnen ist sie netto schädlich.** Der Mechanismus
ebenso wenig: die Umsortierungen fallen auf 6 Zellen und **steigen auf 9**, gegen drei von drei auf dem
Messplatz.

§1 bleibt veröffentlicht — es wurde unter den Tests verdient, die diese Datei deklariert. Was es **nicht**
ist, ist eine Doktrin für diesen Baum: es ist eine Doktrin für Nahbereichs-Vierer der Bauart, aus der der
Messplatz besteht. Der Auftrag selbst spricht das Urteil: *„über die Kampagnenbreite, nicht über einzelne
Geometrien."* Daran gemessen fällt die Verschiebung durch, und dieser Fehlschlag ist die wertvollere
Hälfte der Runde.

Und er benennt eine Instrumentenschwäche, die keine frühere Runde sehen konnte: X1 ist auf einer
Drei-Zellen-Arena fast leer — „mindestens zwei von dreien" ist ein Zwei-Proben-Test. Als **E-25** gebucht,
und **nicht** nachträglich in §5 eingebaut: ein Kriterium, das in der Runde umgeschrieben wird, deren
Ergebnis es ändern würde, ist keins.

**Korrektur zu `E13`, und sie schärft den Befund erheblich.** §7a hat den *letzten* Champion auf der
Breite gemessen — der richtige Test am falschen Genom. Alle fünf Champions des Laufs, auf dieselben 154
Zellen getragen (924 Läufe):

| Champion | fügt hinzu | Arena | Breite besser/schlechter | Netto |
|---|---|---:|---|---:|
| `g0_s3` | `dl=on` | 0,722 | 0 / 0 | **+0** |
| **`g1_61`** | **`stack` 1,5 → 0,75** | **0,889** | 14 / 15 | **−1** |
| `g2_51` | — | 0,889 | 14 / 15 | −1 |
| `g4_21` | `bias_s` −0,625 | 0,889 | 18 / 37 | −19 |
| `g5_40` | `trail` 1,125 | 0,889 | 17 / 38 | −21 |

**Das Kerngen der Verschiebung ist auf der Breite neutral** — die halbierte Höhenstaffelung kostet eine
Zelle von 154 und hebt den Arena-Maßstab von 0,722 auf sein Maximum. `dl=on` allein ist auf allen 154
exakt neutral, was auf der Breite bestätigt, was die Arena schon zeigte: das Netz ist nicht der
Mechanismus.

**Der Schaden ist ausschließlich das, was die Suche danach hinzufügte.** Ab Generation 1 steht der
Arena-Maßstab **flach** bei 0,889 — die Arena hat nichts mehr zu sagen —, und die Suche lief weiter.
Ein flacher Maßstab ist kein Plateau zum Überqueren; er ist die Arena, die schweigt, und der Spaziergang
wurde auf 154 Zellen bezahlt, die sie nie angesehen hat.

§1 nennt jetzt `g1_61`. Die drei späteren Gene sind aus der Behauptung zurückgezogen und stehen mit ihrer
Messung da — die Regel dieses Baums für einen verworfenen Ansatz.

**`E13` §7c: die Selektion auf der Breite — und sie zeigt in die Gegenrichtung.** Die Achse, die §4s
Mechanismus benennt, ist über alle 154 Zellen abgetastet worden (1.078 Läufe, alles außer der
Höhenstaffelung auf der Saat):

| `stack_frac` | besser | schlechter | netto | Test |
|---:|---:|---:|---:|---|
| 0 | 11 | **21** | −10 | **p ≈ 0,025 für „schlechter"** |
| 0,25 | 13 | 18 | −5 | |
| 0,5 | 12 | 18 | −6 | |
| **0,75** (veröffentlicht) | 14 | 15 | **−1** | p = 0,644, also nichts |
| 1,0 | 9 | 18 | −9 | |
| **2,0** | **17** | 10 | **+7** | p = 0,124 — **nicht signifikant** |

**Belegt ist nur das schädliche Ende:** die Staffelung ganz zu kollabieren ist auf der Kampagnenbreite
messbar schlecht. Dass eine *weitere* Staffelung besser wäre, sieht so aus, ist aber bei 17 von 27
bewegten Zellen nicht belegt — nötig wären 19.

**Die Überschrift ist die Uneinigkeit.** Die Arena wählte 0,75 und erreichte dort ihr Maximum; der beste
Wert der Breite ist **2,0, die Gegenrichtung**, und die Wahl der Arena ist dort exakt neutral. Die beiden
Messplätze unterscheiden sich nicht bloß in der Stärke — **sie zeigen auf demselben Gen in
entgegengesetzte Richtungen.**

§1 mit 2,0 neu zu veröffentlichen wäre eine Auswahl auf p = 0,124 und damit derselbe Griff wie eine
nachgezogene Schwelle. Abgelehnt; der Wert steht hier mit seiner Messung.

## 2026-08-03 — der zweite Determinismus-Nachweis, über sechs Architekturänderungen hinweg

Der erste Nachweis (2026-07-31) lief vor F5, D3a und G5. Seither sind **sechs Architekturänderungen** in
den Baum gegangen: die Verbandsform als Missionsdaten (F5), `CanPressOn` gegen ein Bild statt einen
Sender (D3a), das Emissionsmanagement des Piloten (G5), der feste Simulationstakt im Browser, die
Herauslösung der Waffenapparatur aus dem Missionsläufer, und das Cockpit mit drei MFDs.

Der Nachweis ist deshalb **vollständig wiederholt**:

| | erster Nachweis | zweiter Nachweis |
|---|---|---|
| Kampagnen | 11 | **12** (`ar-arena` ist dazugekommen) |
| Läufe | 99 = 11 × 3 Wiederholungen × Threads 1/2/4 | **108** |
| Fingerabdrücke je Kampagne | genau einer, 11 von 11 | **genau einer, 12 von 12** |
| Einzelnachspiele | 104, null Divergenzen | (Kriterium 2 steht aus) |
| rc | 0 auf allen | **0 auf allen** |

**Die Fingerabdrücke selbst haben sich bewegt** — das müssen sie, denn sechs Änderungen haben Verhalten
verschoben und jede ist einzeln gemessen und begründet. Was sich **nicht** bewegt hat, ist die
Eigenschaft: dieselbe Kampagne gibt über drei Threadzahlen und drei Wiederholungen denselben
Fingerabdruck, vor den Änderungen wie danach. Determinismus ist damit nicht für einen Stand belegt,
sondern **über einen Architekturwechsel hinweg**.

## 2026-08-03 — Der Browser fährt die Simulation nicht mehr selbst: eine Schleife, ein Urteil, ein Ende

Der Eigner hat den Defekt selbst gesehen: *„ich habe mir eben eine mission angeschaut und der flieger
ist einfach in den berg geflogen und die simulation lief weiter."* Der Befund war exakt das, was er
beschreibt. `missions/FBMissionRunner.cpp` trug die Abbruchregel im Kopf seiner `while`-Schleife;
`clients/FBAppWasm.cpp` hatte sich eine ZWEITE Schleife geschrieben (`SimTick()`), rief darin
`RunMonitors` und **verwarf den Rückgabewert**. Kein `FirstFlightKo`, keine Urteilsprüfung, kein
Timeout, kein Ende: der Flugmonitor schrieb seine `monitor KO`-Zeile und der Browser integrierte weiter,
solange der Tab offen war.

**Die Reparatur ist nicht eine Prüfung mehr im Browser.** Der Eigner hat die Form vorgegeben — *„gui darf
nur ein client auf die simulation sein"*, *„das muss strukturell unmöglich sein"* —, und daraus wurde:

| Struktur | Wirkung |
|---|---|
| `missions/FBMissionSim` — ein Objekt, das den Tick UND den Lauf-Zustand besitzt | der Läufer ruft `RunToConclusion()`, der Browser `Advance(dt)` |
| `Tick()` ist **privat** | kein Client kann einen Einzelschritt tun, also keine zweite Schleife schreiben |
| `FBRunState` ist `[[nodiscard]]` **am Typ** | ein Client, der das Urteil fallen lässt, übersetzt nicht — gegengeprüft: `FBAppWasm.cpp:374: error: ignoring return value ... [-Werror,-Wunused-value]` |
| `units/FBSimUnit`s Tick-Fläche privat, genau ein Friend | dieselbe Familie wie `FBFdm`s Konstruktor und `FBSystemHealth`s Mutatoren |
| `make -C sim verify-guards` | acht Zwei-Zeilen-Übersetzungseinheiten, **sechs müssen scheitern**, zwei müssen gelingen |
| `verify-layers` druckt **`1 simulation-loop driver(s)`** | wäre die Zahl vorher gedruckt worden, hätte sie 2 gesagt |

**Und das K.O. ist keine Flugzeugfrage mehr.** Auf den Einwand *„module/units können auch panzer,
drohnen, helikopter, schiffe sein"* und die Korrektur *„das muss das schadenssystem machen"* /
*„k.o. ist reine physik"* fragt die Schleife jetzt **„lebt dieser Akteur noch"**:
`core/FBSystemHealth::Destroyed()`. Geschrieben wird das Bit ausschließlich durch den einzigen Friend
des Registers, über die neue Tür `FBDamageModel::ApplyPhysicalKo` — die **nichts entscheidet**, sondern
aufschreibt, was `core/FBFlightMonitor` gemessen hat. Keine Trefferpunkte, keine Lebensenergie, keine
Zahl ohne Herleitung: die Schwellen sind die des Monitors (Bodenkontakt, Strukturkontakt, Durchdringung
unter `kPenetrationMarginM = −3 m`, Divergenz). Der Beobachter stellt fest, das Register verwahrt, die
Schleife liest — und ein künftiger Panzer erbt die Regel, weil er Gesundheit hat und nicht, weil ihn
jemand in eine Liste einträgt.

**Belege.** (1) Im echten Browser (Chrome for Testing, WebGPU, CDP-Mitschnitt), neue Mission
`missions/cfit-oberland.fbm` — ein Schenkel auf fester Höhe ins Berner Oberland:
`t=40.3 ERROR monitor KO reason=CFIT detail="ground penetration"`, unmittelbar gefolgt von
`t=40.3 INFO mission RESULT result=CRASH`. Danach **12 s weitere Wanduhr** mit `simTicksPerS=0`,
`substepsPerFrame=0` bei 46–60 Bildern/s — das Bild läuft, die Simulation steht (Mitschnitt und
Bildschirmfoto: `sim/build/proof-cfit/`). Das Debriefing der
Spieler-Schicht öffnet sich von selbst (es hörte schon immer auf diese Zeile) und zeigt
`judge verdict CRASH / reason ground penetration / concluded at 40.3 s / physical K.O. CFIT`.
(2) Dieselbe Datei in `fb-gym`: Exit 2, `CRASH`, `ground penetration`, `t=39.9` — die 0,4 s sind die
Geländequelle (Browser: gestreamter Kachel-Raster, Gym: gebackenes Schweiz-DEM), Lücke 5.9, nicht die
Regel. **Nebenbefund, neu aufgeschrieben:** zwei Browserläufe derselben Datei geben `t=40.3` und
`t=40.2` — `fb_stream_ground` antwortet aus der gerade geladenen LOD-Stufe, also ist das Gelände unter
dem Flugzeug eine Funktion des STREAMS und nicht der Mission (Lücke 5.9b). Alles darüber — Tick,
Richter, Urteil — ist deterministisch; genau an diesem Sampler hört es auf. (3) Regression über alle 281 Missionen, `tools/fb_regress.sh` mit dem Stand davor und danach:
**281 verglichen, 0 Telemetrie-Hashes bewegt, 0 `events.norm` bewegt, 0 Exit-Code-Unterschiede.**
Determinismus über `--threads 1/2/4` auf `pair-2v2-f16`, `payerne-full` und der neuen Datei: je ein
Fingerabdruck. Sieben Harnesses rc=0, `verify-models` grün, `verify-layers` grün mit
`6 registry reader(s) … 1 antenna-cue poster(s) … 1 simulation-loop driver(s)`.

## 2026-08-03 — Die taktische Karte: das eigene Netz, ein Kraftbild am Leitknoten, APP-6 über OSM, und Befehle, die abgelehnt werden können

**Vorbedingung zuerst, und sie war eine Missionsfrage.** 78 Missionen deklarieren ein `net`, und der
Leitknoten aller 78 saß auf der `hostile`-Seite; die Fähigkeit war seit dem ersten Tag generisch, was
fehlte, war ein Mitglied mit Flügeln. `modules/FBAirNet.h` beantwortet jetzt die runner-erzeugten
`net_*`-Schlüssel für beide Zellenfamilien identisch: `net_link`/`net_period_s`/`net_hold` gehen an das
Terminal, das der Jäger ohnehin trägt — **ein Jäger braucht kein zweites Funkgerät, um in einem
Leitnetz zu sein** —, `net_control` macht ihn zum Hörer, `net_wcs` zum Knoten, `net_autonomy` zum
Rückfall. `net_sector` und `autonomy tight` werden mit BEGRÜNDUNG ABGELEHNT statt still ignoriert: ein
Verantwortungssektor gehört einer Stellung im Boden, und `tight` bräuchte eine Zieladressierung, die
dieser Baum nicht hat. Zwei neue Dateien, keine bestehende angefasst: `missions/map-friendly-net.fbm`
und `missions/map-emcon-gap.fbm`.

**Das Bild ist `core/FBForcePicture` und liest AUSSCHLIESSLICH publizierte Blöcke** — ein `Ingest`
(FBState + Pose) je Beitragendem, sechs Quellen, APP-6-Zugehörigkeit. `verify-layers` zählt weiterhin
**sechs** Wahrnehmungsleser und **einen** Schleifentreiber; das ist die Abnahme von P9 und keine
Zusicherung. **HOSTILE wird nirgends abgeleitet**: eine PPLI ist FRIEND, ein Echo mit gültiger
Mode-4-Antwort ist FRIEND, alles andere UNKNOWN. Eine RWR-Peilung ist eine LINIE ohne Ende, weil kein
Pfad im Baum ihr eine Entfernung geben könnte.

**Gezeichnet wird im HUD-Pass, und die Karte ERSETZT dort die Cockpit-Symbolik** — ein
`FBHudGeometry`, ein `FBHudStage`, **null zusätzliche Begin\*Pass je Frame**. Das erzwingt §9.7
nebenbei baulich: beide Bilder können nicht gleichzeitig auf dem Schirm stehen, also kann keines die
Quelle des anderen lesen. Die OSM-Grundkarte IST der Geländerenderer aus dem Nadir, auf genau der Höhe,
deren Fußabdruck die Kartenspanne ist (`H = span / (2·tan 30°·aspect)`) — **[MESS] mit −89,9° und nicht
−90°**: `FBCameraBasisEcef` bildet `right` aus `fwd × Welt-oben`, im exakten Nadir sind die parallel,
die Basis kollabiert und das Bild kam LEER zurück, bei 22 gezeichneten Geländeblättern.

**Ein Befehl wird von der BOX beendet, nicht vom Absenden.** Der erste Entwurf hat aus dem
`Pending`-Rückgabewert von `Post()` eine Annahme gemacht — und die Box hat den Eintrag vier Sekunden
später abgelehnt, also stand auf der Karte „angenommen“ über einem gescheiterten Eintrag. `FBCommandBus`
hat jetzt `AckOf(seq)` (Ring über die letzten 16 Antworten), und der Pilot hält den Befehl, bis SEIN
Eintrag beantwortet ist: gemessen `order ISSUE` t=61,0 → `CMD_ISSUE steerpoint class=ded dueS=65,1` →
`CMD_ACK accepted latencyS=4,1` → `order ACCEPTED phase=Intercept` t=**65,1**.

**Eine Grenze, die diese Runde ENTDECKT und nicht geerbt hat:** `FBAirPilot::Run` kürzt für einen
kreisenden Mover die Phasenmaschine ab — der Befehlseingang der Basis lief also nie, und ein Befehl an
die AWACS **verschwand ohne eine Zeile**. `ConsumeOrders` ist jetzt protected und dieser Zweig ruft es.
Eine Ablehnung, die der Befehlshaber nicht sieht, ist schlimmer als eine Ablehnung.

**Zwei Projektionsdefekte, beide gemessen und beide von der Sorte, die ein plausibles Bild über den
FALSCHEN Boden legt.** (a) −90° Nick lässt die Kamerabasis kollabieren (s.o.). (b) Der Kartenmittelpunkt
ist **`ViewH/2` und nicht `Height/2`**: die Szene ist um das Drittel der MFD-Bank nach oben verschoben,
die erste Fassung legte also jedes Symbol 120 px — bei 46 km Spanne **4,1 km** — südlich seines eigenen
Bodens. Nachgemessen über den skaleninvarianten Fixpunkt zweier Bilder desselben Ticks bei 6 km und
12 km Spanne: **cy ≈ 226…240** (maskierte Korrelation 0,363/0,333) gegen **0,159 bei cy = 360**. Die
Restunsicherheit ±15 px ist die Auflösung des Verfahrens und als offener Punkt vermerkt.

**Gemessen** (`build/tactical-map/`, `map-emcon-gap.fbm`, Knoten `magic`, die ganze Fraktion beim Spawn
funkstill): `A-before-first-contact-t35.7.png` zeigt **CONTACTS 0** — der Gegner ist 15 km im Bild und
steht NICHT auf der Karte; das Ereignis, das es ändert, ist `t=40.5 radar RADAR_CONTACT unit=vip1
track=1`, und `B-after-first-contact-t45.9.png` zeigt **CONTACTS 1** mit einem Vierpass `NET 2S`.
`C-aged-contact-t76.5.png` zeigt ein 12,4 s altes Datum: auf 40 % gedimmt, mit gestricheltem Ring
`Alter × 250 m/s`. Sechs Befehlsausgänge in einem Lauf, darunter drei Ablehnungen
(`nothing_held`, `no_capability` zweimal). **Regression: 282 Missionen verglichen, 0 Telemetrie-Hashes
bewegt, 0 `events.norm` bewegt, 0 Exit-Code-Unterschiede** (`tools/fb_regress.sh`, Stand davor gegen
danach, nur Hashes, Schnappschüsse danach gelöscht); die einzige Differenz sind die zwei NEUEN Dateien.
Determinismus `--threads 1/2/4` je ein Fingerabdruck pro neuer Mission. Zehn Harnesses rc=0,
`verify-layers`/`verify-guards` (8/8)/`verify-models` grün, `wasm`/`native`/`gym` warnungsfrei.

## 2026-08-03 — `E14`: eine Doktrinverschiebung über die KAMPAGNENBREITE, p = 0,0005

`E13` hat auf der Arena selektiert und die Validierung hat die Übertragung widerlegt. Diese Runde fragt
andersherum: **nicht „was wählt die Arena", sondern „was bewegt die 154 Kampagnenzellen".** Die Achsen
werden nicht gesucht, sondern abgetastet — es sind die vier Genfamilien, die `E11` als breit wirksam
gemessen hat, jede für sich, alles andere auf der Saat.

**Die Verschiebung: der Pilot drückt einen Entscheidungstakt früher — `pilot_attack_bias_s` 0 → −0,2 s.**

| Genom | besser | schlechter | netto | p |
|---|---:|---:|---:|---|
| **`bias-early` −0,2 s** | **25** | 9 | **+16** | **0,005** |
| `bias-late` +0,2 s | 1 | **32** | −31 | dieselbe Achse, andersherum |
| `stack-wide` | 17 | 10 | +7 | 0,124 |
| `net-on` | 0 | 0 | ±0 | auf allen 154 inert |

**Eine Achse, zwei Richtungen, beide sprechen.** Keine andere Genfamilie kommt in die Nähe.

**Und nach dem Chaos-Schirm wird es stärker.** Alle **34** bewegten Zellen wurden über §5s 0,8-m-Gitter
geschickt — **in beide Richtungen, blind dafür, wem sie nützen**. Sechs sind chaotisch und fallen raus:
**23 besser, 5 schlechter, netto +18, p = 0,0005.** Der Schirm hat mehr schlechte als gute Zellen
entfernt — die Richtung, in die ein Schirm einen echten Effekt bewegt, und das Gegenteil dessen, was er
mit einem zufälligen tut.

**Der Mechanismus stand schon vorher im Baum**, als X-2, gemessen auf acht Angriffszellen über vier
Kampagnen, zwei Waffen und vier Höhen: das Minimum von `aimErrM(bias)` liegt auf **jeder** von ihnen bei
−0,20 ± 0,05 s — eine konstante **ZEIT**, nicht eine konstante Entfernung, und genau das sagt, dass es
eine **Latenz** ist.

**Und damit ist es keine Taktik, sondern ein Defekt unserer eigenen Vorgabe** — `AttackReleaseBiasS()`
gibt 0,0 s zurück. Diese Runde ist X-2s Urteil auf 154 Zellen statt auf acht. Die richtige Reparatur ist
deshalb nicht, eine Doktrin zu veröffentlichen, sondern dem Flugzeug **seine eigene Zahl zu geben**,
hergeleitet aus der Latenz, die es ohnehin kennt. Dass das hier nicht geschehen ist, ist die Regel des
Baums: eine Modellzahl braucht einen Beleg, und ein besseres Missionsergebnis ist ausdrücklich keiner.
Gemessen ist die **Größe** des Defekts und sein **Vorzeichen**; geschuldet bleibt die Herleitung.

---

## 2026-08-03 — `E15`: die geschuldete Herleitung, und sie halbiert `E14`s Zahl

`E14` hat eine Doktrin gemessen und nicht veröffentlicht, weil ihr der Beleg fehlte. Diese Runde holt ihn
— nicht aus einem weiteren Lauf, sondern aus der **Kette selbst**. Vom Entschluss bis zum Store, in
Zeitstempeln aus `attack-ccrp`:

```
t=71.4  CMD_ISSUE   dueS=72.0     ttrS=0.531  leadS=0.6  biasS=0
t=72.0  sms RELEASE               (der SMS nimmt an und reiht ein)
t=72.1  stores SEPARATION         (der Store wird eine Einheit)
```

| Glied | Quelle | s |
|---|---|---|
| Lesen → Betätigen | `FBPilot.h` `DecisionDtS_` | 0,1 |
| Betätigen → Quittung | `FBCommandBus.h` `kHotasLatencyS` | 0,5 |
| Quittung → Trennung | **war unkompensiert** | **0,1** |
| **Kette** | | **0,7** — der Pilot hielt 0,6 vor |

**Das dritte Glied ist kein Tuning-Wert, sondern die Warteschlange zwischen `FBStoresSystem::Release`
und `FBOrdnance`s Entleerung.** Und es ist nicht empirisch, sondern **hergeleitet**: `FBMissionSim::
RunPhases()` fährt `StepActors()` (dort fällt die Freigabe an) vor `SimT_ += dt` und `Ordnance_.Launch`
danach — die Trennung liegt per Konstruktion einen Tick später, und derselbe Absatz nennt das als
Absicht („a round is never resolved in the tick it left the rail"). Gemessen wurde die Herleitung nur
noch bestätigt: konstant 0,1 s über `attack-ccrp`, `attack-ccip`, `cbu87-footprint`, `w2-01-dome`. Ein
`static_assert` in `FBOrdnance.cpp` pinnt `kSeparationDelayS` an `kSimTickS`, damit die Zahl nicht
wegdriften kann.

**Vorhergesagt, bevor gebaut wurde:** ein Tick × 231,5 m/s = 23,2 m. **Gemessen am echten Bodendurchgang**
(`stores IMPACT crossLat/crossLon`, nicht an der Vorhersage des Rechners): **+38,8 m → +16,0 m lang, ein
Gewinn von 22,8 m** — 1,5 % neben der Herleitung, der Rest ist die Geschwindigkeitsänderung über den Tick.

**Damit zerfällt `E14`s −0,2 s sauber in zwei Hälften.** −0,1 s war dieser Defekt und ist jetzt
strukturell aus jedem Flugzeug verschwunden. Die anderen −0,1 s waren nie eine Doktrin: sie sind ein
Bias, der eine halbe Breite von `X-3`s Auslösetakt im Mittel wegbügelt — und der Restfehler dieser Runde
(16,0 m = 0,069 s) liegt genau in dessen Treppe. **Hätte `E14` veröffentlicht, stünden ein Defekt und ein
Partitionsartefakt gemeinsam als eine Tuning-Zahl in jedem Flugzeug jeder Geometrie.** Die Weigerung war
richtig, und die Messung, die sie weigerbar machte, hat den Defekt lokalisiert.

**Die MiG bekam denselben Fix — und die Messung hat ihn widerlegt.** Ihr Director zählt einen Countdown
ab statt vorzuhalten, durchläuft aber dieselbe Warteschlange, also schien das Argument zu übertragen. Es
überträgt nicht: die Regression nahm fünf `o3-*`-Missionen von Exit 0 auf 3, und die Ursache stand in
`o3-01-unopposed` — `aimLongM` **−45,5 m → −68,2 m**, also von *zu kurz* auf *noch kürzer*, exakt ein Tick
in die falsche Richtung. **Der MiG-Abwurf war nie zu spät, sondern schon vorher 0,194 s (45,5 m bei
234 m/s) zu FRÜH.** Zurückgenommen, `o3-01-unopposed` fliegt wieder bitgleich zum alten Stand.

Das ist ein eigener, offener Befund und kein Nebensatz: **derselbe Betrag von ~0,19 s taucht hier auf wie
in `C28`**, nur mit umgekehrtem Vorzeichen und auf einem anderen Pfad. Ob das dieselbe Ursache ist, weiß
ich nicht — und eine Zahl ohne isolierte Ursache ist kein Fix. Gebucht, nicht repariert.

**Ein Fix, der nicht besser macht — das muss so gesagt werden.** Auf `w1-06-strike-escort` wandern zwei
Abwürfe von **+10,9 und +17,4 m zu lang auf −12,2 und −5,7 m zu kurz**: beide um 23,1 m, einen Tick, und
der erste wird dabei geringfügig **schlechter** (13,1 → 14,2 m). Das ist `X-3`s Gitter: die Auslösung
kann nur auf 23-m-Stufen landen, und wer einen systematischen Versatz korrigiert, verschiebt jeden Abwurf
um eine Stufe — unabhängig davon, wo auf der Stufe er saß. **Die überprüfbare Behauptung ist deshalb die
über die VERTEILUNG, und sie stand vor der Messung fest:** der Mittelwert von `aimLongM` muss um etwa
einen Tick fallen, die **Streuung darf sich nicht ändern**. Werkzeug: `tools/fb_delivery_stats.py`.

**Und warum kompensiert statt strukturell verhindert.** Der Versatz ist geometrisch: `FBOrdnance::Launch`
spawnt mit `carrier.State()`, also der **live**-Lage einen Tick später. Der Baum enthält das immune
Muster eine Datei weiter — `FBGunBurst` trägt `LatDeg/LonDeg/AltM/VelE…/SimTimeS` beim Auslösen mit, und
deshalb kostet die Kanone dieselbe Warteschlange nichts. Dasselbe für `FBStoreRelease` zu tun **wurde
verworfen**, weil beides, was dafür brechen müsste, Absicht ist: `FBStoresSystem` fasst bewusst keinen
FDM an, und `Launch` steht bewusst hinter der Posen-Barriere („a round is never resolved in the tick it
left the rail"). Zwei absichtliche Entscheidungen erzeugen den Tick zwischen sich. **Der Preis wird
genannt statt versteckt: eine Kompensation überlebt eine Änderung an Phasenordnung oder Taktrate nicht** —
weshalb `kSeparationDelayS` per `static_assert` an `kSimTickS` hängt und nicht wegdriften kann, ohne den
Build zu brechen.

**GEMESSEN, über den ganzen Baum.** 284 Missionen auf beiden Ständen, 259 Abwürfe paarweise, 9 als
Nicht-Abwürfe ausgeschlossen (der Store kam kilometerweit vom Zielpunkt herunter), bleiben **250**.

| | alt | neu | Differenz |
|---|---:|---:|---:|
| Mittel `aimLongM` | +43,15 m | +23,50 m | **−19,66** |
| Streuung | 56,78 | 52,76 | −4,02 |
| Mittel `aimAcrossM` | +26,76 m | +26,66 m | −0,10 |

**Median-Verschiebung je Abwurf: −23,10 m** gegen 23,2 m hergeleitet. Der Längsversatz wandert, die
Streuung bleibt, quer bewegt sich nichts — und quer war die Zahl, die den Fix hätte widerlegen können.
212 von 250 bewegt, 199 näher, 13 weiter weg: das Gitter, wie angekündigt.

Drei Missionen wechseln den Exit-Code, alle 3 → 0, jede einzeln begründet: `w2-01-dome` und `w2-08-flak`
36,38 → 13,29 m bei Zielen 1 → 2, `w3-03-weasel-close` 23,59/21,48 → 15,06/16,50 m bei 7 → 8. **Das ist
Folge, nicht Beleg** — die Zahl stand vor dem ersten Flug fest.

**Offen bleibt der Rest:** der Mittelwert ist nach dem Fix immer noch **+23,50 m lang**, also ein weiterer
Tick. Nicht als Defekt gebucht, weil es ein Mittel über eine gemischte Population ist (die MiG wirft
systematisch KURZ, `C29`) — aber hier notiert, damit die nächste Runde bei der Zahl anfängt und nicht bei
dem Eindruck, diese hier sei fertig geworden.

---

## 2026-08-03 — Die Basislinie der zehn Kampagnen, und der erste Verbesserungsnachweis über ihre Breite

Alle zehn Kampagnen existierten, aber es gab keine Zahl, gegen die „besser" messbar gewesen wäre.
Jetzt gibt es sie: **alle zehn laufen 10 von 10 Missionen durch**, keine bricht ab.

| Kampagne | S | T | F | C |
|---|---:|---:|---:|---:|
| w1-red-flag | 3 | 7 | 0 | 0 |
| w2-osirak | 7 | 0 | 2 | 1 |
| w3-desert-storm | 2 | 8 | 0 | 0 |
| w4-allied-force | 1 | 9 | 0 | 0 |
| w5-baltic-qra | 9 | 1 | 0 | 0 |
| o1-bekaa-1982 | 0 | 9 | 0 | 1 |
| o2-pvo-intercept | 4 | 6 | 0 | 0 |
| o3-yom-kippur-1973 | 5 | 4 | 0 | 1 |
| o4-gaf-mig29g-dact | 1 | 4 | 5 | 0 |
| o5-airfield-defence | 1 | 7 | 2 | 0 |
| **gesamt** | **33** | **55** | **9** | **3** |

**TIMEOUT ist bei der Mehrzahl die erwartete Form** und darf nicht als Fehlschlag gelesen werden —
`w1-04` sagt es wörtlich. Die harte Menge sind die 9 FAIL und 3 CRASH, und sie ballen sich:
**`o4-gaf-mig29g-dact` trägt allein 5 von 9** — ausgerechnet die einzige Kampagne, in der beide
Piloten-KIs direkt gegeneinander fliegen. Über die Seiten: 22 SUCCESS auf 50 F-16-Missionen gegen
11 auf 50 MiG-Missionen, Faktor zwei, und `o1-bekaa-1982` hat null.

**Ein Absturz gelesen statt gezählt.** `w2-04-loaded` endet mit Bodenkontakt bei eingefahrenem Fahrwerk.
Die bindende Leseregel verlangt drei Dinge in fester Reihenfolge, und sie ergeben: `fuelLbs` in der
letzten Zeile **0,000000**, beide `nav WP_REACHED` vorhanden. Der Jet hat beide Wegpunkte erreicht und
ist danach trocken heruntergekommen — die Regel hatte den Fall vorweggenommen (*„a run that does not get
home has answered the campaign's central question in the negative"*). Die zentrale Frage des
Osirak-Angriffs ist die Reichweite, und die gemessene Antwort ist **nein**. Kein Simulationsfehler.

**Der erste Verbesserungsnachweis über die Kampagnenbreite.** Der E15-Fix (der unkompensierte
Trennungs-Tick im Abwurf-Vorhalt) wurde gegen den Stand davor gefahren — beweisbar-minimal, weil die
284-Missions-Regression genau drei bewegte Missionen ausgewiesen hatte und alle drei in w2 und w3 liegen:

| Kampagne | vor E15 | nach E15 |
|---|---|---|
| w2-osirak | `TSSCSFSTSF` | `SSSCSFSSSF` |
| w3-desert-storm | `STTTTTTTTT` | `STSTTTTTTT` |

**Drei Missionen von TIMEOUT auf SUCCESS, null in die Gegenrichtung**, jede andere Position identisch.
Das ist das, was der Auftrag als Produkt verlangt — keine bessere Zahl, sondern eine erklärbare
Verschiebung, und die Erklärung steht in E15: der Abwurf lag systematisch einen Tick zu lang.

**Und dabei ist ein Instrument aufgefallen, das in die Gegenrichtung zeigt.** `w2-osirak` wurde in jeder
Mission besser oder blieb gleich — und sein Kampagnen-Exit fiel von 3 auf 2. Ursache:
`FBCampaignRunner` bildet `worst = max(exitCode)`, und die vier Codes sind ein NAMENSSCHEMA, keine
Schweregradskala; `max` erklärt damit TIMEOUT zum Schlimmsten, während dieselben Missionsköpfe TIMEOUT
als erwartete Form führen. **Das Verhalten bleibt** — es ist in `doc/missions/campaign.md` deklariert,
das Urteil liegt dort ausdrücklich bei der `MISSION_RESULT`-Kette, und eine Umsortierung würde auf jeder
Kampagne eine Observable bewegen. Korrigiert ist das WORT: die Doku nannte das Maximum „the worst
mission's code", und wer dem Wort traute, hätte aus einer korrekten Messung den gegenteiligen Schluss
gezogen.

**Offen und ehrlich benannt:** mehrfacher Durchlauf und die beiden Kriterien aus
`fb_campaign_verify.py` (Determinismus über Wiederholungen × Threads, Replay gegen Einzelläufe) sind
gebaut, aber nicht über alle zehn gefahren — 60 Kampagnenläufe sind Stunden, und diese Runde hat den
Verbesserungsnachweis vorgezogen, weil es ihn ohne Basislinie gar nicht geben konnte.

---

## 2026-08-03 — Kriterium 1 geschlossen: zehn Kampagnen, sechzig Durchläufe, zehn Fingerabdrücke

Der Auftrag verlangt nicht einen Durchlauf, sondern dass jede Kampagne **mehrfach und
deterministisch** durchgespielt wird. Beides ist dasselbe Experiment, und es ist gefahren:
`fb_campaign_verify.py determinism` über alle zehn, je **2 Wiederholungen × `--threads 1/2/4`**.

| Kampagne | Läufe | verschiedene Kampagnen-Fingerabdrücke |
|---|---:|---:|
| w1-red-flag | 6 | **1** |
| w2-osirak | 6 | **1** |
| w3-desert-storm | 6 | **1** |
| w4-allied-force | 6 | **1** |
| w5-baltic-qra | 6 | **1** |
| o1-bekaa-1982 | 6 | **1** |
| o2-pvo-intercept | 6 | **1** |
| o3-yom-kippur-1973 | 6 | **1** |
| o4-gaf-mig29g-dact | 6 | **1** |
| o5-airfield-defence | 6 | **1** |

**60 Kampagnendurchläufe, 600 Missionen, zehn Fingerabdrücke.** Ein Fingerabdruck ist die SHA-256 über
alle `telemetry*.csv`, den normalisierten `events.log` und den Exit-Code jeder Mission, dazu jeden
`campaign-state.txt` und den Kampagnen-Exit — normalisiert werden ausschliesslich `wallS`/`speedup` und
der absolute Pfad, also genau die zwei Feldklassen, die sagen WO und WANN gelaufen wurde statt WAS
gerechnet wurde.

Das ist zugleich der schärfste Determinismus-Nachweis, den dieser Baum bisher hat: nicht eine Mission
über drei Threadzahlen, sondern zehn verkettete Missionen mit übertragenem Zustand, sechsmal, ohne eine
einzige Abweichung. Prinzip 4 („gibt das Tempo das Ergebnis, ist die Kopplung ein Bug") gilt damit auch
über die Kampagnengrenze.

Offen bleibt Kriterium 2 (Replay: jede Stufe einzeln nachgeflogen gegen den Kampagnen-Fingerabdruck) —
es läuft — und der wiederholte Verbesserungsnachweis, für den es einen zweiten Verbesserungsschritt
braucht und nicht nur einen zweiten Durchlauf.

**Und Kriterium 2 im selben Zug: die Kampagnenschicht fügt keinen versteckten Zustand hinzu.**
`fb_campaign_verify.py replay` fliegt jede Stufe EINZELN nach — mit der Zustandsdatei der vorigen als
einziger Eingabe — und vergleicht ihren Fingerabdruck gegen den aus dem Kampagnenlauf. Über alle zehn
Kampagnen: **100 von 100 Missionen MATCH, null Abweichungen**, Exit-Code und Fingerabdruck jeweils
identisch (z. B. `10-w1-10-graduation` exit=3 `610a97bf533f79cb` in beiden Richtungen).

Damit steht die Architekturbehauptung der Schicht gemessen da: eine Kampagne ist **eine Folge gewöhnlicher
Läufe** und nichts weiter. Was zwischen den Missionen wandert, wandert vollständig durch
`campaign-state.txt` — hätte der Läufer irgendwo Zustand im Speicher gehalten, wäre genau dieser Test
auseinandergefallen.

---

## 2026-08-03 — Die Arena verweigert `o4`, und die Widerlegung stand in unserer eigenen Doku

Gesucht war der zweite Verbesserungsschritt: eine Doktrinverschiebung über die Kampagnenbreite. Gewählt
wurde `o4-gaf-mig29g-dact`, weil sie **5 der 9 FAIL** des ganzen Baums trägt und die einzige Kampagne
ist, in der beide Piloten-KIs direkt gegeneinander fliegen. Angenommen war: meiste Fehlschläge =
meister Spielraum.

**Gemessen ist das Gegenteil. 10 Zellen × 25 Hebel = 250 Läufe, Ergebnis: 0 informative Zellen.**

| | |
|---|---|
| 7 von 10 Zellen | **100 % modal** — alle 25 Varianten liefern dieselbe Ergebnisklasse |
| 3 von 10 Zellen | 96 % modal, und immer derselbe eine Beweger: `energy-low` (0,7 — unter der eigenen Mindestfahrt der Zelle, also die entartete Schiene) |

Das gesamte Genom — Deckung, Energie, Netz, Sortierung, Abwurf-Bias, CCIP, **Verbandsform** und
**EMCON** — bewegt auf dieser Front **nichts**. Tor S1 hat gefeuert, und dafür ist es da.

**Die Ursache stand seit vier Runden in diesem Baum, gemessen, und ich habe sie nicht angewandt:**
*„Gradability is a property of the SIDE's size … a lone jet has no formation to shape, no mate to sort
against, nobody to be silent behind"* — 0,44 Beweger im Mittel über 107 Ein- und Zweischiff-Zellen.

`o4` besteht aus **acht Zellen mit EINER MiG** und zwei mit zweien; 24 Flugzeuge auf zehn Missionen,
zwei Zellen mit ≥ 4 je Seite. **Es ist die kleinste Kampagne des Baums.** Ich habe die am wenigsten
graduierbare Form gewählt, weil ich nach FAIL-Zahl sortiert habe statt nach Seitengröße.

**Der Befund, in einem Satz: die FAIL-Zahl ist kein Maß für den Spielraum einer Doktrin.** Eine Zelle,
die immer gleich verliert, ist kein Messrig — sie ist ein Fixpunkt. Und `o4`s Fehlschläge sind damit
kein Doktrinproblem, sondern eines der Fähigkeit oder des Missionsentwurfs; welches, sagt diese Runde
nicht.

**Die Front für den nächsten Versuch ist hergeleitet statt gewählt:**

| Kampagne | Flugzeuge | Zellen mit ≥ 4 je Seite | SUCCESS |
|---|---:|---:|---:|
| o4 (verworfen) | 24 | 2 | 1 |
| **o1-bekaa-1982** | 48 | **8** | **0** |
| o5-airfield-defence | 55 | 8 | 1 |
| w3-desert-storm | 76 | 8 | 2 |

`o1-bekaa-1982` ist groß genug, dass Doktrin wirken KANN, und schlecht genug, dass es sich lohnt. Der
Sweep läuft. Der verworfene Ansatz bleibt mit seiner Messung stehen — 250 Läufe, 0 informative Zellen —
weil ein gemessener Fehlschlag Wissen ist.

---

## 2026-08-03 — 850 Läufe über die reagierenden Zellen: eine graduierbare Zelle, und ein Hebel, der keiner ist

Zwei Sweeps zuvor hatten 0 informative Zellen ergeben, und ich hatte daraus geschlossen, die
Kampagnenzellen unterschieden keine Doktrin. **Das war zu stark, und der Grund war meine Auswahl.**
E14s 34 nachweislich reagierende Zellen liegen so: w3 9, w4 9, w2 6, o5 5, o1 2, w1 2, o2 1, **o4 0** —
ich hatte für beide Sweeps ausgerechnet `o4` (null) und `o1` (zwei) gewählt, erst nach FAIL-Zahl, dann
nach Seitengröße. Beide Male lag die richtige Größe schon gemessen im Baum.

**Der Sweep über alle 34, 850 Läufe:**

| | |
|---|---|
| S5-Ausbeute | **1 informative Zelle von 34** (gefordert: 3) |
| die eine | **`w3-09-saturation`** — 5 Ergebnisklassen, 60 % modal, **10 Beweger**, besteht S1 UND S2 |

**Und der wichtigste Befund ist ein Artefakt, kein Ergebnis: `bias-rail` bewegt 28 von 34 Zellen.**
Das ist der ferne Anschlag `pilot_attack_bias_s = 10`, den die Hebeldatei selbst als *„2,3 km Bahn,
also gar keine Zustellung"* kommentiert. Eine zerstörte Bombenlösung ändert das Ergebnis — das ist
keine Doktrin. **Wer die 28 als Beweger zählt, misst den Anschlag, nicht das Genom.** Rechnet man ihn
heraus, bleiben je Zelle ein bis drei echte Beweger, und 24 der 34 Zellen haben null.

Die Gene, die überhaupt wirken, sind genau die, die der Auftrag als Erweiterung verlangt hat —
`shape-stacked`/`shape-flat`/`shape-abreast` (Verband), `sort-left`/`sort-near` (Sortierung), `net-off`
(Netz), `bias-early`/`bias-late`. Sie wirken aber **schmal**: ein bis zwei Zellen je Gen.

**Was das für den Verbesserungsnachweis heißt.** Er ist nicht unmöglich, aber er hat heute genau eine
Bühne: `w3-09-saturation`. Eine Doktrinverschiebung dort wäre messbar; über die Breite ist sie es nicht,
weil die Breite nicht reagiert. Das ist ein Befund über die MISSIONEN, nicht über die Piloten — und die
Reparatur ist, Zellen zu bauen, die wie `w3-09` graduierbar sind, statt die Piloten weiter gegen taube
Zellen zu optimieren.

**Und ein Werkzeugbefund nebenher:** `fb_campaign_arena.py` bewachte `sim/assets` als Ganzes und hat
den Lauf blockiert, weil ein Modellierer parallel Netze nach `sim/assets/models/` schrieb. Ein
Dreiecksnetz ist keine Modellzahl — es erreicht weder JSBSim noch die Regelung, und `fb-gym` lädt es
nicht (GPU-frei, 0 Dawn-Symbole). Das Tor bewacht jetzt `sim/missions`, `sim/assets/aircraft` und
`MODEL-DELTAS.md`, also das, was ein Ergebnis ändern KANN.

## 2026-08-03 — `E16`: keine Verschiebung, und der Grund ist Arithmetik — dafür eine Rotte, die 462 s blind ist

**Das Ergebnis steht vor der ersten Messung fest, und ich hätte es vor dem Bauen der Rigs ausrechnen
können.** §6 veröffentlicht eine Doktrinverschiebung als Vorzeichentest über GEPAARTE ZELLEN. Bei `n`
Zellen ist das kleinste erreichbare einseitige p der einstimmige Fall `2^-n`:

| n | 1 | 2 | **3** | 4 | **5** |
|---|---:|---:|---:|---:|---:|
| bestes p | 0,500 | 0,250 | **0,125** | 0,063 | **0,031** |

Die Arena hat drei Zellen. **Kein Ergebnis auf ihr erreicht p ≤ 0,05** — nicht mit einem besseren Genom,
nicht mit einer längeren Hebeldatei, nicht mit mehr Läufen. Und der Chaos-Schirm lässt genau eine der
drei zu (`sat-02` 0 von 8; `sat-01` 1 von 8; `sat-03` 4 von 8), also ist das zulässige `n` **eins** und
die Decke **p = 0,5**. Die Lücke zu einer veröffentlichbaren Verschiebung sind **vier weitere
S7-saubere graduierbare Zellen** — `E-27`.

**Der beste Kandidat, gemessen: 2 besser : 1 schlechter, p = 0,5.** `pilot_emcon_frac ≥ 1,35`. Nach X4.2
bleibt 1 : 1. Über alle 31 bewegten Paare gewinnt das SAATGENOM: **9 besser gegen 22 schlechter.** Die
Basis ist auf diesen Zellen ein lokales Optimum, und die Gegenprobe sagt es sauber — `bias-early` UND
`bias-late` sind je 0 : 3, `shape-tight` und `shape-wide` je 1 : 2. Symmetrisch schlechter in beide
Richtungen ist genau das Bild einer Achse, die auf ihrem Minimum sitzt. Für `pilot_attack_bias_s` ist
das die unabhängige Bestätigung von `E15`s Korrektur an `E14`, auf Zellen, die `E14` nie gesehen hat.

**Und jetzt der Befund, der mehr wiegt als die Doktrin.** Sechs F-16, je vier AIM-120, Master Arm scharf,
`task intercept`, 520 s gegen acht MiG-29 — **und keine einzige Rakete verlässt die Schiene.** Die Kette,
jedes Glied mit seiner Zahl aus einem veröffentlichten Kanal:

```
FBPilot.cpp:1523   EmconSilent_ = other && nearestM > radiateM      radiateM = 1,0 x 40 nm = 74,1 km
t=56,0 s           fcr_contacts 0 -> 4      erste eigene Erfassung, ~104 km
t=57,5 s           fcr_on -> 0, flt_src -> 0     und beide bleiben es 462,5 s lang
                   -> fcr_on 574 von 5 200 Ticks = 11,0 %
Meldungsentfernung, vom Gen selbst eingegabelt:
                   f = 1,30 (96,3 km) still  |  f = 1,35 (100,0 km) strahlt    auf ALLEN DREI Rigs
                   -> der gemeldete Punkt verlässt 462 s lang kein 3,7-km-Fenster,
                      während die Geometrie 220 km schließt. Das ist keine Spur, das ist eine Zahl.
fcr_lock           0 von 5 200 Ticks
sms LAUNCH_SOLUTION 18 Zeilen im Lauf, 0 davon blau; eng_shots = 0 auf allen sechs
mission OBJECTIVE  kill unit: 0 von 8 erfüllt
```

**Die Emissionssperre rastet ein.** Einen Tick nach der ersten eigenen Erfassung meldet ein Rottenflieger
„engaged"; alle anderen rechnen `nearestM ≈ 98 km > 74,1 km` und schweigen. Schweigend erfassen sie
nichts, also kann niemand die Meldung erneuern, also bleibt sie stehen, also schweigen sie weiter. Und
die beiden Leser desselben Blocks widersprechen sich: **`flt_src` = 0 sagt „ich habe kein Bild", die
Emissionssperre desselben Ticks sagt „jemand hat eins".** Zwei unabhängige Gene brechen die Rastung mit
identischem Ergebnis — `f ≥ 1,35` und `dl=off` geben beide `fcr_on` 100 %, 2 von 8 Abschüssen, M 8 → 10.
Es ist keine Taktik, es sind zwei Arten, eine 462 Sekunden alte Meldung nicht zu lesen. `X-6`.

**Der Gegenspieler ist echt, und deshalb ist der Defekt gefährlich.** Auf `sat-01` holt sich die
dauerstrahlende Rotte den Verbandsführer ab: `monitor KO unit=bl1 CFIT` bei t = 133,5 s, Lauf zu Ende,
27 von 36 Zielen → 23. Ein Defekt, der auf einem echten Zielkonflikt sitzt, ist für einen Sweep
unsichtbar, der nur das Vorzeichen liest.

**Die Uhrprobe, die `E15` erzwungen hat, kippt zwei der drei Zellen.** `FBMissionSim::Conclude` sagt es
selbst: *„a K.O. always ENDS the run but only DECIDES it when it was nobody's declared objective"* —
`ExpectedLoss` nimmt einen Verlust aus dem URTEIL, nie aus der UHR. Auf `sat-03` verlängern **6 von 6
verbessernden Hebeln** den Lauf (301,7 s → 520,0 s), und X4.2 sagt unabhängig dasselbe: (26, 24) →
(23, 22) bei `timeout × 1,5`. Auf `sat-01` bewegt sich die BASIS selbst — (32, 23) → (30, 22) —, ihr
Bezugspunkt ist also eine Funktion der Uhr. **Nur `sat-02` hält auf beiden Instrumenten.** Und damit
korrigiere ich meine eigene Behauptung von gestern: `sat-02`s Kopf begründet seine Immunität damit, dass
die BASIS die vollen 520 s fliegt — richtig und unzureichend, denn **10 ihrer 12 Beweger tun es nicht**,
und das Urteil trägt der VERGLEICH. `X-7`.

**Zwei Werkzeugbefunde, beide unangenehm.** (1) **Die Hälfte der Hebeldatei ist die Identität**: 12 von 24
Hebeln sind in allen 28 Kanälen bitgleich zur Basis — auf allen drei Zellen. `cover-*` und `energy-*`
waren bekannt; neu sind `emcon-tight`/`emcon-mid`, und zwar aus einem Grund, der das ganze Gen umdeutet:
**`pilot_emcon_frac` ist keine Rampe, sondern eine dreiwertige STUFE** (immer still bei 0, Saatverhalten
bei 0 < f ≤ 1,30, immer strahlend ab f ≥ 1,35). Die drei G5-Allele tasten also ein Phänotyp zweimal und
den anderen einmal ab. S2s Latte `3/9 × 24 = 8` muss folglich aus **zwölf lebenden Hebeln** kommen —
`E-26`. (2) **`pilot_flight_stack_frac` wirkt auf 9 von 9 Rasterpunkten und trägt trotzdem keine
Richtung**: das Vorzeichen wechselt viermal entlang des Rasters, und die Zähne stimmen zwischen den
Zellen nicht überein. Auf dem groben Dreiallel-Raster stand es als 2 : 1 und Zweitbester da. `X-8`.

**Kosten:** 233 Läufe, ~45 min bei `--jobs 6`. `sim/src`, `sim/vendor`, `sim/assets/aircraft` und alle
committeten Missionen unberührt; `tree_clean()` vor und nach jedem Sweep grün. Die 3-Zellen-Tabelle
reproduziert `85c1a74` exakt (9/9/10 Klassen, 60,0/52,0/52,0 % modal, 10/12/12 Beweger) unter einem neu
gelinkten Binary — der Hashwechsel ist kosmetisch, die Verweigerung der Resume-Indizes trotzdem richtig.

---

## 2026-08-03 — `X-6` repariert, und der benannte Mechanismus war der falsche: der Knopf gehörte dem Bild

**Die Vorgabe war eine Sperre in der Emissionsregel — gemessen ist sie es nicht.** `X-6` las die Rastung
als `EmconSilent_ = other && nearestM > radiateM` auf einer Meldung, die niemand auffrischen kann. Eine
Sonde auf genau diesem Ausdruck, jeden Entscheidungstakt geloggt, auf demselben Rig und demselben
Binary, sagt etwas anderes:

```
sat-02, je F-16:   EmconSilent_ TRUE   10 von 5 200 Takten (0,19 %) — t = 57,0 … 57,9 s
                   Radar AUS         4 626 von 5 200 Takten (89,0 %) — ab t = 57,5 s bis zum Ende
                   die Meldung des Rottenfliegers verfällt bei t = 58,0, einen Netzzyklus später
```

**Das Tor öffnet 0,9 s nachdem es geschlossen hat, und das Radar bleibt trotzdem 462 s aus.** Die Meldung
altert korrekt; eine Alterungsschwelle wäre eine Antwort auf eine Frage, die niemand hat. Die Rastung
liegt eine Ebene tiefer, in der HAND: `InterceptCockpit` prüfte vor beiden `RadarMode`-Posts
`state.Radar.H.Readable()` — den Kopf des BILDES —, und `FBRadarSystem::Run` invalidiert genau den,
sobald nichts mehr strahlt. Der einzige Weg zurück ins Strahlen war durch den Zustand gesperrt, den er
aufheben soll. Auch der zweite Widerspruch löst sich auf: `flt_src` = 0 ist kein Widerspruch, sondern
`FBFlightPicture::Assign` ohne EIGENE Echos, gegen die es den gemeldeten Punkt korrelieren könnte. Beide
Schichten lesen denselben Block und sind sich einig; sie beantworten verschiedene Fragen.

**Repariert als Invariante, nicht als Wächtertausch: _jeden Emissionszustand, in den der Pilot selbst
geht, muss er auch verlassen können._** `FBRadarBlock` bekommt das `Powered`-Rückmeldebit, das seine
zwei nächsten Geschwister (`Rwr`, `Datalink`) längst tragen, plus `SetAbsent()` für den Modulzweig ohne
Gerät; der EMCON-Post fragt die Rückmeldung statt das Bild; `IntEmconSilenced_` lässt ihn nur SEINE
EIGENE Stille zurücknehmen. Der zweite Teil ist gemessen erkauft: ein erster Schnitt ohne ihn schaltete
vier Missionen auf, deren ganze Prämisse ein gebrieft stilles Radar ist (`bvr-defend`,
`bvr-defend-blind`, `damage-amraam`, `o5-04-no-radar`). Ein `set fcr_mode off` ist eine Entscheidung
über dem Piloten — dieselbe Rangordnung, die `HaveOrderEmcon_` über seine eigene Regel stellt.

**Bodenwahrheit, `sat-02`, vorher → nachher:** `fcr_on` 11,0 % → 87,3 %; `fcr_lock` 0 → 18 Takte; blaue
`sms LAUNCH_SOLUTION` 0 von 18 → 4 von 12; `eng_shots` 0 → 1 je Sweep-Mitglied; benannte
`kill unit`-Bits **0 von 8 → 2 von 8**; `(V, M)` **(14, 8) → (14, 10)**. Und die Doktrin LÄUFT jetzt,
statt nur nicht mehr zu rasten: pb1 schweigt 1,0 s, strahlt 7,7 s, schweigt 30,6 s, strahlt bis zum
Ende. Dass 2 von 8 derselbe Schlüssel ist, den `f ≥ 1,35` und `dl=off` erreichen, ist Bestätigung und
NICHT das Argument (Prinzip 1) — das Argument ist der Wächter.

**Regression über alle 287 Missionen: 246 bitgleich, 41 bewegt, 2 Exit-Codes.** Die 41 sind EINE Klasse
und der Test ist exakt: eine Mission bewegt sich **genau dann**, wenn eine F-16 mit `task intercept` und
nicht gebrieft stillem Radar zusammen mit einem zweiten Datalink-Terminal fliegt — die Bedingung, unter
der `EmconSilent_` überhaupt wahr werden kann. 72 Missionen erfüllen sie, 31 davon lösen die Geometrie
nie aus, **0 außerhalb der Klasse bewegen sich**. Alle 41 gewinnen `RadarMode`-Verkehr (Basis: exakt zwei
Zeilen je verstummender Jet, der Einbahn-`Off`; danach die Rundwege). Die zwei Exit-Codes sind je ein
sterbendes rotes Flugzeug, und beide Dateien sagen in ihrem eigenen Kopf, dass der Exit-Code nicht ihr
Urteil ist: `o3-10-october-six` 3 → 2 (`yxh` wird bei t = 385,9 kampfunfähig geschossen und erreicht bei
t = 402,4 den Boden — die Datei nennt genau diese Form vorab als ihre Standalone-Gestalt),
`ar-01-headon-noon` 3 → 1 (`ar01hi4` abgeschossen; `(V, M)` blau (20, 12) unverändert, rot (34, 8) →
(33, 7)).

**Zwei Messungen anderer Runden sind damit Messungen des Defekts und stehen zur Neuaufnahme.**
[`formation.md`](formation.md) F2 schrieb einen Sortier-Churn-Rückgang von 65–94 % einer Taktik zu — ein
ausgeschaltetes Radar baut keine Kontaktliste, die flattern könnte: neu gemessen steigt `flt_switch`
wieder um 17–188 % und `flt_dup` verlässt die Null auf allen fünf Missionen. [`duels.md`](duels.md) D3c
nahm seine Abnahmezahlen („20 von 251 Missionen bewegt", `emcon_frac = 3,0` reproduziert die Vorrunde)
auf einem Baum, in dem Stille eine Einbahnstraße war. Und `E16` §6(b) hat `pilot_emcon_frac` als
dreiwertige STUFE vermessen — gegen eine Saat, deren Radar gerastet aus war.

**Tore:** `gym`/`native`/`wasm` bauen · `verify-layers` (6 Registry-Leser, unverändert), `verify-guards`
8/8, `verify-models` grün · zehn Harnesses rc = 0 · Determinismus `--threads 1/2/4` bitgleich in
Telemetrie und Ereignislog (nur `wallS`/`speedup` bewegen sich) · `vendor/` und `assets/aircraft/`
unberührt.

---

## `E17` (2026-08-04) — Die Guillotine verlässt das Urteil: `until <s>`, und die Arena hat zwei Zellen

**Der Blocker war eine Zahl, und er ist um eine Stufe kleiner geworden.** §6 veröffentlicht über
gepaarte Zellen, das kleinste erreichbare einseitige p ist `2^-n`. `E16` hat n = 1 gemessen (Decke
p = 0,5). Diese Runde greift die WURZEL an, die der Erbauer der drei Messrigs selbst benannt hatte, misst
sie, repariert sie im Richter und baut dagegen. **n = 1 → 2, Decke 0,500 → 0,250.** Drei Zellen fehlen
weiter, und die Runde sagt auch, worauf sie NICHT stehen dürfen.

**Zuerst neu gemessen, denn `X-6` hatte den Baum bewegt.** 3 Zellen × 25 Hebel + 24 Chaosläufe, Simulator
`a763d63ebf97c921`: `sat-01` 12 Beweger / modal 52,0 % / **S7 1 von 8**, `sat-02` 10 / 60,0 % / **0 von
8**, `sat-03` 14 / 44,0 % / **5 von 8** (vorher 4). `E-27`s Zählung überlebt die Neumessung.

**Die Wurzel, gemessen und ZERLEGT.** Auf `sat-03` zerfallen die fünf Kipper exakt: in **5 von 5** steckt
der Tod des Rottenführers (der Kampf, eine echte Münze), in **4 von 5** dazu der TAKT — und der Beweis
ist eine Zahl, gegen die man nicht argumentieren kann: `ee3`s Gürtel-Verweildauer bei t = 317,9 s ist
**175,2 s im 520-s-Lauf und 175,2 s im Lauf, der dort abbricht**. Dieselbe Bahn auf die Zehntelsekunde,
dasselbe Budget von 200 s — und das eine liest `unmet` (der Lauf ging weiter bis 305,5 s), das andere
`met`. Über das Gitter nimmt dasselbe Ziel vier verschiedene Antworten aus vier Abbruchzeitpunkten an,
während seine eigene Chaosamplitude an einem FESTEN Zeitpunkt **1,0 s** beträgt. Eine Störung von 0,6 %
wird in ein Bit verstärkt.

**Die Reparatur ist EIN Satz im Richter.** `objective ... until <s>`: der Zustand wird zu einer vom
AUFTRAG genannten Simulationszeit EINGEFROREN — kein Kind, kein Prädikat je Kind, dasselbe `StateOf`,
einmal gelesen. `FBObjectiveCovers` bleibt unberührt (Deckung ist eine Eigenschaft der ERKLÄRUNG), der
Richter bekommt keine neue Quelle, und eine Spanne am oder hinter dem `timeout` ist ein Parse-Fehler.
Erhaltung, voll gemessen: **3 337 von 3 337 Telemetriedateien bitgleich (SHA-256)** und **287 von 287
`events.log` identisch** über den ganzen Vor-Runden-Baum. Wirkung: `sat-03` nur mit Fenstern und sonst
unverändert geht von **5 von 8 auf 1 von 8**. Und der eingefrorene Spruch wird VERÖFFENTLICHT, nicht
behauptet: auf `sat-04` sind **864 von 1 000** `mission OBJECTIVE`-Zustände bitgleich mit ihrer eigenen
`mission WINDOW_CLOSED`-Zeile, **0 Abweichungen**; die übrigen 136 gehören Einheiten, die vor ihrer
Spanne abgeschlossen haben.

**`sat-04-vul-window`, die Zelle, die die Reparatur kauft.** `sat-01`s Geometrie mit den zwei gemessenen
Chaoskanälen geschlossen und sonst nichts — ein kontrollierter Versuch, keine neue Frage, damit die
Reparatur zurechenbar bleibt. Basis (27, 18), 9 Klassen, modal **52,0 %**, **12 Beweger von 24**,
**S7 0 von 8**. Und die Uhrprobe strukturell bestanden: die Laufdauer spannt über den Hebelsatz
**133,5 … 520,0 s** und die Klasse folgt ihr nicht (`emcon-tight` bei 520,0 s und `emcon-mid` bei 514,7 s
liefern dieselbe (28, 20)).

**Zwei Gesetze, die jede künftige Zelle binden.** (a) **G6 ist arithmetisch ausgeschlossen**, wo eine
Zelle eine Abgabe gegen einen Letalradius benotet: die Auslösung ist ein ENTSCHEIDUNGSTAKT, also ist der
Treffpunkt bei 22,9 m Bahn gerastert, und der ganze `pilot_attack_bias_s`-Hebel ist ±0,1 s = **23 m**.
Ein Hebel der Größe L überquert eine Schwelle mit Rand M auf beiden Seiten nur, wenn L > 2M — und
23 > 44 ist falsch. Signal = Chaos, dieselbe Ursache wie `sat-01`s einziger Kipper. (b) **`protect` und
`survive` werden erst NACH dem Schlagabtausch benotbar, und der Schlagabtausch IST das Chaos.** Gemessen
statt geschlossen: `sat-05` (= `sat-03` gefenstert, sonst nichts) fällt von 14 Bewegern und 44,0 % auf
**4 Beweger und 84,0 %** — S1 und S2 beide verweigert. **Zehn von `sat-03`s vierzehn Bewegern waren
Beweger des Abbruchzeitpunkts**, also `X-7`, bestätigt durch Entfernen der Uhr statt durch Lesen von
Dauern. Die Datei wurde nicht committet; die Messung ist das Produkt.

**`sat-06-qra-window` wird von S1 verweigert, und das Zahlenpaar IST der Fund.** Eine andere Frage
(Identifizierungsdurchgang gegen UNBEWAFFNETE An-26, also gar keine Kampfmünze) auf denselben reparierten
Mechaniken: S2 ok (9 Beweger), **S7 0 von 8**, S1 **NEIN** bei 64,0 %. Dieselbe Zelle, ein Zielpunkt um
11 m verschoben: auf der Rasterkante **S1 ok (60,0 %), 10 Beweger, S7 1 von 8** — auf der Rastermitte
**S7 0 von 8, 9 Beweger, S1 NEIN**. Der Hebel, der die zehnte Klasse trennte, WAR die Münze. Committet
ist die S7-saubere Form, denn ein Kriterium kauft man nicht zurück.

**Eine Korrektur an einer committeten Messung.** `E16` §6(b) las `pilot_emcon_frac` als dreiwertige
STUFE und `emcon-tight`/`emcon-mid` als Identität. Auf `sat-04` bewegen **alle drei** Emcon-Allele die
Klasse. Die Stufe war eine Messung des `X-6`-Rasters.

**Tore:** `make gym` · `verify-layers` (6 Registry-Leser unverändert), `verify-guards` 8/8,
`verify-models` grün · zehn Harnesses rc = 0 · Erhaltung 3 337/3 337 + 287/287 · Determinismus
`--threads 1/2/4` bitgleich (287 Altmissionen sowie beide neuen Rigs einzeln) · `vendor/` und
`assets/aircraft/` unberührt · keine Commits.

---

## `E19` (2026-08-04) — Die erste KO-EVOLUTION, und der Gegner kann ein Gen von neun tragen

**Die Prämisse des Auftrags war halb falsch, und die erste Messung hat es gezeigt.**
`tools/fb_campaign_evolve.py` trägt `--archive`, `archive_sample` und `kArchiveSample` — aber es WAR
nie eine Ko-Evolution und wird durch Einschalten des Archivs keine: sein eigener Kopfkommentar sagt es,
*„der Gegner ist committeter Missionstext; er kann nicht antworten"*. Ein Archiv über eine Welt, die
sich nicht bewegt, hat nichts zu verhindern. Gebaut wurde deshalb ein zweites Werkzeug:
`sim/tools/fb_campaign_coevolve.py` — EIN Lauf, ZWEI Seitenschlüssel — plus `tools/duels-sat.txt`, die
fünf `E18`-Zellen mit beiden Seiten DEKLARIERT.

**Zuerst das Instrument gegen eine committete Zahl geprüft, bevor irgendetwas geglaubt wurde.** Der
zweiseitige Leser reproduziert `E18`s Tabelle in **5 von 5 Zellen exakt** — (14,10) (27,18) (17,17)
(14,27) (22,23).

**Die zentrale Messung, und sie ist auf Bit-Ebene genommen, nicht auf Klassenebene.** 24 Hebel in die
MiG-29-Seite gespleißt, SHA-256 über die gesamte Telemetrie beider Seiten: **23 von 24 sind
bitgleich zur Basislinie**, auf `sat-02`, `sat-04` und `sat-07`. Der einzige Beweger ist der
Sortier-Kontrakt. Über das ganze 24-Schlüssel-Alphabet: **12 Schlüssel bewegen die MiG, und KEINER
davon steht im Genom**; von den neun Genen sind **acht bitstill**. Das Genom ist F-16-förmig.

**Zwei Mechanismen, beide in Quellzeilen und einem publizierten Kanal.** G5 (Emission):
`SilentRadarModeOrdinal()`/`EmconRadiateNm()` stehen auf −1/0,0 (`FBPilot.h:310–311`), der ganze
EMCON-Block hängt an beiden (`FBPilot.cpp:1540`), und überschrieben wird er in **genau einer Datei**
— `FBF16Pilot.h`. G1 (Form): `FBFlightPicture::BuildMembers` bricht ohne Datalink-Block ab, die MiG hat
keinen — `flt_mates` ist **3 auf jeder F-16 und 0 auf jeder MiG**, obwohl jede `flight ia 1…4`
deklariert. Ohne Rottenmitglied kein Führender, ohne Führenden keine Station: `shape-tight/wide/stacked`
lassen die MiG-Abstände bei **1394 / 3537 / 4113 m** — den Zahlen der Basislinie auf den Meter.

**S7 ist keine Eigenschaft der ZELLE, sondern des PAARES (Zelle, Gegner).** Dasselbe ±3-m-Gitter, auf
dem `E18` alle fünf Zellen mit 0 von 8 zertifiziert hat, kippt **4 von 25 Paaren** — `sat-04`/`near`
3 von 8, `sat-08`/`right` 3 von 8, `sat-08`/`none` 3 von 8, `sat-04`/`none` 1 von 8. Jedes schmutzige
Paar trägt einen NICHT-committeten Gegner. Die vier fliegen aus der Auswertung.

**Der Sweep über den ganzen Gegnerraum — 625 Läufe — und er ist NEGATIV, aber auf eine neue Art.**
Kein Hebel erreicht die Decke 0,031 gegen IRGENDEINEN Gegner; das Beste je Gegner ist `emcon-mid`
p = 0,188 (gegen `red-left`, also `E18`s Welt, exakt reproduziert), `emcon-tight` 0,125,
`shape-tight` 0,250, `net-off` **0,062**, `emcon-tight` 0,250. **Und das Vorzeichen JEDES Hebels, der
überhaupt eine Richtung hat, kippt mit dem Gegner:** `emcon-mid` liest `+ + − − =` über die fünf
Allele, `emcon-tight` `+ + − + +`, `net-off` `= = = + −`, `shape-abreast` `+ + + − −`. Vorzeichenstabil
sind nur „immer schlechter" (`bias-rail`, `ccip-tight` — beides Schienen) und „immer nichts".

**Das ist die Diagnose, die `E18` sich selbst nicht geben konnte.** Seine p = 0,188 war kein schwaches
Signal, das mehr Zellen geschärft hätten — es war ein Signal, BEDINGT auf einen Punkt eines
Gegnerraums, der mindestens fünf hat. Die Bedingung wiegt mehr als der p-Wert.

**Und der Gegner hat auf drei von fünf Zellen gar keine eigene Fitness.** Die DREI TROCKENEN Zellen,
mit denen `E18` das Chaos gekauft hat, geben der MiG ein einziges `survive`-Ziel, das nichts bedrohen
kann: rote Klasse **konstant** über den ganzen Strategieraum — (24,8), (12,4), (12,4). Das Gerät, das
die chaosfreie Arena gekauft hat, ist dasselbe, das sie einseitig macht.

**Vier Exploit-Befunde, und alle wiegen mehr als die Doktrin.** `X-15` ein Genwert auf ein Modul, das
das Gen nicht ausdrücken kann, wird STILL angenommen (`ApplyTuning` liefert `true`, die Mission ist
bitgleich); `X-16` zwei Genfamilien sind Ein-Modul-Gene und vier Runden Doktrin wurden darauf benotet;
`X-17` S7 misst einen Gegner und meldet eine Zelleneigenschaft; `X-18` ein trockenes Rig entfernt das
Chaos, indem es die Fitness des Gegners entfernt.

**Und die Ko-Evolution selbst, 915 Läufe, 3 Generationen, lief — mit einem degenerierten Ergebnis, das
selbst die Aussage ist.** Beide Champions stehen ab Generation 0 fest (blau `sort=left`, rot
`sort=none`), die Bewertung gegen das eingefrorene Feld ist über alle drei Generationen FLACH (0,800 /
0,600), Instrument (b) ist **nicht berechenbar — 1 unterscheidbarer Champion je Seite**, wo die
Statistik 3 braucht, und jedes Archiv nimmt genau EIN Genom auf. Blaus Zahlengen ist flach: `emcon` bei
0 / 0,75 / 1,5 / 2,25 / 3 punktet identisch, sobald der Sortier-/Kanal-Bit steht. Das ist `X-4`
(*„der Datalink kostet eine F-16, und NICHT über die Sortierung"*), von einer Suche reproduziert, die
nicht danach gesucht hat.

**Und ein fünfter Befund, gefunden beim Lesen der Champion-Zeile: `X-19`.** `Genome.line()` schreibt
`dl=` nur, wenn es von `"off"` abweicht — `"off"` ist aber WAHR, also spleißt das Werkzeug
`set datalink off` ein und wirft das `set datalink on` der Mission heraus. Jedes blaue Genom dieser
Runde ist mit ausgeschaltetem Netz geflogen und hat dabei keinen Kanal-Bit gedruckt. §3.2 verlangt vom
Archiv genau eine Eigenschaft — *„verbatim und wieder fliegbar"* — und die hält nicht, auch nicht in
irgendeinem Archiv, das `fb_campaign_evolve.py` je geschrieben hat.

**Ein Werkzeugfehler auf eigene Rechnung, weil er teuer war.** Vier Läufe brachen mit fehlenden Dateien
ab, und die Ursache war nicht der Simulator: `ThreadPoolExecutor.map` reicht ALLE Jobs vorab ein, also
lief eine abgebrochene Instanz noch 20 Minuten weiter und löschte per `rmtree` die Verzeichnisse der
NEU gestarteten — gleiche deterministische Tags, gleicher Pfad. Der erste Abbruch war echt und
banal: die Platte stand auf 99 %.

**Keine Doktrinverschiebung veröffentlicht.** §6s bindende Regel greift: der stärkste Kandidat der
Runde ist `shape-trail`, gepoolt p = 0,055, mit einem Vorzeichen, das unter `red-near` umkippt.

**Tore:** Determinismus `--threads 1/2/4` auf einem zweiseitig gespleißten Paar — gleicher Schlüssel und
BYTEGLEICHE Telemetrie (`sat-09`, SHA-256 `929d49b2ea9a8a9a`, dreimal) · `make -C sim verify-models`
grün · `sim/src/`, `sim/vendor/` und `sim/assets/aircraft/` unberührt · Missions- und Modellbaum vor und
nach jedem Lauf sauber · keine Commits.

## `E30` (2026-08-04) — Drei Zellen, auf denen BEIDE Seiten eine bewegliche Ergebnisklasse haben

**Der Zielkonflikt, den `E-30` benannt hat, existiert nicht.** Er lautete: entweder ist das Gefecht
tödlich, dann kommt das Chaos zurück, oder es ist trocken, dann hat der Gegner nichts zu gewinnen.
Beides ist an dieselbe Ursache geknüpft worden — und die Ursache war eine andere. Ein `objective` wird
vom RICHTER gelesen und von niemandem sonst. Man kann also die Bewertung einer Zelle ändern, ohne einen
einzigen Meter Flug zu bewegen. Gemessen statt behauptet: `sat-07` → `sat-10` **17 von 18** Telemetrie-
Dateien bytegleich (die achtzehnte unterscheidet sich in `zone_zb_s`/`zone_zb_in` NACH t = 300,1 — den
zwei Spalten, die der Richter selbst schreibt), `sat-08` → `sat-11` **18 von 18**, `sat-09` → `sat-12`
16 von 20 präfixgleich plus vier, und die acht blauen `UNIT_RESULT`-Zeilen unverändert.

**Was der trockene Rig wirklich entfernt hat, war nie die Fitness des Gegners — sie war nie deklariert.**

**Drei neue Zellen, beide Seiten bestanden:** `sat-10-duel-merge`, `sat-11-duel-qra`,
`sat-12-duel-gate`. Blau 52,0 / 60,0 / 52,0 % Modalanteil mit 12 / 10 / 12 Bewegern von 24, Rot
30,0 / 30,0 / 40,0 % mit 7 / 7 / 6 von 9, **S7 = 0 von 8 auf allen 105 Paaren (Zelle, Gegner)**. Die
Laufdauer nimmt je Zelle GENAU EINEN Wert über alle 35 Läufe beider Hebelsätze an — X-7 erreicht einen
Vergleich nicht, in dem jedes Mitglied gleich lang ist.

**Das Instrument zuerst, gegen eine Tabelle, die es nicht erzeugt hat.** `tools/fb_duel_arena.py`, auf
das VOR-`E20`-Binary gerichtet, reproduziert `E18` §3 auf die Ziffer — 9/60,0 %/10, 10/60,0 %/10,
9/52,0 %/12, samt Beweger-NAMEN — und die rote Spalte `E19` §3s Konstanten (24,8), (12,4), (12,4) mit
0 Bewegern.

**Und dabei fällt ein unbezahlter Preis auf: `E20`s Reparatur VERWEIGERT zwei von `E18`s fünf Zellen.**
Dieselbe Messung mit dem heutigen Binary: `sat-07` 60,0 % → **68,0 %** (10 → 8 Beweger), `sat-09`
52,0 % → **68,0 %** (12 → 8). S1 verweigert. `E20` hat 293 Missionen nach Exit-Code geprüft und drei
nach Klasse — das Tor selbst hat niemand neu gefahren, und in keiner Tor-Liste steht, dass man es muss.
Jede Aussage in `doctrine-evolution.md`, die auf der Fünf-Zellen-Arena und ihrer Decke 2⁻⁵ = 0,031
ruht, ist damit eine Aussage über das alte Binary. Gebucht als `E-33`.

**Eine Regel musste die Runde sich erarbeiten, und sie ist `X-17` eine Ebene tiefer.** Die erste
Platzierung befolgte `E18` §1b wörtlich — jede Sprosse in der Mitte einer gemessenen Lücke, ≥ 10× die
Chaosamplitude gegen den COMMITTETEN Gegner — und das Tor verweigerte trotzdem zwei Zellen: blau kippte
**4 von 8** unter `red emcon-hi` und **3 von 8** unter drei Sortier-Allelen, auf Sprossen mit 17×
Marge. Eine Sprosse muss das Chaosband ihrer Größe unter JEDEM deklarierten Gegner verlassen, nicht nur
unter einem. Die Regel steht jetzt als Code in `tools/fb_rung_ladder.py`; danach 0 von 8 auf allen
Paaren.

**Die Ko-Evolution, neu gefahren (1 212 Läufe, Genom-Schnitt exakt `E19`s, einzige Variable ist die
ARENA):** Rot hat zum ersten Mal **DREI verschiedene Champions** (`r0_s1` → `r1_00` → `r2_s4`), ein
Archiv mit **sechs** Mitgliedern statt einem, und der feste Maßstab steigt 0,889 → 1,000 → 1,000.
**Instrument (b) ist damit zum ersten Mal in diesem Baum berechenbar:** n = 3, zyklische Tripel 0 von 1,
T = 0,0000.

**Blau friert weiter in Generation 0 — und der Grund ist gemessen, in zwei Schritten.** Erstens:
`fb_evolve.SORT_ALLELES[0]` ist `("off", "")`, also trägt der Saat-Genotyp jeder blauen Population, die
dieser Baum je entwickelt hat, `dl=off`. Ohne Netz findet `FBFlightPicture::BuildMembers` keine
Rottenmitglieder — G1 erreicht `FormationStation` nie — und `EmconSilent_` hat keinen fremden Bericht,
kann also nie still werden: G5 ist tot. [MESS, 30 Läufe] fünf blaue Genome über beide deklarierten Gene
ergeben mit `dl=off` **eine einzige Klasse je Zelle**; mit dem Kanal-Bit der Mission ergeben dieselben
fünf **drei verschiedene Klassen auf `sat-11`**.

Zweitens, und das ersetzt die erste Erklärung als Grund: mit befreitem Saatgut
(`--blue-alleles ":,:left,:near,off:"`, weitere 1 212 Läufe) TRENNT Generation 0 Blaus Genom zum ersten
Mal überhaupt — fester Maßstab **0,208 / 0,267 / 0,367 / 0,700 / 0,900 / 0,933** über sechs Genome — und
der Sieger ist `dl=off` mit 0,933. Ab Generation 1 trägt die ganze Population es und ist wieder flach.
**Das Kanal-Bit ist ein Einwegtor: der erste Zug der Suche entfernt zwei Drittel des Alphabets, das die
Suche noch absuchen müsste.** Das ist eine Eigenschaft der LANDSCHAFT, nicht des Saatguts, und ein
anderer Default wäre nicht die Reparatur gewesen (`E-34`).

**Keine Doktrinverschiebung veröffentlicht.** Der Sweep aus `E18` §4 / `E19` §5 wurde auf den neuen
Zellen NICHT gefahren — `E-28` steht unverändert dort, wo `E19` es gelassen hat.

**Tore:** nichts in `sim/src/`, `sim/vendor/`, `sim/assets/aircraft/` bewegt, also keine Regression
geschuldet · `make -C sim verify-models` grün · bewachter Baum vor und nach jedem Lauf ohne modifizierte
Datei · Determinismus `--threads 1/2/4` auf `sat-10-duel-merge`, dreimal derselbe Telemetrie-SHA-256
`32720b2093962ca5` · keine Commits.

---

## `E31` (2026-08-04) — `X-20` repariert, und der Verdächtige hatte das falsche Vorzeichen

**Die Frage war: Doktrin oder Exploit?** `E30` hatte gemessen, dass Blaus Suche Generation 0 mit
`dl=off` gewinnt. Zwei Lesarten standen offen, und `X-20` war der benannte Verdächtige — eine F-16, die
aus EMCON zurückkommt, erfasst sofort wieder.

**Der Verdächtige zeigt in die andere Richtung, und das ist gemessen statt argumentiert.** EMCON ist nur
mit einem Bild erreichbar, ein Bild auf diesen Zellen nur über den Datalink — `X-20` ist also eine
Subvention, die AUSSCHLIESSLICH `dl=on` bekommt: über 69 (Zelle, Genom)-Paare zeigen **21 von 69**
`dl=on`-Läufen einen festen Kontakt im Rückkehrtakt und **0 von 69** `dl=off`-Läufen überhaupt eine
Stille. `dl=on` verlor 50 : 19, *während* es sie kassierte. Die Reparatur bewegt 2 von 69 Paarungen und
das Urteil nicht.

**Repariert wurde trotzdem, eine Schicht tiefer als gebucht.** `NextScanS_` gehört `FBRadarSystem`, und
genau deshalb hatte die F-16 keine Stelle für den Aufruf: ihre Emission läuft über den MODUS. Drei
Zeilen im nicht-strahlenden Zweig von `Run()`, kein Anfangszustands-Lüge, jedes Modul abgedeckt.
Gemessen an `sat-02-picture-split`/`pb2`: Rückkehr aus 36,3 s Stille mit **0 Kontakten** statt 8 im
selben Takt, erster fester Track nach **4,1 s = ein CRM-Frame**.

**Darunter lag ein zweiter, viel breiterer Defekt.** Ein Raketensucher ist bis zur Aktivierung DUNKEL,
und das Nachholen produziert nicht nur einen Track, sondern eine ZAHL: die Annäherungsrate wird über das
Look-Paar differenziert, nachgeholte Frames teilen sich EINEN `simTimeS`, also bleibt `ClosureMs` auf
der initialen 0. **[MESS, 296 Missionen] 1 472 von 24 688 `RADAR_CONTACT`-Zeilen trugen `closureKt=0`;
danach 0 von 25 200.** Endspiel-Beispiel `o3-09-two-fronts`: Fehlabstand **4,32 m → 0,98 m**.

**Volle Regression, beide Binaries, und die Begründung ist EIN Gesetz statt 115 Sätzen.** 115 von 296
Missionen bewegen sich, 181 sind bytegleich. Jede bewegte Mission trägt mindestens einen der zwei
Fingerabdrücke des Defekts (`closureKt=0` oder ein `radar standby`-Drop), **keine einzige trägt keinen**,
und die drei unbewegten mit Fingerabdruck (`mig29-r27`, `sat-08-ident-qra`, `sat-11-duel-qra`) haben nur
MiG-29-Emissionszyklen — das eine Muster, das `ResyncScan()` schon hatte. Drei Exit-Codes ändern sich,
jeder mit eigener Kette: `ar-10` 1 → 3 (rot V 15 → 16, die dokumentierte 16), `pair-cover` 0 → 3
(Splittergeometrie 1,92 → 3,22 m, `failed` 4023 → 4016), `o3-09` 3 → 2 (alle 14 Systeme aus, Absturz).
**`o3-09`s Leseregel „exit 2 wäre ein Defekt" ist damit falsch und als Korrektur geschuldet** — nicht
genommen, weil ein Missions-Edit mitten in der Runde beide Schnappschüsse und das Zellentor entwertet.

**`E-33` galt für diese Runde, sie hat es befolgt — und dabei selbst eine Zelle verloren.** Das Zellentor
neu gefahren: **kein einziger S1-/S2-Wert bewegt sich, sechs Spalten, auf die Ziffer.** Bewegt hat sich
S7, auf genau einer Zelle: `sat-10-duel-merge` geht von `E30`s 0 von 8 auf ALLEN 35 Paaren auf blau
`sort-near` 3 von 8 und rot **16 von 25 Gegnern bei 3 von 8**, `committed` darunter. Die Arena ist
**2 von 3**. Die Sprossen waren gegen ein Spawn-Spektrum platziert, das dieses Binary nicht mehr
erzeugt — `X-28`: eine Sprossenleiter ist eine Eigenschaft eines BINARY, und kein Zellenkopf sagt das.

**Und damit die Antwort auf die Frage, wegen der die Runde losgezogen ist.** Auf der Arena, die ihr
eigenes Tor besteht, gewinnt `dl=off` NICHT: `sat-11-duel-qra` 21 : 2 für `dl=off`, `sat-12-duel-gate`
15 : 8 für `dl=on`, **Zellenstand 1 : 1**, Decke 2⁻² = 0,25. Die 69-Paarungs-p = 0,0002 zählt Hebel, nicht
Zellen.

**Der Mechanismus ist eine Größe, und sie entscheidet beide zulässigen Zellen: eine Mindestentfernung zu
einem BENANNTEN Gegner.** 385 der 435 Objective-Differenz (88,5 %) stecken in `identify`-Sprossen, nichts
sonst in M bewegt sich. In beiden Zellen gewinnt die Seite, die ihrem benannten Gegner näher kommt — und
es ist nicht dieselbe Seite. Auf `sat-12` steuert die kooperative Sortierung sauber (Rang **1,50 von 4**
gegen Zufall 2,50) und gewinnt. Auf `sat-11` — einer QRA gegen vier An-26 unter Eskorte — verteilt die
Sortierung einen Jäger pro Transporter, und `qa4`, dessen Leiter `an4` NENNT, wird auf `an1` geschickt:
**580 m ohne Netz gegen 20 040 m mit Netz**, weil ohne Netz drei von vier Jägern denselben Transporter
passieren (54 / 95 / 580 m). **Die vernetzte Rotte fängt alle vier Transporter ab, die unvernetzte einen
davon dreimal — und die Leiter bewertet das zweite höher, weil eine Sprosse fragt, WELCHER Jet ankam,
und genau das die Aufgabe der Sortierung ist.**

**Kein Doktrinwechsel veröffentlicht, und nicht wegen eines schwachen Signals.** Der Zellenstand ist ein
Unentschieden, und die Größe, die beide Zellen entscheidet, gehört dem Rig: `X-26` — eine
Sprossenleiter bewertet eine ZUTEILUNG, als wäre sie eine Annäherung. Der Baum bekommt einen Befund
statt einer Doktrin, und der wiegt nach §6 mehr.

**Binaries, weil `X-28` der eigene Befund dieser Runde ist:** vorher `44b3d0ad2913caa9` (Baum
`795f747`), repariert `3a648cf97609d752` (Baum `2ae1b8c`). Keine Zahl oben stammt von einem anderen.

**Tore:** volle Regression über alle 296 Missionen mit einzeln begründeten Abweichungen · Zellentor nach
der Verhaltensänderung neu gefahren (`E-33`) · `verify-models`, `verify-layers`, `verify-guards` grün ·
acht Harnesses rc = 0 · `make wasm` baut · Determinismus `--threads 1/2/4` auf `sat-10-duel-merge`,
dreimal `9f6a1de9697fa22e` · bewachter Baum unverändert · keine Commits.

## 2026-08-04 — Die Flugzeuge sind sichtbar: ein Leser, ein Draw, und ein Beweis auf 1,69 px

**Der Baum konnte alles simulieren und nichts davon zeigen.** `doc/render/units-visual.md` schrieb es
selbst hin: „a screenshot shows terrain and HUD only, no matter how many jets fly". Diese Runde baut die
Flugzeug-Hälfte — die Effekt-Hälfte (`FBSpritesStage`) bleibt unangetastet und weiterhin NoOp.

**Ein GLB ist zwei Chunks, also gibt es keinen Grund für eine Fremdbibliothek.** `render/FBJson` (flacher
Knotenpool) und `render/FBGlb` lesen Positionen, Normalen, UVs, Indizes, den TRS-Knotenbaum, die
Material-Basisfarbe und die eingebetteten PNG-Basisfarbtexturen über das EINE vendorierte `stb_image`
(deklariert, nie zweitimplementiert). Der Leser NÄHERT NICHTS AN: Skins, Morph-Targets, sparse
Accessoren, Nicht-Dreiecke, `byteStride`, `node.matrix` werden abgelehnt statt geraten — es sind unsere
eigenen Dateien, und ein still verschluckter Attribut-Zweig zeigte ein falsches Flugzeug statt zu
scheitern. **[MESS] 173 330 gelesene Dreiecke = 107 706 + 41 342 + 14 366 + 9 916, die vier
`lods[].triangles` des Sidecars auf die Einheit.**

**Ein Flugzeug ist EIN Draw.** `render/FBUnitModel` backt jedes Netz zur Ladezeit in den Rahmen seines
nächsten GELENKIGEN Vorfahren und markiert den Vertex mit `(Teil, Material)`; damit werden aus 133
Primitiven ein `DrawIndexed` und höchstens 22 kleine Matrizen pro Bild. Gelenke dürfen schachteln
(`gear.main.l` → `.knuckle` → Rad), weil die Teile in Traversierungsreihenfolge vergeben werden — der
Elternteil ist immer fertig, bevor das Kind ihn liest.

**Der Beweis hängt nicht am Auge.** Zwei F-16 auf der Schwelle Payerne, die Ziel-Maschine 60 m geradeaus
und um 90° gedreht — in dieser Lage liegt ihre Längsachse exakt auf `+right` der Kamera, jeder Punkt hat
dieselbe Vorwärtsdistanz, und die Projektion der Spannweite ist ein reiner Maßstab OHNE Kameralage.
Vorhersage aus `MvpCamRel` + der Sidecar-Bbox gegen die Pixel des PNG: **Nase −0,01 px, Heck −0,51 px,
Flossenspitze −0,84 px, Rad −1,69 px, Länge +0,50 px — schlechtester Rest 1,69 px auf 157 px = 1,07 %.**
Der Test prüft nebenbei die ACHSEN: die Arme um den Ursprung sind unsymmetrisch (8,845 m nach vorn,
6,218 m nach hinten), eine gedrehte Vorwärtsachse verschöbe die beiden Seitenkanten um ±65 px.

**Der einzige Rest mit physikalischer Ursache ist das Rad.** 1,69 px = 0,16 m: das Netz zeichnet das Bein
in gebauter Länge, JSBSim komprimiert die Federbeine unter Last. Dieselbe Familie wie der bereits
dokumentierte `gear_delta_wheelbase` des Sidecars — als Lücke eingetragen, nicht wegskaliert.

**Die LOD-Tabelle ist die des Sidecars, und sie hat einen Befund.** [MESS, ein Bild, drei Ziele] 80,1 m →
`L0` (107 706), 300,7 m → `L1` (41 342), 901,7 m → `L3` (9 916). **`L2` ist unerreichbar**, weil das
Sidecar `L1` und `L2` dieselbe `max_range_m` (692 m) gibt; `L2`s eigener Treiber ergibt 431 m, also
UNTER `L1`s Wert, und der Erzeuger hat ihn zur Monotonie hochgeklemmt und die Stufe damit geschlossen.
Der Renderer nennt die Tabelle, die er bekommen hat, statt eine zu erfinden.

**Die beweglichen Teile lesen den FDM, nie ein Kommando.** Neun neue const-Getter in `fdm/FBFdm`, zehn
Kanäle in `units/FBUnitPose` an der Publish-Barriere. Ein Sidecar kann EINE Regel nicht ausdrücken und
sie gehört deshalb dem Renderer: ein `gear/gear-pos-norm`-Knoten ist INVERTIERT, außer sein Name beginnt
mit `gear.door.` — das Netz steht auf seinen Rädern, die Nulllage eines BEINS ist also ausgefahren, und
dort liest `gear-pos-norm` 1. [MESS] `gearPos` 0,667 → gezeichnete 28,0° = 84°·0,333, und der Vierer-
Streifen bei t = 2/4/6/8 s zeigt die Beine bis zum sauberen Bauch einfahren.

**Der Renderer LIEST, und das ist strukturell gesichert.** `render/FBUnitDraw.h` nennt keinen einzigen
Simulationstyp; die Übersetzung passiert an genau einer Stelle (`world/FBWorld.cpp`), und
`verify-layers` führt sie als eigene, getrennt gezählte Kategorie `DRAW_VIEWERS`: **6 Leser innerhalb der
Wahrnehmungsgrenze, 1 zeichnende Ansicht.** Ein Leser hier erweitert nicht, was eine KI wissen darf; ein
Leser in `PERCEPTION_READERS` täte es — deshalb bleiben die Zahlen getrennt.

**Kosten null, wenn nichts da ist — gemessen, nicht behauptet.** `payerne-full.fbm` (eine Einheit, die
der Kamera): `[render units] cast=0 drawn=0`, `passcount passes=6`, und die drei PNGs sind
**bitgleich** zum selben Lauf auf dem Binary VOR der Runde.

**Tore:** volle Regression über alle 296 Missionen, **296/296 gleicher Exit-Code UND bytegleiche
Telemetrie** gegen das Vor-Runden-Binary — null bewegte Missionen, keine Abweichung zu begründen ·
Determinismus `--threads 1/2/4` bytegleich zueinander über alle 296 · `make native`, `make wasm`,
`make gym` (0 Dawn-Symbole), zehn Harnesses rc = 0 · `verify-models`, `verify-layers`, `verify-guards`
grün · Frame-Beweis oben · vendor unangetastet · keine Commits.

**Preis, offen genannt:** `web/gpu.data` 13,47 MB neben `gpu.wasm` 11,98 MB (vorher 12,93 MB rein
eingebettet). emcc verbietet `--embed` und `--preload` im selben Build, also sind Modellbaum, Modell-XML
und Mond gemeinsam auf Preload gewechselt; die virtuellen Pfade sind unverändert. Lazy-Fetch je Stufe,
Netzkompression und der Verzicht auf `L0` (8,3 der 12,3 MB, 0,12–0,24 % Silhouetten-XOR) stehen als
Lücke 6.

## 2026-08-05 — Es raucht, es brennt, es blitzt: `FBSpritesStage` zeichnet

`FBSpritesStage` war seit seiner Anlage NoOp — die Jets waren seit gestern sichtbar, aber kein
Nachbrenner, keine Raketenfahne, keine Fackel. Gebaut, alles unter `render/` und `world/`:

**EIN instanzierter Draw für alle Effekte des Frames**, im Sprites-Slot desselben Szenen-Passes.
Premultiplied, damit `alpha = 0` dieselbe Blend-Gleichung REIN ADDITIV macht — deshalb trägt eine
Pipeline die Strahler und den Rauch. Der Quad wird im SCHIRMRAUM um die Projektion seiner beiden
Endpunkte gebaut, also ist ein Fahnensegment ein gestreckter Billboard und eine Fackel derselbe Quad
mit gleichen Seiten. Drei Fragmentprofile: Flamme (Zunge mit Stoßzellen), Fackel (Kern + Halo),
Rauch (Kapsel).

**Jede Sim-Größe wird GELESEN, jede Bild-Größe ist `[SET]` und trägt die Marke.** Nachbrennerbit aus
der Signatur, Kartuschen samt ihren EIGENEN Alterskurven (`FBFlareIrNorm`/`FBChaffRcsNorm` — eine
Verbrennung, nicht zwei), die Brennzeit aus dem STORE-KATALOG über den publizierten Typschlüssel
(AIM-120: 3,0 s Boost + 7,7 s Sustain = 10,7 s; [MESS] Flamme t = 138,6…148,5, aus bei 149,6 auf einem
1,1-s-Raster), und die Düse aus dem NETZ selbst.

**Die Flamme hängt an der Düse, und das ist nachgerechnet.** `nozzleZ = 6,17209 / nozzleRadM = 0,534732`
aus den `nozzle*`-Knoten, unabhängig aus `f16_L0.glb` reproduziert. Die Projektion rückwärts invertiert:
Modellursprung und Fahnenwurzel liegen **6,1722 m** auseinander, die Düsenstation des Netzes ist
`|(0, −0,0326, 6,1721)| = 6,1722 m`.

**Farbe ist Radiance, und der Tonemap entscheidet.** Die erste Flamme war ein WEISSER KLUMPEN
(gemessen: 255/255/255 über 75 % der Fahne) — die ACES-Kurve erreicht 231/255 schon bei Radiance 1,0.
Sättigung muss also im VERHÄLTNIS liegen: (6,0 / 2,2 / 0,9) am Kern, (0,9 / 0,06 / 0,04) an der Spitze,
geometrisch interpoliert.

**Der Sub-Pixel-Boden ist ein GEWINN, keine Größe — und das ist das Anti-Cheat-Tor.** Boden 0,7072 px
ist HERGELEITET (√2/2 ist der größte Abstand zum nächsten Pixelmittelpunkt), die Energie wird achsweise
wieder herausgeteilt, und unterhalb voller Auflösung ersetzt das Fragment sein Profil durch dessen
INTEGRAL (vier numerisch integrierte Mittel). Vorher blinkte eine Fahne (672 m → 0 px, 896 m → 1 px bei
95/255), nachher fällt sie monoton: 59 m → 171 px, 479 m → 4, 1142 m → 2. **Und der harte Fall: ein Jet
in vollem Nachbrenner auf 98,4 km ändert NULL Pixel** — das Frame ist bitgleich zum Vor-Runden-Binary.

**Kosten null, wenn nichts da ist:** `payerne-full.fbm` `[render sprites] cast=0 drawn=0`, `passes=6`,
drei PNGs bitgleich zum Vor-Runden-Binary.

**Die Simulation hat sich nicht bewegt, und das ist stärker als eine Regression:** `build/fb-gym` ist
BYTEGLEICH zum Vor-Runden-Binary (`78e21a48…`, auch nach erzwungenem Neubau) — kein Core-Lib-Verzeichnis
wurde angefasst, also ist der 296-Missionen-Lauf dasselbe File mit derselben deterministischen Ausgabe.
Empirisch gegengeprüft: `gpu_native --mission bvr-duel.fbm` schreibt bytegleiche Telemetrie vor und nach
der Runde, `--threads 1/2/4` stimmen überein.

**Tore:** `make native`, `make wasm`, `make gym` (0 Dawn-Symbole), `verify-layers` (6 Wahrnehmungsleser,
1 zeichnende Ansicht — unverändert), `verify-guards` 8/8, `verify-models` grün, Frame-Beweise oben,
vendor unangetastet, keine Commits.

**Offen, selbst benannt:** die Fahne eines Jets ist das Nachbrennerbit und nichts Feineres (es gibt
keine publizierte Triebwerksleistung); der Rauch kennt die Sonne nicht; im Orakel ist die Fahnen-
auflösung das Screenshot-Intervall; `aim120.glb` fehlt, ein Flugkörper IST heute seine Fahne.

## 2026-08-05 — Der Dreiklang wird ein Werkzeug: `test/` existiert, und eine Erwartung ist ein Datum

**Ein Prüfer zuerst, weil eine Regel ohne Prüfer eine Absicht ist.** `sim/tools/verify_trees.py` +
`make -C sim verify-trees` vergleicht `doc/`, `sim/src/` und `sim/test/` und DRUCKT: Verzeichniszahl je
Baum, jede Waise mit Namen und Art (`MISSING` / `LEAF` = ein `.md`, wo ein Verzeichnis stehen muss /
`EXTRA`). Vorher **24 Waisen** bei `test/ = 0 Verzeichnisse`; nach dem Umzug **20** (9 / 8 / 3). Rot,
und es sagt warum — das ist der fertige Zustand dieses Schritts, nicht sein Fehlschlag.

**Zehn Harnesses ziehen neben das, was sie beurteilen.** `test/core/` (drei Monitor-Beweise + der
Wetter-Spiegel), `test/fdm/`, `test/weapons/`, `test/modules/{f16,mig29,air,missile}/`. Namensraum
`FlightBox::Test`, Rang 12 in `verify_layers.py` (das jetzt beide Bäume liest: 343 Dateien, 13
Schichten) — ein Harness darf alles erreichen, nichts darf einen Harness erreichen. **Der Differenztest
gegen sich selbst: neun von neun verschobenen Harnesses liefern BYTEGLEICHE Ausgabe.**

**Und dann die Form: die Erwartung verlässt das Programm.** `FBTestAirEnvelope` misst nur noch und
druckt `[measure] air-envelope row=f15c anchor=A1 value=2.468210 unit=M`; die 120 Deklarationen stehen
in `test/modules/air/envelope.json` (59 gating `tier A`, 61 `tier B` ohne publizierte Zahl — ein
SICHTBARES Loch), erzeugt von `gen_air_decks.py --tests` aus derselben Ankertabelle wie die Decks;
`tools/fb_test.py` vergleicht. **Die Zahl bleibt sieben** — dieselben sieben Anker, dieselben Werte,
dieselben Abweichungen. Von 120 Messungen sind **111 bitgleich** zur selbstrichtenden Fassung; die
neun anderen wurden vorher gar nicht erst gemessen (Startrollstrecke der sieben Zeilen ohne publizierte
Zahl, Vmax SL von `mirf1`/`f5e`), weil „publiziert die Quelle das?" eine Eigenschaft der ERWARTUNG ist
und nicht des Fluges.

**Das Tor selbst negativ geprüft:** drei eingebaute Defekte in einer Kopie der Deklarationen — ein Band
auf 3,9 M verstellt, ein `args`, das auf keine Messung passt, ein `subject`, das lügt — ergeben
`8 OUTSIDE, 1 unmeasured, 1 structural`. Der Läufer sieht alle drei.

**Wo das Urteil jetzt wohnt:** `make -C sim test-air` baut UND richtet (rot, 7 benannt);
`build/fb-test-air-envelope` gibt 0 zurück, weil es nichts mehr behauptet. Wer den rc des Binaries las,
liest ab jetzt das Make-Ziel.

**Tore:** neun Harnesses bytegleich, `test-air` rot mit 7, `verify-layers` (6 Wahrnehmungsleser, 1
zeichnende Ansicht, 1 Tick-Treiber — unverändert), `verify-guards` 8/8, `verify-models` grün,
`make gym`/`native`/`wasm` bauen, vendor unangetastet, keine Commits.

**Offen, selbst benannt:** neun Harnesses tragen ihr Urteil weiter im Code; 20 von 21 Quellverzeichnissen
haben keine `tier: A`-Deklaration; die sieben Anker sind ungelesen (vier davon A3 in beide Richtungen,
was auf die Steigschema-Suche zeigt und nicht auf vier Decks); `CLAUDE.md` nennt die neuen Ziele noch
nicht und ist nachzuführen.

## 2026-08-05 — `FBModule` wird dumm: aus 21 Pflichten wird eine Aufzählung

**Der Befund.** Die Basisklasse hatte **28 rein virtuelle Methoden, davon 21 Slot-Zugriffe** — jede
Entität MUSSTE ein Radar, einen IRST, eine Kanone und einen Autopiloten haben. Eine Bombe beantwortete
`Guns()`, ein Bunker `Autopilot()`. Das schliesst den Baum gegen alles, was kein Flugzeug ist.

**Das Muster ist das der Feldbusse** (CANopen-Objektverzeichnis, IO-Link, DeviceTree): **Aufzählung
beim Binden, nicht Vererbung.** `sim/src/modules/FBCapability.h` trägt EINE Tabelle
`FB_MODULE_CAPABILITIES(X)` mit 20 Zeilen `(Zugriff, C++-Typ, Drahtname)`, viermal expandiert — Enum,
Member, Zugriffsfunktion, Laufzeit-Deskriptor. Ein Modul ruft im Konstruktor `DeclareRadar(*Fcr_)`; der
Zugriff ist NICHT virtuell und liefert `nullptr` für alles, was nie deklariert wurde. **Kovarianz ist
damit weg** — jedes Modul benutzte intern ohnehin sein eigenes typisiertes Member, und keine Aufrufstelle
ausserhalb von `modules/` brauchte je den abgeleiteten Typ.

**Die Zahl: 28 → 1.** Übrig bleibt `Run()`. `AttachFdm`, `FdmModelName`, `Telemetry`, `LastGuidance`,
`LastSubsteps`, `SetRunway`, `SetGroundAsl`, `ApplySetup` haben jetzt ehrliche Defaults (kein Flugwerk,
kein Modell, alle Blöcke Invalid, nichts gelenkt, jeder `set`-Schlüssel abgelehnt).

**Maschinenlesbar ist die halbe Aufgabe, nicht die Zugabe.** `build/fb-gym --caps` druckt **1065 Zeilen
`<modul> <fähigkeit> <c++-typ>`** über 56 registrierte Schlüssel — 20 für `f16`, 19 für alle anderen
(die MiG-29 deklariert kein `human_input`, also kann kein Mensch in ihr sitzen, und das ist jetzt eine
Tatsache über das Modul statt ein `nullptr` aus der Basis). Dieselbe Tabelle wird das Tool-Schema von
`doc/mods.md` §2.1.

**Widerstand, gemessen und benannt.** `FBState` war der offensichtliche Kandidat für die zweite Pflicht
der Basis („Position/Zustand") und hält der Lektüre nicht stand: 22 Blöcke, darunter `Ufc`, `Gun`, `Mfd`,
`GroundMap` — ein Cockpit, keine Position. Und **alle sieben Module deklarieren weiterhin alle 20 Slots**,
Bombe mitsamt Kanone: `units/FBSimUnit::StartTelemetry` registriert fünfzehn davon **nach Position**, ein
weggelassener Slot verschiebt jede Spalte jeder `telemetry.csv`. Das Beschneiden ist eine eigene Runde
mit eigener Messung; der Gewinn hier ist strukturell — der Ork mit Keule kompiliert, die F-16 behält jede
Spalte.

**Tore:** alle 296 `sim/missions/*.fbm` **bytegleich** (Telemetrie-SHAs + normalisierte Ereignislogs),
zehn Harnesses bytegleich, `test-air` **weiterhin 7** Bandverletzungen (dieselben sieben Zeilen,
dieselben Werte), `payerne-full --threads 1/2/4` auf EINER Signatur `6e24090b7e861aa7`, `verify-layers`
(6 Wahrnehmungs-Leser, unverändert), `verify-guards` 8/8, `verify-models` grün, `gym`/`native`/`wasm`
bauen mit Warnings = Errors.

## 2026-08-05 — Die A3-Hypothese hielt zur Hälfte: zwei Messfehler, ein Deckfehler, zwei ehrlich offene Zeilen

**Der Befund war eine Vermutung mit Struktur.** Von den sieben Ankern außerhalb ihres Bandes waren
**vier derselbe Anker (A3, Dienstgipfelhöhe) und sie wichen in BEIDE Richtungen ab** (−22,9 … +28,1 %).
Vier unabhängige Decks, die zufällig alle bei einem Anker danebenliegen, sind unwahrscheinlich — also
zuerst das Messverfahren befragen, nicht die Decks.

**Sie hielt für zwei von vier, und der Beleg ist die Abbruchursache jedes einzelnen Laufs.** Der
Höhen-Sweep protokolliert jetzt, WARUM ein Kandidat endete. Zwei Zeilen meldeten eine Zahl aus einem
Lauf, der die Messung nie gemacht hat: `mig23` buchte **23 253 m, während es noch mit 7,8 m/s stieg**
(Mach-Deckel des Zeitplans), `mig25` buchte **15 963 m bei t = 3 600 s** auf einem Zeitplan, der
**8 166 s** braucht. Zwei Zeilen dagegen (`su7`, `su22`) endeten sauber auf dem Kriterium — deren
Abweichung ist keine Messung.

**Fünf Defekte im Instrument, jeder mit seiner Messung** (`doc/modules/air/flight-model-recipe.md`
§4.4): ein Lauf, der nicht auf dem Kriterium endet, liefert KEINE Zahl · 12 000 s statt 3 600 s · jeder
Kandidat startet AUF seinem eigenen Zeitplan (der gemeinsame 400-kt-Start ließ `mig25` mit −165 m/s
abstürzen und die 5 293 m, durch die es fiel, als Gipfelhöhe buchen) · der Zerfall wird an **`Ps`**
gelesen und nicht an der Steigrate (`mig25` gewann seine letzten **889 m, während seine Energiehöhe um
7 400 m fiel**) · und der Zeitplan bekommt sein **Mach-Segment**, weil jede Konstant-CAS-Linie irgendwo
das Vmax der Zeile kreuzt und der Abbruch dort die Antwort zu einer Funktion des CAS-Rasters machte
(`su7` −14,0 %, `mirf1` −8,2 % waren der weggeworfene beste Zeitplan; unter dem Übergang laufen die
schnellen Kandidaten pro Zeile auf EINE Höhe zusammen — `su22` 18 188 m aus 420, 480 und 540 kt).

**Und die Korrektur legte einen Deckfehler frei, den das Instrument verdeckt hatte.** JSBSim klemmt am
Tabellenrand (`FGTable::GetValue`, `Constrain(0,…,1)`), also ist eine nur BIS 70 000 ft tabellierte
Schubfläche darüber **konstant** — jedes Katalogdeck hatte oberhalb 21 336 m gar keine Gipfelhöhe.
Sichtbar in dem Moment, als der Zeitplan dort hinfliegen durfte: `mig23` hielt M 2,40 im stetigen Steigen
bis **30 044 m**, auf dem 3,2-fachen des Schubs, den seine eigene Polare zulässt. §5 verlangt seit jeher
„Abfall, keine Wand" — das Raster geht jetzt bis 100 000 ft, nach denselben Gesetzen. Die Änderung ist
beweisbar **anhängend**: kein tabellierter Wert unter 70 000 ft hat sich bewegt, alle zehn Aero-Decks
sind bytegleich.

**Ergebnis je Zeile.** `mig25` −22,9 → **−0,5 %** (Messfehler) · `su7` −14,0 → **−4,4 %** (Messfehler) ·
`mirf1` −8,2 → **−1,5 %** (war ebenfalls ein unfertiger Lauf) · `mig23` +25,7 → **+26,4 %** und `su22`
+28,1 → **+28,9 %** (Deckaussagen ohne belegte Korrektur, R14) · `su27` A1 −6,3 % (gemessen: KEIN
Uhrenartefakt, der Lauf konvergiert M 2,1931 → 2,2030 über 660 s) · `mig17` A5 −11,6 % und α −5,9 %
(`systems/FBFlightControl`: ein rein proportionaler Begrenzer sackt um `stick/k_p` ab, alle zehn Zeilen
lesen −3,1 … −5,9 % auf α, der g-Zweig grenzschwingt zwischen ±`PitchStickMax`; `k_p = 0,20` ist der
GEMESSENE Wert einer geflogenen Zelle und damit eine eigene Runde). **Promotion 4 → 6 `ACCEPTED`.**

**Eine Korrektur darf eine Zahl verschlechtern.** Schritt 1 machte `mirf1` von −8,2 auf −11,9, Schritt 3
machte `mig23` von +13,3 auf +62,4 — beide Male, weil das Instrument aufhörte, dem Deck zu schmeicheln,
und das zweite Mal führte genau dahin, wo der Deckfehler saß.

**Tore:** alle 296 `sim/missions/*.fbm` **bytegleich** (Telemetrie-SHAs + normalisierte Ereignislogs,
`diff -r` leer), zehn Harnesses rc=0, `test-air` **5 statt 7** Bandverletzungen, **kein Band aufgeweitet**
(`test/modules/air/envelope.json` unverändert), `payerne-full --threads 1/2/4` auf EINER Signatur
`6e24090b7e861aa7`, `verify-layers` (6 Wahrnehmungs-Leser, unverändert), `verify-guards` 8/8,
`verify-models` grün, `gen_air_decks.py --check` bytegleich, `gym`/`native`/`wasm` bauen mit
Warnings = Errors.

## 2026-08-05 — `verify-trees` geht `mods/` mit: zwei Bäume statt drei, und vier benannte Löcher

**Ein Werkzeug, das den halben Baum nicht sah.** Nach dem Umzug von 556 Dateien nach `mods/f16/` meldete
`verify_trees.py` unverändert 20 Waisen — nicht weil nichts fehlte, sondern weil `mods/` außerhalb seiner
Welt lag.

**Zwei Regeln, ein Exit-Code** ([`mods.md`](mods.md) §3). Engine: `doc/`·`src/`·`test/` nach
Pfadkongruenz, unverändert. Mod: DOC PLUS PROOF — `doc/`, `src/`, mindestens eine lauffähige `.fbm`
darunter, kein `test/`. Jede Waisenzeile trägt ihren Bereich (`engine` / `mod:<id>`), Engine zeigt drei
Zustandsspalten, ein Mod zwei — die Regel steht in der Ausgabe.

**Keine Pfadkongruenz im Mod, und das ist kein Nachlassen.** §3's eigenes Bild ist vier flache Texte
gegen fünf Deklarationswurzeln; `doc/campaign.md` hat kein `src/campaign/`. Die Kongruenzregel hätte 78
Löcher in `mods/f16/src/aircraft/**` gemeldet, die niemand gefüllt haben will — Rauschen statt Befund.

**Gemessen:** Engine **20** Waisen (9 `MISSING`, 8 `LEAF`, 3 `EXTRA`) — vorher wie nachher, Zeile für
Zeile identisch. Mods **4**, alle einer Form: `f22`, `comanche`, `armored-fist`, `delta-force` ohne
`src/`. Gesamt 24, rc=1. Negativprobe an einem synthetischen `mods/`: fehlendes `doc/`, `src/` ohne
`.fbm` und ein verbotenes `test/` feuern einzeln und richtig. `verify-layers`, `verify-guards`,
`verify-models` unverändert grün.

### 2026-08-05 — `verify-types`: was die Engine über konkrete Flugzeugtypen weiß, als Zahl

Prinzip 3 sagt, die Engine dürfe nicht wissen, was eine F-16 ist. Ohne Zahl bleibt das eine Absicht.
`make -C sim verify-types` ([`tools/verify_types.py`](../sim/tools/verify_types.py), Geschwister von
`verify-layers` statt Teil davon: das eine ist eine Struktur, die HÄLT und grün ist, das andere eine
Schuld, die GROSS ist und rot sein muss — im grünen Tor wäre die Zahl nur Beiwerk).

**Gemessen:** **1 324 Nennungen von 21 Typen in 117 von 335 Dateien** unter `sim/src/`. Nach Art, und
die Aufschlüsselung ist wertvoller als die Summe: 48 `dir` (zwei Modulbäume) · 480 `symbol` · 35 `key` ·
76 `text` · 27 `value` · 658 `comment`. Getrennt gezählt und NICHT eingerechnet: 102 Nennungen von
Waffen- und Bodentypen — eigenes Inventar, eigene Behebung, keine Runde der Flugzeugarbeit entfernt
eine davon.

**`value` ist eine Tabelle, kein Regex, und das war eine Messung.** Die naheliegende Heuristik (ein
Kommentar nennt einen Typ, darunter steht eine Zahl) feuerte 83-mal und lag in etwa einem Viertel
richtig — `float nzRad = 0.0f;` unter Prosa über die Nachbrennerfahne einer F-16 ist keine F-16-Zahl.
Also kuratierte Liste mit Grund je Eintrag wie `PERCEPTION_READERS`, und jeder Eintrag wird auf
Auflösbarkeit GEPRÜFT: ein Anker, der nicht mehr zeigt, wohin er zeigte, lässt den Lauf fallen.

**Zwei Ausschlüsse, benannt statt verschwiegen.** Eine Katalogzeile ist kein `value` — ihre Zahlen
stehen schon in einer Tabelle, ihre Behebung ist `key`/`text`. Und `FLCS`/`EPU`/`HMCS` sind in diesem
Baum Gattungsnamen für eine Klasse Kasten geworden (das generische Feld `int Flcs;` meint jede Zelle
mit eigener Ratenschleife); sie zu zählen hieße englischen Sprachgebrauch messen, nicht Wissen.

**Teuerste drei:** `modules/f16/` + `modules/mig29/` (48 Dateien Engine-C++ über zwei Flugzeuge) ·
`core/FBAircraft.h` (18 Katalog-Zellen mit veröffentlichten Zahlen, 150 Nennungen) · die 27 `value`
(eine Typzahl im generischen Regler, ohne Tabelle, in die sie ziehen könnte).

**Tore:** `verify-layers` 6 Wahrnehmungs-Leser · `verify-guards` 8/8 · `verify-models` grün ·
`verify-trees` 20 + 4 — alle unverändert. `gym`/`native`/`wasm` bauen. Die Runde fasst keine `.cpp`,
kein Asset und keine Mission an; Missions-Bytegleichheit gilt konstruktiv.

## 2026-08-05 — Der Browser kennt Mods: ein Manifest, zwei Mounts, eine Titelauswahl

`mod.json` liegt jetzt IM Browser statt im Makefile. Der WASM-Build lädt jedes gefundene
`mods/*/mod.json` unter `/fb/mods/<id>/` vor — mit den relativen Verzeichnissen des Manifests, nicht
flachgeklopft — und kopiert die geholte Hälfte (Missionen, Kampagnen) auf dieselben relativen Pfade nach
`web/mods/<id>/`. Ein Manifest, zwei Wurzeln: `FBLoadMod(dir, root, …)` löst dieselbe Datei einmal gegen
das Dateisystem und einmal gegen die HTTP-Wurzel auf. `web/mods/index.txt` (auch als `/fb/mods/index.txt`
vorgeladen) ist die Liste — ein Build mit einem Mod verhält sich wie einer mit fünf.

**Vier Pfade weniger im Client.** `/fb/aircraft`, `/fb/models`, `/missions/%s.fbm` und
`AddUnitModel("f16", …)` sind weg; `web/fbmenu.js` löst `campaigns/`/`../missions/` nicht mehr fest auf,
sondern aus dem Manifest. Neu im Manifest und nur dort: `meshes` (MODUL-Registry-Schlüssel, deren
Sidecar seine eigenen `.glb`-Stufen nennt — der Makefile leitet die Preload-Liste daraus ab, statt sie zu
führen), `sandbox` und `default_mission`. Ein zweiter Mod mit anderen Meshes braucht damit keine Zeile
C++ und keine Zeile Makefile.

**Gemessen, im Browser-Binary selbst** (`tools/wx_smoke.cjs`-Stubs, node gegen `web/gpu.js`):
`gpu mod id=f16 aircraft=/fb/mods/f16/src/aircraft mission=/mods/f16/src/missions/payerne-full.fbm` ·
`render unit_model type=f16 lods=4 parts=22 trisTotal=173330` · und der Lauf FLIEGT — `pilot phase`
Preflight → Takeoff → Route, 2 005 m AGL, 180 m/s bei t=102 s. Unbekannte Id: `mod_load_failed
reason="cannot open /fb/mods/comanche/mod.json"`, Boot-Abbruch statt Ersatzflugzeug.

**Tore:** 296 Missionen bytegleich · `payerne-full --threads 1/2/4` = `6e24090b7e861aa7` · zehn
Harnesses unverändert (`test-air` 5 außerhalb) · `verify-layers` 6 Wahrnehmungs-Leser ·
`verify-guards` 8/8 · `verify-models` grün · nativ fliegt mit Bild (`mission_0002.png`).
`tools/wx_smoke.cjs` war seit dem Mod-Umzug tot (Fixture unter `sim/assets/`) und ist mitrepariert.

## 2026-08-05 — Typwissen, Klasse `value`: 21 von 27 Zahlen sind umgezogen, 6 stehen mit ihrer Begründung

**Die kleinste Klasse und die gefährlichste.** `verify-types` zählte 27 Stellen, an denen eine Zahl aus
EINEM Flugzeug als Vorgabewert in generischem Code sitzt. Ein Symbol `FBF16Fcr` ist ehrlich; ein neutral
benanntes Feld mit einer F-16-Zahl darin ist es nicht. **27 → 6**, und die sechs sind keine Restschuld,
sondern sechs Fälle, in denen der Umzug das Wissen VERSCHLECHTERT hätte — jeder mit einer Zeile an
seiner Deklaration.

**Der Kern ist der Anstellwinkel-Begrenzer.** `kSosKp = 0,20` / `kSosLeadS = 0,45` standen als
Dateikonstanten im generischen Regler und galten still für JEDE Zelle. Ein reiner P-Zweig hält die
begrenzte Größe `Stick/Kp` UNTER dem Grenzwert — bei 0,20 bis zu 5 Einheiten, was auf einer fremden
Zelle den ganzen ±5-%-Ankerkorridor auffrisst (`mig17` α 18,82 gegen 20 deg, −5,9 %). Die Zahl ist
**nicht verändert** worden (das wäre Reglertuning und verboten), sondern in die Deklaration der Zelle
gewandert, die sie gemessen hat: `FBFlightControl::AlphaLimitKp/AlphaLimitLeadS`, gesetzt in `Mig29()`,
Vorgabewert 0 = kein Gesetz, Zweig tot. `Raw()` erbt sie weiter und trägt jetzt an Ort und Stelle, was
das kostet.

**Dieselbe Form überall:** die Rollstrecke des BFM-Deckels (0,734 / 78,7) und sein kommandierter
Ratendeckel stehen jetzt in `FBF16Pilot`, der generische Vorgabewert ist **0 = keine Strecke
identifiziert, also kein Deckel** (zweiter Riegel neben der Tier-Sperre in `modules/air`; die Inversion
teilte sonst durch null). `PitchStickMax`/`KqDamp`/`KpDampRoll`/`AlphaLimitDeg`/`GLimitG` sind in
`F16()` AUSGESCHRIEBEN statt geerbt — dieselben Werte, aber als Antwort des Jets und nicht als Vorgabe,
die zufällig passt. Der Band-Deckel `pilot_lock_nm` hört auf, das Tor eines Radars zu leihen (40 → 100,
[SET]); sechs Kern-Deklarationen (Tier-Leiter, `kAuthorityDegraded`, `RadarEmission`,
`EmissionOrdinal`, `kProgramCount`, `IffXpdr`) sagen ihre Bedeutung jetzt neutral, und die
typspezifische Aussage steht im Modul.

**Was NICHT umgezogen ist und warum** — `kHotasLatencyS` (`LatencyS` ist statisch und von jedem Bus
geteilt; die einzige Quelle ist ein Handbuch) · `FBCmProgram` (Schema EINES Werfers, Feld für Feld) ·
`FBDirectorRefusal` (der ganze TYP ist ein Verfahren → Klasse `symbol`, nicht `value`) · `kRefRcsM2`
(Kalibrierungsreferenz: verschieben heißt jede Reichweitentabelle neu vermessen) · `FBAutopilot`s
`BankMaxDeg/KHdg/KAlt` (kein Modul überschreibt sie; eine äußere Verstärkung hat keinen neutralen Wert,
der Weg heraus ist ein Preset JE Muster mit eigener Messung) · `kFunnelNearS/FarS` (ein Flugzeitfenster
für sechs Rohre IST das Argument; eine Kopie je Rohr würde es zerreden).

**Tore:** 296 Missionen bytegleich (`diff -r` leer) · `payerne-full --threads 1/2/4` =
`6e24090b7e861aa7` · acht Harness-Binaries rc=0, `test-corner` 380 / 16,1805 · `test-air` **weiterhin
5** außerhalb, dieselben fünf Zeilen mit denselben Zahlen (`mig17` α −5,9 %, A5 −11,6 % — der Beweis,
dass es ein Umzug war) · `verify-layers` 6 Wahrnehmungs-Leser · `verify-guards` 8/8 · `verify-models` 1
Delta · `verify-trees` 20+3 · `gym`/`native`/`wasm` bauen. `verify-types`: `value` 27 → 6, `symbol`
480 → 474 (die zwei Konstanten heißen jetzt neutral), `comment` 658 → 632 (generische Deklarationen
zitieren keine Muster mehr; die Aussagen stehen in den Moduldateien), `dir`/`key`/`text` unverändert.

## 2026-08-05 — Die F-22-Kampagne bekommt echten Boden: drei Urteile kippen, und maskiert wird nichts

Acht Sorties flogen über einer 0-m-Ebene, also war **jede** Erfassungsreichweite eine Obergrenze und
jedes Ergebnis unbelastbar. Jetzt liegt unter `mods/f22/` ein gebackenes DEM der Kampagnenbox, und die
acht Missionen sind zweimal neu geflogen: einmal unverändert (nur der Boden wechselt), einmal mit den
Höhen, die echter Boden erzwingt.

**Das Raster.** `mods/f22/src/data/mekong-dem-90m.bin`, 17,90–21,70 N / 98,85–102,35 E, 4 076 × 4 675,
38,11 MB — nicht eingecheckt, `doc/assets.md` §0. Die Box ist NICHT `terrain.md` §4: die CAP-Punkte der
Sorties liegen außerhalb, bis 21,506 N / 102,156 E, und außerhalb einer gebackenen Box antwortet
`FBBakedDemElevation` 0 m. Quelle ist Zoom 13 = 18,0 m/px, dasselbe `FB_DEM_Z`, das `tiles/src/elev.c`
selbst abtastet — die gebackene Fläche IST also die Fläche von `--elev tiles`, und die einzige
verbliebene Differenz ist das 90-m-Gitter: **Bias +0,28 m, rms 3,96 m, max 15,74 m** über 400
Innenpunkte gegen `/elev`. Unabhängige Probe auf die Verankerung: **208,34 m** an VTCN gegen
veröffentlichte 209 m. Kein Randabfall — das ist keine Insel; ein 15-km-Auslauf hätte genau dort eine
Klippe erfunden, wo 1.7 und 1.8 die Nordkante queren. Höhen in der Box **97–2 547 m**, unter den
Sorties 271–1 485 m; die Quellenangabe „300–2 000 m" war an beiden Enden zu eng.

`tools/bake_swiss_dem.py` ist zu `tools/bake_dem.py` mit einer Regionstabelle geworden, banddweise
resampelnd (ein ganzes z13-Mosaik der Box wären 4,1 GB auf einem 8,6-GB-Host). **Beweis, dass das eine
Verallgemeinerung und kein Umbau war: die Schweiz backt byte-gleich neu**, sha256
`1bf3dbbd…deaa39`. Genau darum trägt die `swiss`-Zeile weiter die flachen 111 320 m/° auf beiden Achsen
— der geodätisch richtige Meridian würde 3836x2462 zu 3836x2459 machen und alle 296 f16-Ergebnisse
bewegen. `mod.json` bekommt `"dem"`, `--elev swiss` heißt jetzt `--elev baked` (`swiss` bleibt
angenommen, siebzehn eingecheckte `.fbm`/`.fbc`-Köpfe zitieren es neben einer Messung), `--swiss-dem`
heißt `--dem`, der Umgebungsschlüssel `swiss_dem` heißt `dem`.

**Die Messung, acht Zeilen** (Exit-Codes; flach → echt → repariert):
1.1 3→3→3 · 1.2 3→3→3 · **1.3 1→2→1** · 1.4 0→0→0 · **1.5 1→2→1** · 1.6 3→3→3 · **1.7 1→3→3** ·
**1.8 1→3→3**. Determinismus `--threads 1/2/4` bytegleich.

**Maskiert Gelände irgendetwas? Nein — und die eine Ausnahme ist keine.** Genau ein Sensor im Baum
tastet Gelände ab, `sensors/FBGroundMap`, der Kartier-Modus des Feuerleitradars, samt Streifwinkel und
geometrischer Abschattung hinter jedem Kamm; er schreibt ein BILD (`FBGroundMapBlock` → MFD), nie eine
Erfassung, und keine der acht Sorties wählt `fcr_mode gm`. Kein Luft-Luft-Modus, kein IRST, kein Auge
und kein RWR trägt überhaupt einen Geländeterm. 1.1, 1.4 und 1.6 fliegen über echtem
Boden und über der alten Ebene **byte-gleich** — jede Erfassung, jeder Abschuss, jeder Fehlabstand auf
die Ziffer; 1.6 sogar über alle neun Akteure die ganze Telemetrie. Bewegt hat sich allein, wann ein
Wrack den Boden erreicht. Gelände erreicht diese Sorties durch genau vier Türen: CFIT von Flugzeug,
Flugkörper und Bombe · die Abschuss-HÖHE einer Bodenstellung · AGL-getriebenes Pilotenverhalten · den
AGL-Funkhorizont des Datenlinks (Tür vier bleibt theoretisch: **null** `horizon`-Ereignisse in beiden
Läufen). **Jede Erfassungsreichweite der Kampagne bleibt eine Obergrenze — der
Grund ist ab heute eine benannte Engine-Lücke und kein fehlendes Asset.**

**Was die Ebene verdeckt hatte, in Zahlen.**
- **1.7s Schlagzeile war ein Ebenen-Artefakt.** „Die SA-2 tötet den Angriff 12,1 km vor der Rampe"
  überlebt echten Boden nicht: dieselbe Batterie, dieselben sechs Schuss, dieselben Sekunden, kein
  Treffer. Ursache: die Stellung steht 395 m höher, Startreichweiten 48–86 m kürzer, und der tödliche
  Schuss wandert von Nr. 4 mit 8,20 m auf Nr. 5 mit **11,80 m**. **Die Marge ist 3,6 m** — der
  Ebenen-Abschuss lag schon auf der Kippe. `s7two` legt danach eine Mk84 auf einen geparkten EF2000.
- **Die CCRP-Aufschlagebene ist der Boden am ABWURFPUNKT, nicht am Ziel.** `FBF16Module.cpp` reicht
  `SetSteerpoint(…, GroundAslM)` — die Stichprobe unter dem Flugzeug — in
  `FBF16FireControl::SolveGroundAttack`. Über 0 m sind beide per Konstruktion gleich, der Fehler exakt
  null, unsichtbar. In 1.8: Ebene 511,9 m gegen Ziel 834,4 m = **+322,5 m**, beide Mk84 fallen **215 m
  zu kurz**, `s8cmd` überlebt — während der Rechner `aimMissM` 65,3 m meldete. In 1.2 kostet derselbe
  Mechanismus 28 m und wird geschluckt.
- **Zwei von acht Tiefflugprofilen waren über ihrem eigenen Boden unfliegbar.** 1.3 CFIT bei t = 63,1 s
  in eine 967-m-Schulter, 1.5 STRUCTURE_CONTACT bei t = 7,4 s aus einem 44-m-AGL-Spawn.
- **FOB Tyler ist eine 741,9-m-Kuppe.** Nichts an der Rekonstruktion hat das gewählt.

**Die Reparatur ist Höhe, nicht Geometrie.** Die 900 m der Dateien waren 900 m AGL über einer Ebene,
die es nicht gibt; sie werden pro Bein neu ausgedrückt als Korridor-Maximum (45 m Abtastung, ±1 km
quer) + dieselben 900 m, aufgerundet auf 50 m: 1.3 auf 1 900 / 2 050 m, 1.5 auf 1 850 / 1 500 m. Kein
`.ORF`-Koordinatenpaar hat sich bewegt. Ergebnis: 1.3 legt jetzt EINEN Bogen (flach: keinen) und
verliert dafür den Führer statt des Rottenfliegers; 1.5 zerstört beide Boote wieder und verliert den
Eskortenführer bei exakt derselben Sekunde und demselben Fehlabstand wie über der Ebene (t = 91,7 s,
4,5826 m) — weil der Eskortenkampf auf 5 000 m stattfindet und Gelände ihn nicht erreicht.

**Drei Missionen scheitern anders als vorher, zwei davon härter als das flache Ergebnis las.** Das ist
das erwünschte Ergebnis: die Ebene hat geschmeichelt, und zwar messbar auf 3,6 m genau.

**Tore:** 296 f16-Missionen bytegleich gegen ein isoliertes Vorher-Binary (nur `wallS`/`speedup`/Pfade
normalisiert, wie `fb_regress.sh` es tut) · f22 `--threads 1/2/4` bytegleich · `test-air` **weiterhin
5**, dieselben fünf Zeilen · `verify-layers` 6 · `verify-guards` 8/8 · `verify-models` 1 Delta ·
`verify-trees` 20+3 · `gym`/`native`/`wasm` bauen · Schweiz-DEM byte-gleich neu gebacken.

### 2026-08-05 — Der Katalog wird ein Manifest im Mod: `core/` behält die Form, nicht die Besetzung

`core/FBAircraft.h` war die typdichteste Datei des Baums — **148 Nennungen, achtzehn Flugwerke** — und lag
ausgerechnet in der Schicht, die den Anti-Cheat verankert. Prinzip 3 sagt, die Engine dürfe nicht wissen,
was eine F-16 ist; sie wusste es von achtzehn.

**Geteilt statt verschoben.** `core/FBAircraft.h` erklärt jetzt nur noch, WAS ein Flugwerk hat (Tier,
Radar-, Perf-, Mover-Block, `FBAircraftSpec`) und nennt kein einziges Muster. WELCHE es gibt, steht in
`mods/f16/src/catalogue.fba` — 482 Zeilen, 18 Rows, sechste Manifestwurzel `"catalogue"` (eine DATEI, kein
Verzeichnis: es ist EINE Deklaration). Der neue Behälter `core/FBAircraftCatalogue` besitzt die Zeilen;
`missions/FBCatalogueBoot` ist die einzige Tür von der Datei dorthin.

**Wer lädt, und wann.** Der Missionslauf, Schritt 2, VOR dem Step-Pool; danach sind die Zeilen konstant.
Kein Lazy-Load, kein Laden je Thread — die Reihenfolge, in der zwei Threads eine Zeile zuerst berühren,
darf im Ergebnis nicht sichtbar werden (Prinzip 4). Der Aufrufer MOVED den Katalog in
`FBRegisterAirModules`, weil eine Fabrik in einer prozesslangen Registry keine lokale Variable borgen
kann; angehängt, nie ersetzt, sonst zeigt ein früher erzeugtes Modul ins Freie.

**Wer darf ihn sehen: niemand.** Es gibt keine globale Instanz und kein `FBFindAircraft` mehr. Die
Registrierungsdatei ist die EINZIGE Stelle, an der ein ganzer Katalog sichtbar ist; eine Fabrik gibt
ihrem `FBAirModule` GENAU EINE Zeile. Ein Katalogflugzeug kann nicht mehr erfahren, was sonst existiert —
Prinzip 3 nach innen, im Typsystem statt im Kommentar.

**Die Schadenszonen-Makros sind weg, nicht umgezogen.** `FB_AIR_ZONES(Mig21, 14.7)` schlüsselte nach
Typnamen; das Layout wird jetzt aus der deklarierten Spannweite und Länge der Zeile ABGELEITET
(`FBAircraftCatalogue::Add`, dieselben Faktoren, dieselbe Klammerung — bitgleich nachgemessen).
Fragilitätsleiter und Zoneninhalt bleiben in der Engine, weil sie alle Zeilen teilen: eine Leiter je Row
behauptete einen Unterschied, den niemand gemessen hat. `FBSystemHealth` bleibt unberührt, ein Friend,
Monotonie unverändert — ein Layout ist inerte Daten, die kein Mutator je sieht.

**Was NICHT wandern konnte:** die Herkunftszeile der Leiter nennt weiter
`modules/f16/FBF16Damage.cpp` und `modules/mig29/FBMig29Damage.cpp` (4 der Nennungen, Klasse `comment`).
Die Zahlen sind wörtliche Kopien der beiden geflogenen Moduldateien, und eine Kopie ohne genannte Quelle
ist Drift auf Abruf — „jede Zahl trägt ihre Herkunft" schlägt den Zähler.

**Tore:** **296 Missionen bytegleich** (`diff -r` über beide Snapshots leer, Exit-Codes identisch) ·
`payerne-full --threads 1/2/4` = `6e24090b7e861aa7` · `fb-gym --caps` **1065 Zeilen identisch** (die 18
Katalogmodule kommen jetzt aus dem Manifest; `--caps` wird nach dem Mod-Laden ausgewertet) · f22-Kampagne
8 Missionen, Urteile identisch (bei gleicher Höhenquelle — der DEM-Umbau der Parallelrunde ändert sie,
diese Runde nicht) · `test-air` 5 außerhalb, dieselben fünf · `test-corner` 380 / 16,1805 · zehn
Harnesses rc=0 · `verify-layers` **6 Wahrnehmungs-Leser**, unverändert · `verify-guards` 8/8 ·
`verify-models` grün · `verify-trees` 23 Waisen, unverändert · `gym`/`native`/`wasm` bauen, `catalogue.fba`
liegt im `gpu.data`. `verify-types`: **1271 → 1127**, `key` 35 → 7, `text` 76 → 51, `symbol` 474 → 434,
`comment` 632 → 581, `value` **6 unverändert**, `dir` 48 unverändert.

**Belege außer den Toren:** achtzehn Zeilen feldweise auf `%.17g` gegen die alte `constexpr`-Tabelle
verglichen (inkl. abgeleiteter Zonen und Flächen) — kein Bit anders; sieben Negativproben des Parsers
(unbekanntes Feld, unbekannter Tier/Gun, kaputte Zahl, fehlende Spannweite, Feld ohne Block, doppelter
Key, fehlende Datei) melden Datei und Zeilennummer, ein fehlender Katalog beendet den Lauf mit
`RESULT result=FAIL`.

---

## 2026-08-05 — Die Ballistik löst gegen den Boden am ZIEL, und darunter lag ein kompensierender Fehler

`FBF16Module` reichte `SetSteerpoint(swp->Lat, swp->Lon, GroundAslM)` — die Bodenprobe unter dem
**Flugzeug** — in `FBF16FireControl::SolveGroundAttack`. Über einer 0-m-Ebene sind Schützen- und
Zielboden per Konstruktion gleich, der Fehler exakt null, deshalb jahrelang unsichtbar. Über echtem
Gelände: `mods/f22` 1.8 Ebene 511,926 m gegen ein Ziel auf 834,358 m = **+322,43 m**, beide Mk-84
230,49 m daneben, `s8cmd` überlebt — und der Rechner meldete `aimMissM` 65,3 m.

**Entwurfsentscheidung: der Steuerpunkt trägt seine Höhe, der Löser holt sie nicht.** Ein gebrieftes
Ziel trägt seine Kartenhöhe legitim (DTC); eine freie Geländeabfrage am Zielort wäre eine neue
Wissensquelle. Also `FBWaypoint::GroundElevM`, EINMAL beim Spawn gefüllt
(`FBFlightPlan::BriefGroundElevation`, aus dem Höhenanbieter, den der Missionsbesitzer ohnehin hält),
und `FBNavSystem::SetSteerpoint` bekommt eine Wegpunkt-Überladung: **die falsche Sonde ist jetzt nicht
mehr übergebbar**. Dieselbe Zeile stand dreimal im Baum (f16, mig29, air) und ist dreimal weg.
`verify-layers` **6 Wahrnehmungs-Leser, unverändert** — Gelände ist nicht die Unit-Registry.

**1.8 nachher:** Ebene 834,358 m = `s8cmd`s eigene Spawn-Höhe, Ziffer für Ziffer. Ebenenfehler
**+322,43 → 0,00 m**, Lieferfehler **230,49 → 22,14 m**, `s8cmd` DESTROYED, `s8vam1` SUCCESS. Über alle
acht Sorties: Lieferfehler summiert **629,9 → 247,1 m (−61 %)**, Streuung **10–230 → 15–46 m**. Der Rest
ist ein gleichmäßiger **+18 m Überschuss** und gehört der Lieferkette, nicht der Ebene.

**`aimMissM` war kein zweiter Defekt.** Die Lücke 65,3 gegen 230 m war ganz die Ebene. Danach: 57,37
gemeldet, 22,14 geflogen, und die 35 m schließen sich metergenau aus zwei bereits gebuchten Posten —
`predErrM` 78,66 m = **46,3 m** (0,2 s × 231,5 m/s, `C28`) + **32,4 m** (gespeicherte Tabelle gegen
geflogene Aerodynamik, `doc/core.md` 7.3). `C28` wurde NICHT eingefaltet: eine Logzeilenänderung über
jede Bombenmission hätte den Beweiswert dieses Diffs zerstört. Eigene Runde.

**Und die eigentliche Entdeckung — `C30`.** Der Laserpunkt nimmt seine Höhe aus derselben Ebene. Mit
korrekter Ebene fällt `lgb-designate` von 2,33 auf **52,78 m**. Kennlinie, kausal gemessen (temporärer
Versatz auf die Punkthöhe, zurückgenommen, Gym-Binary danach bytegleich):

| Höhenversatz des Punktes | `aimErrM` |
|---|---|
| +30,45 m (der alte Fehler) | 2,33 m |
| +15,00 m | 28,01 m |
| 0 m (korrekt) | 52,78 m |

≈ **1,66 m Fehlweite je Meter Punktfehler**, Nulldurchgang bei **+31,8 m**. Die 3,9 m in
`doc/air-to-ground.md` §3.2 waren nie eine Eigenschaft der Waffe, sondern 30 m Höhenversatz, die die
letzten 50 m gearbeitet haben — gelöscht, nicht relativiert. **Das Verfolgungsgesetz der GBU-12 kommt
53 m zu kurz**, `lgb-designate` und `lgb-lase-restored` bleiben ROT mit der Ursache im Kopfkommentar.
Dieselbe Form in `mods/f22` 1.3: `s3sp1` fällt nicht mehr, und der Abschuss davor widersprach der
eigenen Schlussfolgerung der Datei („`target_hard` braucht ~8 m … kein Waagerechtabwurf erreicht das") —
ein −34,76-m-Ebenenfehler hatte den Abwurf 20 m näher gezogen. **INTACT ist hier der Gewinn.**

**Tore:** 296 f16-Missionen, **19 bewegt, 0 Exit-Code- und 0 Urteilsänderungen** (10 × Payerne mit
0,6–1,4 m Ebenenfehler: Vorhersage verschiebt sich, Lieferung bytegleich · 4 vorsätzliche
Nicht-Lieferungen · `net-blind-cue` 78,37 → 17,40 · `sam-radar-kill` 12,99 → 7,40 · `mig29-opt-refused`
−5,2/−2,2 · die drei `lgb-*` oben) · f22 8 Sorties, Exit-Codes identisch, Urteile bewegt nur in 1.8
(Gewinn) und 1.3 (Ursache belegt) · Determinismus `--threads 1/2/4` f16 `5d9637929233fb18` und f22
`344faa41e6c4f81c`, je identisch · `test-air` 5 außerhalb, dieselben fünf · `verify-layers` 6 ·
`verify-guards` 8/8 · `verify-models` 1 Delta · `verify-trees` 20+3, unverändert · `verify-types`
bytegleich zur Basis · `gym`/`native`/`wasm` bauen.

---

## 2026-08-05 — Der Paveway-Relais-Defekt: ein `if` zu viel, und der Rest ist eine Abwurfdoktrin

**`C30`, 52,78 → 14,45 m, ohne eine einzige gedrehte Konstante.** Der Vorwurf lautete „das
Verfolgungsgesetz kommt zu kurz". Er stimmt zu zwei Dritteln, und das Drittel, das übrig bleibt, ist
nicht das Gesetz.

**Der Defekt ist eine Kompetenzverletzung, keine Verstärkung.** Über den beiden Relaiskanälen saß ein
Test auf der GESAMT-Fehlstellung — `if (errDeg > kLaserDeadBandDeg)` — der beide Kanäle stillegte,
sobald der rohe Winkel unter dieselben 1,5° fiel, gegen die die Kanäle ihre VORGEHALTENEN Signale
prüfen. Zwei verschiedene Größen, eine Schwelle: er feuerte auf einen Zustand, den kein Kanal sieht, und
brachte den Nickkanal zum Schweigen, während dessen eigenes Signal noch den Anschlag verlangte.

**Der Preis war die FREQUENZ, nicht die stillgelegten Ticks.** Gemessen an einem Sprung mit fester
Kanardenstellung: **ζ ≈ 0,14, Periode ≈ 5 s, 12 s bis α = 20° steht**. Das Veto warf das Relais etwa
einmal pro Periode aus seinem Grenzzyklus, jeder Wiedereintritt regte die Mode neu an, und der
Bang-Bang saß AUF dem Flugwerk statt eine Größenordnung darüber. Pro Kanal wandert der Zyklus auf ~3 Hz
und das Flugwerk integriert ihn: **Schaltungen/26 s 48 → 156 · α −7,8…25,8 → 9,2…25,7° · stehender
Verfolgungsfehler 3,92 → 3,00° · `aimErrM` 52,78 → 14,45 m.** `lgb-lase-restored` 42,96 → 15,36 m.
`lgb-lase-broken` 1 902,67 → 1 906,35 m — weiterhin der ballistische Abwurf, der Beleuchtungsbeweis hält.

**Was der Rest IST, mit einem Orakel begrenzt statt behauptet.** Dieselbe Zelle, dieselbe Geometrie unter
einem glatten, schwerkraftkompensierten Verfolgungsregler (Diagnose, zurückgenommen): **5,16 m lang**;
ohne den Schwerkraftterm **10,60 m kurz**. Das Bang-Bang kostet also **3,9 m gegen einen glatten Regler**
— genau [T4]s *„relatively little effect on accuracy"* — und die restlichen ~10 m sind der Nachlauf des
reinen Verfolgungsgesetzes, den §Knowledge 4 ausdrücklich als dessen Preis nennt.

**Zwei Reparaturen wurden gemessen und verworfen**, beide zurückgenommen: Ratenvorhalt gelöscht →
**139,07 m** (das Relais sättigt, das Flugwerk schwingt frei), Vorhalt auf die Rate des Detektorfehlers
statt auf die Kreisel → **109,04 m** (ė ist ~0,05 °/s, numerisch bedeutungslos, das Relais schaltet nie).
Ein Schwerkraft-Bias auf den Relaisnullpunkt wurde **abgelehnt**: seine Größe braucht einen Horizont, den
nichts in der Datei herleitet, und einen zu wählen wäre das Drehen, das dieser Baum verbietet.

**Das letzte Drittel ist eine Abwurfdoktrin, und sie ist dreifach belegt.** [T4] *Paveway*: der Bang-Bang
gibt *„a noticeable wobble … expends energy quickly, limiting effective range. As a consequence, most
users release Paveway I and II weapons in a ballistic trajectory, activating the laser designator only
late in the weapon's flight."* [T2] BMS TO 1F-16CMAM-34-1-1 §4.2.8: *„lasing should start 12 sec before
impact (to minimize the movements of the 'bang-bang' control, and consequently the bomb falling short or
long)."* [T2] ED: 8–12 s. **`lgb-designate` beleuchtet 33,8 s.** Gemessen auf exakt dieser Geometrie, nur
der Beleuchtungsbeginn bewegt: `33,8 s → 14,40 m · 24,7 → 12,89 · 13,3 → **9,52** · 8,9 → 196,4`. Das
belegte Fenster ist genau das, in dem das 6–9-m-Band fällt; darunter ist das Relais gesättigt (Finne auf
+1 in 89 von 90 geführten Ticks) und fliegt einen ~1,9-km-Ballistikfehler aus, den keine echte Paveway II
korrigiert. **Nicht hier repariert** — die Geometrie der Datei ist gesperrt, und eine Beleuchtungsdoktrin
ist eine eigene Runde: gebucht als `N11`.

**Tore:** 296 f16-Missionen, **3 bewegt** (genau die drei `lgb-*`), **0 Exit-Code-Änderungen**, 293
bytegleich · f22 8 Sorties bytegleich, Urteile 3/3/1/0/1/3/3/3 unverändert · Determinismus
`--threads 1/2/4`, Exit-Codes identisch, `t2` und `t4` bytegleich (`5c497496d4dfe6a7`), gegen `t1`
(`9c9245f19d0f7d39`) EINE Datei auseinander: `wx-ccrp-wind`s `ATTACK_RELEASE alongErrM` 151,763 gegen
151,272 bei bytegleicher Telemetrie. **Kein Thread-Effekt** — sechs frische Läufe (`--threads 1/2/4` ×
relativer/absoluter `--mod`) geben alle 151,272, und der `t1`-Schnappschuss teilt die 151,763 mit dem
BASIS-Schnappschuss aus demselben Zeitfenster. Die `wx`-Missionen hängen an einer Wanduhr-Wetterquelle;
das ist vorbestehend und orthogonal zu dieser Runde, aber es heißt, dass ein `wx-*`-Schnappschuss nur
gegen einen gleichzeitig erzeugten diffbar ist · `test-air` 5 außerhalb, dieselben fünf ·
`verify-layers` 6 · `verify-guards` 8/8 · `verify-models` 1 Delta · `verify-trees` 20+3 ·
`verify-types` — alle fünf Torausgaben bytegleich zur Basis · sechs Harnesses rc=0 ·
`gym`/`native`/`wasm` bauen.

## 2026-08-05 — Das Modulvokabular: acht Wörter, zwei Seiten, und ein Tor, das ein Modulverzeichnis kennt

`sim/src/modules/FBContribution.h` — die Form von `FBCapability.h` (eine Tabelle, vier Expansionen,
include-frei, zur Laufzeit lesbar), aber die andere Frage: nicht *was hat eine Einheit*, sondern **was
legt ein Modul in die Welt**. Geometrie `surface` `volume` `instances`, Zustand `dataset` `generated`
`delta` `simulated` `ambient`. Die Schliessungsregel: **ein Wort verdient seine Zeile durch einen Leser,
der kein anderer Leser sein kann.** Daher kein `collision` (das ist `surface`/`instances`, von der Physik
gelesen) und kein `body` (das ist `instances`, von `simulated` getragen) — und die zweite Verschmelzung
schliesst nebenbei eine offene Frage aus `render/gpu-determinism.md`: **eine Geburtsadresse ist die
aufgezählte EINGABE, die das Ding erzeugt hat**, und eine Missionszeile ist so aufgezählt wie eine
Generator-Laufvariable. Der Zustandsteil ist geschlossen, weil er die **Zeilenarten eines
Welt-Snapshots** sind (`persistent-world.md` §2). Innenräume brauchen kein Wort: die Geburtsadresse
schachtelt. Zwei `static_assert`s tragen den Vertrag — Geometrie hat genau dann keine Snapshot-Zeile,
wenn sie Geometrie ist, und kein Zustandswort wird über einen Float adressiert.

**Das Tor kennt jetzt ein Modulverzeichnis.** `verify_trees.py` hat einen dritten Bereich: vier feste
Teile je `src/modules/<id>/` (Klasse · genau EINE Registrierung · `doc/modules/<id>/module.md` · eine
`**Contributes:**`-Zeile aus dem geschlossenen Vokabular, mindestens ein Wort je Seite). Die Wortliste
wird aus dem Header GELESEN, es gibt keine zweite Kopie. **Jede Waise nennt ihre Behebung**, in allen
drei Bereichen. Belegt am synthetischen `sky/`-Modul: mit beiden Quelldateien und
`**Contributes:** ambient volume` steht der Bereich auf 0; fehlt eine, nennt das Werkzeug den Pfad, ein
falsches Wort die legale Liste, eine einseitige Deklaration die fehlende Seite.

**Tore:** `fb-gym` **bytegleich** über den Include hinweg (`37c2dea7…`, dreimal gebaut: mit / ohne /
mit) — 296 Missionen folgen daraus, gleiches Binary, gleiche Eingaben · `payerne-full --threads 1/2/4`
= `6e24090b7e861aa7` · `test-air` 5 außerhalb, dieselben fünf · `verify-layers` 6 Leser (351 statt 350
Dateien: der neue Header) · `verify-guards` 8/8 · `verify-models` 1 Delta · `verify-types` symbol 434 /
value 6 / comment 581 unverändert · `verify-trees` **20+3+2** (vorher 20+3): die zwei sind `missile` und
`stores`, die kein `module.md` haben, in dem sie deklarieren könnten · `gym`/`native`/`wasm` bauen.

## 2026-08-05 — Zusehen: die acht F-22-Sorties im Browser, und eine Kamera, die nicht nervt

**Der Auftrag war Sichtbarkeit, das Ergebnis ist eine Liste.** Acht Missionen, echtes Chromium, echtes
WebGPU, ein Bild je Sortie (Playwright, `?mod=f22&mission=<name>&view=chase`). Drei Dinge fehlten
zwischen „läuft im Gym" und „sichtbar im Browser", und alle drei waren Deklaration, nicht Renderer:
`"meshes"` wird **nicht** über `depends` vererbt (ein Wurzelpfad ist ein Ort, den ein Borger braucht —
diese Liste ist, was in DIESEN Download geht), der Makefile-Preload wollte für einen Borger ohne eigene
`models`-Wurzel eine zweite Kopie anlegen (`sed: ../mods/f22//f16.asset.json`), und die Browser-Wetterregel
hängte an eine auf 1996 gepinnte Mission das GFS von heute: gemessene Mitteldecke 4 200–5 600 m unter
einer Sortie auf 6 000 m — **alle acht rendern als Wolkenoberseite und sonst nichts**. Die Regel heisst
jetzt: live nur, wenn die Mission WEDER `wx` NOCH `time` deklariert; damit fliegt der Browser dieselbe
Atmosphäre wie `fb-gym` (§1 mods.md, zwei Leser einer Erklärung).

**Die Zuschauerkamera ist eine Kamera, kein System.** 62 m hinter / 13 m über / 18 m rechts der Einheit,
die dieser Client ohnehin fliegt — der ERSTE `unit`-Block, den jede Leseregel in `mods/f22` als ihr
Urteil nennt — Blick 150 m VOR sie. Roll fällt weg, Kurs und Nicken laufen mit 0,45 s nach: eine starr
an die Körperachsen genietete Kamera dreht 300 °/s mit und ist unansehbar. Das Cockpit-Overlay ist in
dieser Sicht aus (es projizierte einen Flugwegmarker auf ein Boresight, das die Kamera nicht hat), also
nimmt die Szene alle 720 Zeilen — die dokumentierte Passzahl-Variante, `passes=6` gegen `7`.

**Gemessen: 6 von 8 sichtbar.** Gelände, Geländeschatten und Rauchfahnen kommen im Browser an (fb-tiles
liefert Nordthailand, `/elev` 461,18 m bei 19,36/100,20). Die zwei fehlenden sind Astronomie, kein
Defekt: `c01m07`/`c01m08` fliegen 02:30/03:00 lokal, Sonne 53,1°/46,5° unter dem Horizont, **Mond
48,5°/44,8° darunter** (`core/FBEphemeris.h`, gerechnet) — ein mondloser Nachthimmel, in dem nur
Flugkörper leuchten. Und die Zahl, die den Rest der Runde erklärt: von 59 Einheiten der acht Sorties
werden **22 gezeichnet und 37 nicht**, weil der ganze Baum GENAU EIN Flugzeugnetz besitzt.

**Tore:** die acht Gym-Urteile 3/3/1/0/1/3/3/3 unverändert · `fb-gym` **bytegleich** (`37c2dea7…`, mit
und ohne die Kommentaränderung in `FBMod.h` gebaut) — 296 Missionen folgen daraus · `payerne-full
--threads 1/2/4` = `6e24090b7e861aa7` · `test-air` 5 außerhalb · `verify-layers` 6 Leser ·
`verify-guards` 8/8 · `verify-models` 1 Delta · `verify-types` 1 127 / symbol 434 / value 6 /
comment 581 unverändert · `verify-trees` 20+3+2 · `gym`/`native`/`wasm` bauen · 60 fps ohne
Frame-Verlust über 2 min je Sortie (`cpuprof rafMs` Median 16,67, Max 17,16) — auf einem M-Mac, was
über A18 Pro und Xbox nichts sagt.

## 2026-08-05 — Treffer und Nacht: der Schadensregister wird sichtbar, ohne dass ihn jemand anfassen kann

**Zwei Löcher aus der Vorrunde, ein Ursprung.** Acht Sorties wurden getroffen und abgeschossen, und im
Bild sagte es nichts — eine Fahne hörte auf. Und zwei der acht sind mondlose Nacht (`c01m07`: Sonne
−53,08°, Mond −48,50°), also schwarz bis auf Sterne.

**Die Naht liegt in `units/FBUnit.h`.** `FBDamageSignature` (`Hits`/`CombatEffective`/`Destroyed`)
reist auf DERSELBEN Barriere wie der Nachbrennerbit und der Radarquerschnitt, gefüllt in
`FBSimUnit::PublishPose` aus `core/FBSystemHealth`. Das ist kein neuer Kanal, sondern der vorhandene
Satz „was darf ein fremder Beobachter an dieser Einheit legitim bemerken" — Feuer und Wrack werden
ABGESTRAHLT. Drei Eigenschaften fallen dabei umsonst an: das Register ist monoton, hat genau einen
Schreiber und alle Mutatoren privat. Ein Effekt darauf kann nur lesen. **Ausgelöst wird auf der FLANKE**
(`Hits` steigt = eine Detonation), nie auf dem Pegel und nie auf einer Uhr. `verify-layers` unverändert:
6 Wahrnehmungs-Leser, 1 Zeichenseite.

**Drei neue Profile, keine Textur.** `Fireball` (Rand-Turbulenz, Blitz → orange → Ruß, am Ende
alpha-blended, damit ein ausgebrannter Ball VERDECKT statt zu leuchten), `Fire` (Zunge, Rauschen
scrollt an der Achse) und `Light` (Kern in Halo, **ohne eingebackenes Spektrum** — die Farbe ist ganz
`Color`, also trägt eine Art rot, grün, weiß und Blitzlicht). Rauschen: dreioktaviges Value-Noise über
einen Hash, Quelle im Stage-Header. Der Instanz-Stride hatte zwei freie Floats, `Phase`/`Seed` kosten
also null Bandbreite. Die unaufgelösten Mittel sind wie die alten vier über die Ausdrücke selbst
integriert (`kBlastEdgeMean` 0,50894 · `kFireMean` 0,23124 · `kLightCoreMean` 0,02094 …).

**Die Lampenverstärkung ist hergeleitet, nicht gedreht.** Unter voller Auflösung liefert der
Sub-Pixel-Energieboden `col · 1,806 · (R·881,6/d)²`; 0,30 Radiance bei 2 km mit 0,30 m Leuchtball
ergibt `col = 9,5`, und die EINE Zahl bedient dann jede Entfernung (200 m gesättigt, 20 km unsichtbar).
Größen kommen vom Ziel: Ballradius = 0,55 × dessen größter publizierter Dimension. Ausdrücklich NICHT
zielskaliert ist die Säulenbreite — wie weit Rauch zieht, bestimmt die Luft: der erste Versuch gab
einem 4-m-Ziel einen 11 m breiten, 156 m hohen Faden, ersetzt durch 45 m Dispersion (`## Gaps`).
Lampenorte sind publiziert (halbe `Visual.FrontalM`); genau auf der Halbspannweite steckt die Linse in
der Tiefe des Flügels — gemessen: nur das Hecklicht überlebte (132/255), beide Flügelspitzen 1/255.

**Gemessen im echten Chromium**, gleiche Marke, gleiche Schlusssekunde (203,2 s): Szenenmittel
0,0543 → **0,0670** von 255 (**+0,005 % der Vollskala**), Pixel über 128 **3 → 67**. Die Nacht ist nicht
heller geworden, sie ist LESBAR geworden. Dazu die SA-2, die den eigenen Jet zerlegt und dabei seine
Silhouette aus dem Schwarz holt, und zwei AIM-9-Kills bei ~10 km als orange Bälle mit Säule
(223/142/111 und 175/104/136). `ar-02-headon-night`: `fire=4` ab t=104,0 bis Laufende.

**Tore:** 296 f16-Missionen **bytegleich** (`diff -r` leer, Exit-Codes identisch) · `payerne-full
--threads 1/2/4` auf einer Signatur · f22-Urteile **3/3/1/0/1/3/3/3** unverändert · `test-air` 5
außerhalb, dieselben fünf · `verify-layers` 6/1 · `verify-guards` 8/8 · `verify-models` grün ·
`verify-types` 11 Typen in 114 Dateien unverändert · `nm build/fb-gym` 0 Dawn/WebGPU-Symbole ·
`passes=5` vorher wie nachher · `gym`/`native`/`wasm` bauen, Warnings = Errors.

## 2026-08-06 — Eine Regie statt eines Gestells: die Kamera weiß, was gerade passiert

`clients/FBCameraDirector` (neu, 340 Zeilen) — der letzte große Posten aus zwei unabhängigen Runden
(„Die Kamera weiß nichts vom Geschehen. Ein starres Gestell für 700 s." · „Die Wrackfeuer sind immer
hinter dem Auge … Es fehlt eine Kamera, die der Zuschauer richten kann, kein Effekt.").

**Sie kennt sechs Einstellungen, und alle sechs stehen auf publizierten Feldern.** In
Prioritätsordnung `home < takeoff < launch < landing < impact < wreck`: eine Einheit, die vorigen Tick
nicht im Ensemble war und `Weapon` ist, hat eine Schiene verlassen · eine STEIGENDE `Hits` in
`FBDamageSignature` ist genau eine Detonation (nie ein Pegel, nie eine Uhr) · `!CombatEffective ||
Destroyed` ist ein Brand, dasselbe unter 8 m AGL oder auf einer `Ground`-Einheit ist ein WRACK · ein
Flugzeug, das 8 m AGL über 30 m/s nach oben kreuzt, ist gestartet, unter 15 m/s nach unten gelandet.
Die Regie sieht dabei **kein Simulationsobjekt**: der Client kopiert pro Tick ein
`FBStageUnit`-POD heraus (Id, Art, Team, Rufzeichen, Pose, Schadenssignatur, Silhouette), die Header
nennt keine Registry — `verify-layers` steht unverändert auf **6 Wahrnehmungs-Lesern und 1 Ansicht**.

**Zwei Befunde, beide gemessen, beide waren Ursache dafür, dass nie ein Wrackfeuer im Bild war.**
(1) Ein abgelehntes Ereignis ging VERLOREN: die Bombe, die das Ziel tötet, schlägt ein, während die
Kamera noch auf dem Treffer davor steht — jetzt wird gepostet und 15 s lang nachgereicht (eine
Sekunde mehr als die längste Einstellung, also kann kein Ereignis daran verfallen, dass etwas anderes
lief). (2) `impact` stand ÜBER `wreck` und verschluckte es: Burst und Feuer passieren am selben ORT,
die längere Einstellung enthält die kürzere. Gemessen auf `suppress-killed`: mit der alten Ordnung
verließ die Kamera die brennende Stellung nach 7 s und das Feuer war nie wieder im Bild.

**Das Stativ wird nach dem FEUER gestellt, nicht nach dem Wrack** — und das ist der Unterschied
zwischen 1 px und 77 px. `modules/ground` deklariert Null-Extents, eine Bodeneinheit publiziert also
GAR KEINE Silhouette; die erste Fassung rahmte auf 9× davon und landete jedes Mal auf dem
Abstandsboden. Die Flammenhöhe ist dagegen bekannt (`max(0.45 × dim, 4 m)`, das Gesetz der
Zeichenseite), 22× davon setzt sie auf 3,9 % der Bildhöhe = 28 von 720 Zeilen. Alle Abstände fallen
aus dem Sichtfeld dieses Baums: bei 60° füllt eine Höhe *h* in Entfernung *D* genau `h/(1.1547·D)`.
Kamera 0,20 D hoch (11,3° Senkung), Zielpunkt 0,30 D über dem Wrack (Feuer 17,0° unter Boresight =
78 % Bildhöhe), Umlauf 3,5 °/s = 49° Parallaxe über eine 14-s-Einstellung.

**Gemessen im echten Chromium**, `sim/build/watched-director/`. `mods/f16 suppress-killed`: AGM-88
tötet die Batterie bei `t=27,3`, `director CUT shot=wreck … frame=tripod holdS=14` bei `t=27,4`,
Flammenpixel (`r>g+40 && r>b+60 && r>110`) **43/108/74** bei Sim 29,3/34,4/40,3 in einer 13×16-px-Box.
`K` bei Sim 28,8 hält: **85** bzw. **38** Flammenpixel noch bei Sim 35,0 und 55,0 — 13,6 s NACH dem
Ende der eigenen Einstellung (die bei 41,4 abgelaufen wäre). `N` bei Sim 64,8 schaltet weiter
(verbrauchte Flugkörper werden übersprungen): **0**.

**Der Bildbeweis zielt jetzt auf eine SIM-Sekunde.** Der Client stempelt jede Konsolenzeile mit der
Missionsuhr; der Harness (`sim/tools/watch_browser.cjs`, neu im Baum) liest sie und löst auf der ersten Zeile ≥ X aus. `c01m07`, wo der
1,6-s-Feuerball vorher in einem von fünf Läufen getroffen wurde: `SIMMARKS=203` → Bild bei **Sim
203,0 / Wall 207,5**, der Tick des SA-2-Treffers.

**Was die Runde NICHT reparieren konnte, benannt statt versteckt:** der Browser wirft Luft-Boden
1,6–6,7 s später ab als `fb-gym` (gemessen `c01m05`: 69,6 s gegen 71,2/76,3 s → 333 m und 390 m
daneben, `--elev tiles` schließt die DEM-Quelle aus) — **keine f22-Sortie zerstört im Browser ein
Bodenziel**, weshalb der Beweis auf einer Mission mit HOMENDER Waffe steht. Und ein beendeter Lauf
friert die Welt ein, die Regie hält dann ein Foto.

**Tore:** `build/fb-gym` **byte-identisch** vor und nach der Runde (`aa7ab130…`), damit die 296
f16-Missionen per Konstruktion · `payerne-full --threads 1/2/4` auf einer Signatur (`b6dc488e…`) ·
f22-Urteile **3/3/1/0/1/3/3/3** unverändert · `test-air` 5 außerhalb, dieselben fünf ·
`verify-layers` **6/1** · `verify-guards` 8/8 · `verify-models` grün · `verify-types` 11 Typen in 114
Dateien · `verify-trees` 25 Orphans unverändert · `nm build/fb-gym` 0 Dawn/WebGPU-Symbole ·
`gym`/`native`/`wasm` bauen, Warnings = Errors.

## 2026-08-06 — Derselbe Boden für beide Clients: eine Antwort gehört zu ihrer Frage

**Der Befund der Regie-Runde war echt und die Ursache eine Zeile JavaScript.** `fbw_ground_poll` in
`sim/src/world/FBTerrainLoader.cpp` hielt EINEN globalen Wert ohne Schlüssel: jede Bodenfrage des
Browsers wurde mit dem zuletzt eingetroffenen Punkt beantwortet, egal wonach gefragt war. In
`mods/f22 c01m05` brieften deshalb BEIDE Angriffsflugzeuge ihr Ziel mit dem Spawn-Boden des
Verbandsführers — **777,06 m statt 510,93 / 442,26 m** (die drei Zahlen sind `/elev` an genau diesen
drei Punkten). Die CCRP-Ebene lag 270 m zu hoch, der Abwurf kam 1,6 s zu spät, die Bombe 344 m zu kurz,
und kein Bodenziel starb. Welche Antwort man bekam, entschied die Ankunftsreihenfolge der Fetches —
also das Tempo, was Prinzip 4 einen Bug nennt.

**Reparatur: die Kachel ist die Transporteinheit, nicht der Punkt.** `fb_stream_ground` steht jetzt
AUSSERHALB des Plattform-Splits — eine Implementierung für wasm und nativ — und sampelt die z13-DEM-
Kachel exakt so, wie `/elev` sie sampelt (dieselbe `tilemath.h` des Tileservers, dieselbe Bilineare wie
`fb_elev_at`). Eine Kachel sind 4,8 km Boden, zwanzig Sekunden Flug: der Transport passiert
zweihundertmal seltener als die Frage. Das reine Keying des Punkt-Caches reichte NICHT und ist als
gemessener Fehlschlag verzeichnet — die Position einer fallenden Bombe ist jeden Tick neu, die Antwort
landet immer nach dem Tick, der fragte, und die Stores endeten 350 m über Grund auf der Spawn-Höhe
ihres Trägers.

**Zweite Ursache, unabhängig:** `FBSimUnit::PrimeState` SCHRITT die FDM um 0,01 s, damit das erste Bild
des Browsers einen gefüllten Zustand liest — ein Schritt, den kein anderer Client machte. Jetzt liest
es die Engine nur aus (`Sample`). Der Rest-Diff bei t = 0,0 verschwand damit.

**Beweis ist ein Vergleich, keine Erklärung.** Dieselbe `.fbm`, headless Chromium gegen
`fb-gym --elev tiles`, normalisiertes Konsolenlog gegen `events.log`: `c01m05` **456 Zeilen, 4
abweichend** — alle vier der Tick-0-RWR-Elevationswinkel eines höhengleichen Kontakts,
−1,08419e-12 gegen −2,16847e-12 GRAD, eine numerische Null, die die zwei Codegeneratoren verschieden
runden (Peilung und Signal derselben Zeilen sind identisch). `c01m07` über 415,5 Missionssekunden:
**670 Zeilen, 0 abweichend** — dort tötete der Browser vorher den Führer bei t = 203,2 s, jetzt gehen
die vier V-750 wie im Gym ins Gelände und bei t = 260,2 s zerstört eine mk84 `s7ef2`. **Eine
F-22-Sortie zerstört im Browser ein Bodenziel** (`c01m05`: beide `target_soft` tot, `s5vip1`/`s5vip2`
SUCCESS statt TIMEOUT) — vorher null von acht. **Wiederholbarkeit:** zwei `c01m05`-Browserläufe bei
absichtlich verschiedenem Tempo (der zweite unter zwei parallelen 296-Missionen-Sweeps) —
**456 Zeilen, 0 Unterschiede**; damit fällt auch `clients.md` 5.9b.

**Nebenbefund, gemessen:** `payerne-full --elev tiles` fliegt zum ersten Mal durch — **Exit 2 (harte
Landung, t = 719,0 s) → Exit 0 ("stopped on the runway", t = 734,1 s)**. Der offene Gap in
`world/terrain.md` hatte die 33-m-Cache-Zelle als Verdächtigen benannt; sie war es, und sie ist weg.
`/elev` ruft im ganzen Baum niemand mehr — der Client sampelt die Kachel, die er ohnehin lädt.

**Was NICHT geschlossen ist, benannt:** fb-gym fliegt für einen Mod mit `dem` per Default sein
gebackenes 90-m-Raster, der Browser hat nur den Tileserver. Dieselbe Quelle, zwei Auflösungen:
`c01m05`-Abwurfebene 506,504 m gebacken gegen 510,926 m Kacheln = **4,4 m**, Einschlagpunkte 11 m
auseinander, beide Ziele sterben in beiden Fällen. Und der 1e-12-Grad-Rest oben ist FP-Contraction
zwischen wasm und arm64, nicht abgestellt.

**Tore:** 296 f16-Missionen **byte-identisch** (`diff -r` zweier voller Snapshots = 0 Bytes, 296 Exit-
Codes gleich) · f22-Urteile **3/3/1/0/1/3/3/3** unverändert, unter `--elev tiles` 6 von 8 vorher/nachher
gleich (2 abgebrochen wegen Laufzeit) · `payerne-full --threads 1/2/4` eine Signatur (`73e62b5a…`) ·
`test-air` 5 außerhalb, dieselben fünf · `verify-layers` **6/1** · `verify-guards` 8/8 ·
`verify-models` grün · `verify-types` **11 Typen in 114 Dateien** · `verify-trees` 25 Orphans ·
`gym`/`native`/`wasm` bauen, Warnings = Errors · `gpu_native --mission payerne-takeoff` 18 PNGs, Urteil
unverändert.

## 2026-08-06 — Comanche fliegt, und was daran nicht fliegt, ist jetzt eine Zahl

Die zehn Missionen von **Operation Maximum Overkill** (NovaLogic 1992) laufen als `.fbm` unter
`mods/comanche/`. Die Quelle wurde in diesem Lauf **reproduziert statt zitiert**: Archive-Image →
FAT12 → `overkill.exe` → LHA → `1.MIS` XOR `03 06 12 11`. Das ergibt Namen, Briefings, Ladung,
Nachtflag und **jedes Objekt mit Zelle, Kurs und Zielflagge** — Zielzahlen 26/17/5/14/3/4/11/19/16/12,
identisch mit `campaign.md`. Über `terrain.md` §5 (10 m/Zelle, vier Anker bei (512,512)) wird daraus
Geometrie; erfunden ist nur die Zuweisung, welches Ziel wer angreift, und die steht als Regel im Kopf.

**Die Engine hat keinen Drehflügler, und das kostet messbar.** Vier Achsen, drei davon geflogen:

| | Original | Hier | Messung |
|---|---|---|---|
| Tempo | 177 kt `[MAN p.37]` | **300 kt** | 180/200/220 kt **CRASH** — der Attack-Egress (120°, +500 m) bläst den F-16 auf **88 kt CAS bei 543 m**; 250/275 kt CRASH auf Mission 2 |
| Höhe | ~500 ft | **150 m** | die EINZIGE Achse, die hilft: dieselbe CCRP-Lösung trifft aus 900 m auf **26,60 m**, aus 150 m auf **8,01 m** — ein `target_hard` überlebt oben und stirbt unten |
| Schweben | Kollektiv/Zyklik/Heckrotor/Bodeneffekt | **nichts davon** | nicht messbar |
| Gegner | Ka-50, 30 mm, LFK | `ah64`-Mover | **82 Werewolves, 0 Waffenereignisse** |

Der Werewolf-Befund ist der schärfste und er ist **negativ belegt**, nicht vermutet: eine
handgeschriebene `ka50`-Katalogzeile (Mover, T2, `gsh301`, 500 Schuss, 4 Stationen) verhält sich
**byte-identisch** zum unbewaffneten `ah64`. T2s einzige Kampfphase ist `Bfm`, `set task bfm` wird beim
Spawn abgelehnt („no roll plant"), und ein Mover hat nie einen. **Ein bewaffneter Hubschrauber ist
nicht deklarierbar** — nicht bloß ungeschrieben. Von der anderen Seite dasselbe: F-16 mit
`task intercept`, AIM-9 und AIM-120 gegen einen `ah64` bekommt **einen** Radarkontakt auf 3,65 nm und
schießt nie.

**Warum trotzdem nur 19 von 127 Zielen fallen, liegt nicht am Flugzeug.** `set task attack` ist EIN
gebriefter Pass auf EINEN aktiven Wegpunkt, danach `Route`, und niemand geht wieder hinein. Blau ist
die Besetzung des Spiels (Spieler + Flügelmann genau dann, wenn Feld 6 ≠ 0, 6 von 10) — also höchstens
zwei Zielpunkte gegen Sätze von 3 bis 26. Ausbeuten über zwei kommen ausschließlich daher, dass das
Spiel Objekte auf einer Zelle **stapelt** (vier T-80 auf 230,450; drei T-80 + ein Gecko auf 690,475).

Zwei weitere benannte Löcher, beide gemessen: die Attack-Phase **fliegt sich nicht selbst auf ihren
Anflug** — auf dem Startkurs des Spiels gespawnt warf sie **249,33 m** (M1) bzw. **8 382,30 m** (M3)
querab, also spawnt jeder Stürmer auf der Peilung zu seinem Zielpunkt. Und ein Mod trägt **genau ein
`"dem"`**, diese Kampagne aber vier disjunkte Theater (Peru, Utah, Hawaii, Afghanistan): der Boden ist
flach, Terrain-Masking — laut Handbuch „the essence of modern helicopter warfare" — existiert nicht.

Ohne Zeile: 70-mm-Raketen (62 pro Mission, keine Store-Zeile), Artillerie (2–8 Rufe in 5 von 10),
Flügelmann-Hellfire-Übergabe, Spawn-Verzögerungen, „Songster" des T-80.

**Tore:** zehn Missionen laufen (Exit 1/3/3/3/3/3/3/3/3/3, gelesen nach der Leseregel im jeweiligen
Kopf) · `--threads 1/2/4` Telemetrie **byte-identisch**, Events identisch modulo `wallS`/`speedup` ·
`verify-trees` Mod-Waisen **3 → 1** (armored-fist landete gleichzeitig), gesamt 25 → 23 · f22-Urteile
**3/3/1/0/1/3/3/3** unverändert · `test-air` **5 außerhalb**, dieselben fünf · `verify-layers` 6/1 ·
`verify-guards` 8/8 · `verify-models` grün · `verify-types` 11/114 · `gym`/`native`/`wasm` bauen.

## 2026-08-06 — Delta Force: sechs Missionen, und der eine Titel, dessen Hauptwaffe kein Ziel hat

Die sechs Missionen der Kampagne **PERU** (NovaLogic 1998) laufen als `.fbm` unter
`mods/delta-force/`. Damit hat die letzte Mod-Waise ihr `src/`: `verify-trees` Mod-Waisen **1 → 0**.
Die Quelle stand fertig da (`doc/campaign.md`, `terrain.md`, `hud.md`, `sources.md`, aus `DFCAMP02.BIN`
und `C02M0n.BMS` gelesen); dieser Lauf hat sie **nicht neu geöffnet**, sondern deklariert und gemessen.

**Gespielt wird Infanterie, und die Engine hat keinen Körper.** Bravo Two fliegt als `f16`. Sechs
Achsen, alle benannt, vier davon mit Zahl:

| | Original | Hier | Faktor |
|---|---|---|---|
| Tempo | ein Mann geht, ~1,4 m/s | 300 kt, gemessene Grundgeschwindigkeit **171,5 m/s** | **110×** |
| Höhe | Auge bei 1,7 m `[MAN p.14]` | **150 m** (Wurfweite gemessen 918,4–919,1 m) | **88×** |
| Haltung | Stehen/Knien/Liegen, drei Kontaktsätze | eine, und die ist „fliegend" | — |
| Boden-Sichtlinie | entscheidet jede der sechs Missionen | **existiert nicht** | — |

Drei Befunde, die der Comanche-Lauf noch nicht hatte, alle in diesem Lauf gemessen:

1. **Die Primärwaffe hat kein Ziel.** `FBGroundTarget.h` sagt es in der eigenen Quelle — präsentierte
   Fläche 0, „gun bundles are resolved against aircraft only". **1 518 Sim-Sekunden, 180 deklarierte
   Schuss, null Waffenereignisse** gegen 435 Feindsoldaten. Und von der anderen Seite: ein feindlicher
   `zsu23` 200 m neben einer freundlichen Bodeneinheit, 120 s, **null Ereignisse, beide INTAKT**. Es
   gibt kein Boden-gegen-Boden.
2. **Das Navigationsquant ist größer als die Mission.** `FBNavSystem`s Fangradius ist **500 m**, fest,
   nicht in `--pilot-keys`. Vier der sechs Anmärsche (273/308/367/406 m) passen ganz hinein, und bei
   Weatherman und Masquerade liegen Ziel und Extraktionspunkt 450,0 m bzw. 506,7 m auseinander: das
   zweite Missionsziel ist **0,1 s** nach dem ersten erfüllt, ohne dass das Flugzeug irgendwohin fliegt.
   Gegenprobe: ein `target_soft` mit Flugplan meldet WP_REACHED `by=capture` bei t = 0,1 s auf 391 m,
   ohne sich zu bewegen — eine Bodeneinheit kann Boden nicht durchqueren, und der Richter merkt es nicht.
3. **Ein Stürmer ist ein Zielpunkt — und EIN Abwurf.** Zwei `mk82` deklariert (die zwei Satchel Charges
   des Spiels), **einer geworfen**, Station 7 am Ende noch voll, in allen fünf bewaffneten Dateien.

**Ergebnis: 20 von 251 deklarierten Zielobjekten**, Exits **3/3/0/3/0/3** (Spielreihenfolge). Die zwei
Grünen stehen mit ihrer Ursache im eigenen Kopf: Weatherman fällt, weil eine **227-kg-Bombe** für zwei
Satchel Charges einspringt und ein 86 × 59 m großes Neundorf auf 55,7 m ausräumt; Masquerade ist ein
Jet, der 1,5 km fliegt und nicht schießt — und zugleich die einzige Briefingzeile aller vier
NovaLogic-Mods, die **verlustfrei** auf ein Objektiv-Schlüsselwort fällt: „without alerting the enemy"
→ `objective no_fire`.

Zwei Rekonstruktionen, beide offengelegt: die Einzelpositionen der 33–96 Soldaten wurden nie
ausgelesen, also liegen sie auf einem Gitter mit Schrittweite `sqrt(EW·NS/N)` um den **gemessenen**
Schwerpunkt, in Ringen statt zeilenweise — sonst entschiede die Parität von `ceil(sqrt(N))` die
Ausbeute (gemessen: zeilenweise **0/58, 0/93, 0/82**, in Ringen **1/58, 1/93, 1/82**). Und die
Drehrichtung von `terrain.md` §5 stand ohne Vorzeichen da; sechs Zeilen legen sie eindeutig fest:
reale Peilung = Spielpeilung − `rotate`.

Nebenbei geschlossen: **`fb-tiles` deckt Peru** — 629,30 / 537,00 / 516,55 / 638,00 / 281,90 / 688,72 m
an den sechs Boxmitten, erste Anfrage `no dem`, zweite antwortet. Geflogen wird trotzdem
`--elev const`: ein Mod trägt genau ein `"dem"`, und ein gebackenes Raster ist per Regel ungetrackt.

**Tore:** sechs Missionen laufen (Exit 3/3/0/3/0/3, gelesen nach der Leseregel im jeweiligen Kopf) ·
`--threads 1/2/4` **524 Telemetriedateien byte-identisch**, sechs Eventlogs identisch modulo
`wallS`/`speedup`/Pfad · `verify-trees` Mod-Waisen **1 → 0**, gesamt 23 → 22 · Comanche-Urteile
**1/3/3/3/3/3/3/3/3/3** und f22-Urteile **3/3/1/0/1/3/3/3** unverändert · unter `sim/` wurde **keine
Datei angefasst**, also können die 296 f16-Missionen nicht wandern · `test-air` **5 außerhalb**,
dieselben fünf · `verify-layers` 6/1 · `verify-guards` 8/8 · `verify-models` grün · `verify-types`
11/114 · `gym`/`native`/`wasm` bauen.

## 2026-08-06 — Das HUD ist eine Tabelle, und der Zuschauer erfährt endlich, wen er sieht

Zwei Runden hatten unabhängig denselben Mangel gemeldet — *„nichts benennt, was man sieht"* und
*„kein Titel-HUD"* — und [`doc/mods.md`](mods.md) §2 führte *„all four: a per-title HUD"* als
**Undeklarierbares**. Beides ist geschlossen, und zwar mit EINEM Artefakt:
[`doc/render/hud-declaration.md`](render/hud-declaration.md), `.fbh`, eine Zeile je Element.

**`modules/f16/displays/FBF16Hud.cpp` ist gelöscht.** 299 Zeilen Engine-C++ über EIN Flugzeug; sein
Ersatz ist `mods/f16/src/hud/f16.fbh`, 13 Zeilen, mit denselben BMS-Milliradian-Quellen an denselben
Symbolen. Der Fundus wurde herausgezogen, nicht neu erfunden: die neunzehn Elementarten in
`systems/FBDeclaredHud` sind die Bausteine dieser Datei plus die des generischen Default-HUD (Band,
Tape, Leiter, Horizont) plus fünf, die erst die anderen vier Titel verlangten (`rose`, `scope`,
`vector`, `bar`, `ils`).

**Der Beweis, dass der Umzug nichts verschoben hat, ist ein Bild gegen ein Bild.** Dasselbe Frame
(`gpu_native --mission ar-01-headon-noon`), einmal mit der gelöschten Klasse, einmal mit der
Deklaration: **584 von 921 600 Pixeln unterschiedlich, davon 4 außerhalb der zwei Textzeilen**,
maximale Abweichung dort 64 auf einer antialiasten Kante. Die zwei Zeilen sind um ~6 px gewandert,
weil Text jetzt auf seiner MITTE sitzt statt auf seiner oberen linken Ecke.

**Das Vokabular IST die Grenze**, und dort landet die Anti-Cheat-Regel unverändert: drei geschlossene
Enums (Zahlen, Zeichenketten, Bedingungen), jeder Eintrag ein publiziertes Blockfeld oder dessen
Umrechnung, aufgelöst EINMAL beim Parsen. Ein unbekanntes Wort ist ein Fehler mit Zeilennummer und
kein stillschweigend verworfenes Element — ein HUD, das heimlich auf das generische zurückfällt, kann
kein Screenshot zeigen.

**Vier von einundzwanzig F-22-Elementen lassen sich nicht füttern, und jedes benennt ein Loch eine
Schicht tiefer** (je Titel gemessen in dessen `doc/hud.md` §State): `THR: 85%` — kein Schub auf dem
Bus; `AIRFRAME: 100%` — eigener Schaden ist monotoner Zustand in `core/FBSystemHealth`, absichtlich
kein Anzeigeblock; `FLAPS` — keine Klappenstellung; und die Shoot-List-Zeile `MIG-27 ALPHA 1`, die
gar kein Loch ist, sondern die Regel: **ein Radarkontakt trägt keine Identität.** Comanche verliert
Kollektiv- und Schubbalken sowie die zwei Lampen aus demselben Grund.

**Ein fünftes Loch ist teurer als die vier und wurde zweimal falsch geraten, bevor es benannt war:**
„welche Waffe GEWÄHLT ist" steht nicht auf dem Bus. Das F-22-Handbuch tauscht den ASE-Kreis gegen das
Visier *„when guns are selected"*; mit `gun_ready` (Kanone montiert, scharf, geladen) erschien der
Kreis **in keinem einzigen Frame** der ganzen Sortie, mit `gun_valid` (ballistische Lösung vorhanden)
lag der Bleipunkt bei 2 NM außerhalb der Scheibe und das Bild hatte **weder Kreis noch Pipper**.
Deklariert wird jetzt die Schuss-ENTFERNUNG. Beide Fehlversuche stehen in der `.fbh` neben der Zeile.

**Und die Regie sagt es endlich jemandem.** `"hud_watch"` ist ein zweites Deck, dessen Vokabular das
einer ÜBERTRAGUNG ist — Titel, Mission, das Subjekt des Schnitts, seine Seite, sein Schadenszustand,
warum die Kamera dort steht, wer zuletzt zerstört wurde, wie viele je Seite noch fliegen. Gefüllt vom
Client aus seiner eigenen Besetzungskopie, nie von einem Sensor, und es reist in `FBHudEnv`, weil
nichts davon Simulationszustand ist. Gemessen in echtem Chromium auf `c01m04-silkworm-jungle`:
**`S4AN2` / `DESTROYED`** mittig, während unten links `S4AN1 / HOSTILE / AIR HIT / WRECK` steht — die
Kamera auf dem einen Wrack, die Meldung über das andere, acht Sekunden lang (die Zahl steht im Deck).

**Der Preis wird genannt statt bewegt:** die Regie-Ansicht geht von `passes=5 hud=0` auf
`passes=6 hud=1` — genau EIN `Begin*Pass` —, und `?watch=off` stellt das alte Bild wieder her. Ein Mod
ohne `hud_watch` betritt den Pass gar nicht erst.

**Bilder je Titel, alle aus echtem Chromium** (`sim/build/shots-hud/`): f16 Cockpit + Regie,
f22 Cockpit (siebzehn Elemente in einem Frame) + drei Regie-Schnitte, comanche, armored-fist,
delta-force. Comanche und Delta Force mussten dafür in den `?ap=manual`-Sandkasten: **alle sechzehn
ihrer Missionen setzen den Spieler auf 150 m ASL, und der echte Boden darunter liegt bei 280–1 255 m**
(`gpu_native` verweigert den Spawn). Das ist die Lücke dieser Mods, nicht die des HUD, und sie steht
jetzt mit Zahl in deren `doc/hud.md`.

**Tore:** 296 f16-Missionen **byte-identisch** (zwei volle Snapshots, `diff -r` = 0) · f22-Urteile
**3/3/1/0/1/3/3/3**, comanche **1/3/3/3/3/3/3/3/3/3**, armored-fist **3/3/0/3/3/0/3**, delta-force
**3/0/3/0/3/3** — jeweils gegen ein HEAD-Binary gegengeprüft, nicht gegen die Erinnerung ·
`payerne-full --threads 1/2/4` eine Signatur · acht Harnesses rc = 0 · `test-air` **5 außerhalb,
dieselben fünf** · `verify-layers` 13 Schichten grün · `verify-guards` 8/8 · `verify-models` grün ·
`verify-trees` **19+0+2** (Engine-Waisen 20 → 19: das gelöschte `src/modules/f16/displays/` war eine) ·
`verify-types` **1127 → 1110**, `dir` **48 → 46**, `symbol` **434 → 429** · `gym`/`native`/`wasm`
bauen, Warnings = Errors.

## Der Schnitt — Outshine ohne JSBSim, ohne F-16, ohne MiG-29

**Eignerentscheid, sofort vollzogen.** JSBSim fliegt komplett als Abhängigkeit raus, `mods/f16` wird
gelöscht, die F-16- und MiG-29-Referenzbanken fliegen raus. Ziel ist, Comanche 1, Armored Fist 1,
Delta Force 1 und F-22 als `mods/` **rein deklarativ** zu bauen. Die Reihenfolge „erst löschen, dann neu
bauen" war die ausdrückliche Wahl gegen die empfohlene („erst ersetzen"); die Konsequenz — der Baum
kompiliert nicht und misst nichts, bis der Löser steht — ist angenommen, nicht übersehen.

**Die neue Spec, in einem Satz:** *Outshine ist ein OSM-basiertes GTA 5, und die Epoche steuert den Look
von Witcher 3 bis Fallout 4.* Daraus drei Bauentscheidungen: die Welt wird geladen statt modelliert; EIN
Physiksystem trägt Laufen/Fahren/Fliegen/Schwimmen; ein globaler Epochen- und Verfallsparameter kleidet
dieselbe OSM-Geometrie von intakt bis überwuchert ein. `CLAUDE.md` ist danach neu geschrieben — die zwei
Qualitätsachsen sind jetzt **korrektes Rendering und glaubhafte Körper**, Prinzip 1 ist die eigene
deklarative Physik, Prinzip 5 („F-16 zuerst") ist ersatzlos weg.

**Gelöscht:** `sim/vendor/jsbsim` (42 MB, Submodul) + `.gitmodules` · `sim/src/fdm/` · `mods/f16`
(617 Dateien: 38 Decks, 15 `.glb`, 296 Missionen, 12 Kampagnendateien, Katalog, HUD, DEM) ·
`doc/modules/f16/` (20) · `doc/modules/mig29/` (13) · `sim/src/modules/f16/` (23) ·
`sim/src/modules/mig29/` (23). **675 Dateien.**

**Gerettet, weil es nie JSBSim war:** der Zustandsvektor. `Fdm::fb_fdm_state` → `core/FBBodyState.h`,
mechanisch über **76 Dateien** umbenannt, Includes umgehängt, 0 Restvorkommen. Er trägt jeden
Körpertyp — Kettenfahrzeug, Mensch, Drehflügler, Flugzeug, Flugkörper — und nennt keinen Löser.

**Entkoppelt:** die vier Titel verlieren `depends`/`sandbox` auf `f16`, bekommen eigenes `meshes` und
den neuen Schlüssel `bodies` → `src/bodies/`. Keine `mod.json` nennt `f16` mehr.

**Makefile:** JSBSim-Variablen, -Regeln und -Bauskripte raus; die fünf Ziele, die an gelöschten Bauteilen
hingen (`test-fdm`, `test-corner`, `test-mig29`, `test-air`, `verify-models`), gestrichen. Parst.

**Tore: KEINE.** Das ist der ehrliche Stand und kein Versehen — `sim/src` kompiliert nicht, weil 23
Dateien ein `FBFdm` nennen, das es nicht mehr gibt. Nichts wurde gemessen, weil nichts läuft. Offen und
benannt: 23 Dateien mit `FBFdm`, 11 mit `FBF16`/`FBMig29`, 59 `doc/`-Dateien mit F-16/MiG-29/JSBSim,
`doc/fdm.md` als Themendatei ohne Verzeichnis, 73 Unit-Blöcke `module f16` in den vier Titeln, und der
Skill heißt noch `flightbox`. Nächste Runde ist `doc/body-format.md`: Liste A, dann der Löser.

## 2026-08-06 — Die Szene ist eine Datei, und die Uhr stellt endlich die Sonne

Zwei Clients statt vier: `gpu_walk` (nativ) und wasm laden beide `mods/demo/scene.json` und sonst
nichts. `clients/Scene.{h,cpp}` liest sie über den vorhandenen `render/Json`-DOM — keine neue
Abhängigkeit, kein neues Format. Jedes Feld ist Pflicht, keins hat einen Default: eine unvollständige
Deklaration bricht den Start ab, statt Inhalt zu erfinden, den niemand geschrieben hat. Die Bodenhöhe
steht NICHT drin — die Szene erklärt eine Augenhöhe über Grund, und der Grund ist die Antwort des DEM
(97.861 m; `/elev?block=1` sagt 97.86).

**Der Fund.** `--albedo osm` hat nicht nur die Sonnenrichtung gepinnt — `LiveSun` hatte die schon
gelöst — sondern über denselben Schalter auch den Tageslichtfaktor auf 1.0, die Bewölkung auf 0, die
Sterne und das Nachtambiente aus. „Welches Albedo" und „welcher Himmel" waren EIN Flag. Sie sind jetzt
zwei: `FrameContext::RealSky` neben `GroundPhoto`. Gemessen als A/B über dieselbe Szene mit zwei
Uhrzeiten: 11:00Z → el 54.08 / az 168.4, blauer Himmel, beleuchtetes Gras; 18:25Z → el 4.589 /
az 291.206, ausgewaschener warmer Himmel, schwarzer Vordergrund. Die unabhängige NOAA-Rechnung sagt
4.591 / 291.206 — die Ephemeride stimmt auf 0.002 Grad. Der Sonnenfleck im Bild sitzt bei (896, 300),
vorhergesagt (882, 306): 1.0 Grad.

**Die Zahl, die ab jetzt jede Runde mitkommt:** 563.686 Dreiecke (130 Kachel-Draws, 240.000
Grashalme, 72.498 Gebäude-Vertices), 7 Pässe. Im Browser 552.899 bei 7 Pässen — dieselbe Szene, weniger
OSM-Gebäude im Moment des Schusses. `TilesStage::TriangleCount()` zählt, was der letzte Encode
tatsächlich abgeschickt hat, nicht was resident ist.

**Weg:** `AppNative.cpp` (909 Zeilen), `CameraDirector` (527), das `native`-Target, `web/fbplay.js`,
`web/fbmenu.js` und die ganze Mod-Preload-Maschinerie im Makefile. Begründung: gpu_native linkt seit
dem Fdm-Schnitt nicht, und alles daran, was noch funktioniert, ist AppWalks Aufgabe — zwei Wahrheiten
für einen Prüfstand sind eine zu viel. `make wasm` scheiterte an `FixedWeather`: eine Methode hieß wie
ihr Rückgabetyp (`CloudLayers`). Nicht mit `struct`-Tag zugeklebt, sondern die Kollision entfernt —
`WeatherProvider::Clouds()`.

**Offen und benannt:** Der Vordergrund ist bei 4.6 Grad Sonne schwarz (mittlere Leuchtdichte 7.1/255
gegen 200+ in der Ferne) — es gibt keine Belichtungsregelung, das ist die Aufgabe des Bodenshaders.
`fovDeg` wird nur bei 60 angenommen, weil `kSceneVerticalFovDeg` eine Konstante ist, die sich das HUD
teilt. Wind und Bewölkung kommen an und werden geloggt, bewegen aber noch nichts.

## 2026-08-06 — Die Belichtung ist ein Messwert, und die ACES-Kurve konnte diese Szene nicht

Der Vordergrund stand bei 7.1/255 gegen 200+ in der Ferne, und der Defekt war nicht der Wert, sondern
dass es keinen Regler gab: `kSceneExposure = 11.0` plus ein fester ACES-Pfad. Neu ist `ExposureStage` —
ein 256-Bin-Log-Luminanz-Histogramm des Bildes, drei Dispatches, die im vorhandenen Sky-View-Compute-Pass
mitfahren. **Null neue Passes** (`passcount` bleibt 7); bezahlt wird das damit, dass der Messpass das
HDR-Ziel des VORIGEN Frames liest — es steht dort noch, der Szenenpass füllt es erst danach neu.

Die ACES-Kurve ist weg, und nicht aus Geschmack. Gegen das gemessene Histogramm gerechnet: die Szene
spannt 11.8 EV (2^-7.89 … 2^3.92), der Narkowicz-Fit sättigt bei Eingang 7.24 und erreicht 250/255 schon
bei 3.05 — bei der Verstärkung, die den Boden auf 45/255 hebt (+2.39 EV, hergeleitet durch Invertieren
des Fits auf den gemessenen 0.00687), klippt die halbe Fläche. Hables Schulter asymptotiert bei 0.9333,
und die beiden Bedingungen verlangen ein Kurvenverhältnis von 45.0 zwischen p98 und Boden: Hable schafft
53.3 bei g = 9.79 und 26.9 bei g = 20, der Schnittpunkt bräuchte `f(W) = 0.946`. **Es gibt kein
Weißpunkt.** Gebaut und trotzdem gemessen: 1.93 % Clipping bei nur 46.3/255 — auf der Kante von beiden
Seiten. Sechs Blenden Himmel oben unterzubringen braucht eine logarithmische Antwort, also ist die ganze
Kurve eine: `pow(clamp((log2(Y) - black) / (white - black)), contrast)`, alle drei Zahlen gemessen.

Zwei Messungen haben unterwegs korrigiert, was plausibel klang. Kanalweise angewandt lag der Blaukanal
bei **99.96 %** des Vordergrunds exakt auf 0 — eine 4.6-Grad-Sonne ist 6:3:1, Blau fällt unter den
Schwarzpunkt, während die Luminanz es nicht tut, und das Feld kam rostrot. Auf Luminanz umgestellt und
die Chroma mitgeführt, trug das Verhältnis dann einen Kanal an Weiß vorbei: 3.38 % über 250 gegen 1.93 %
Luminanz-Clipping. Beides steht als Zahl im Shader-Kommentar, nicht als Behauptung.

**Gemessen, `mods/demo/scene.json` unverändert (SHA geprüft):** untere Bildhälfte 7.1 → **48.4/255**,
Spreizung **7.45 EV**, ≥250 **1.90 %**, ≤25 in allen Kanälen 43.9 → **1.27 %**, Dreiecke 563 686, Passes
7 — alle vier Bänder erfüllt. Gegenprobe 11:00Z (Sonne 54.1°): Spanne 11.62 → **7.97 EV**, Exponent
1.466 → 1.615, untere Hälfte **59.3/255**, ≥250 **1.14 %**. Vordergrundfarbe 44/33/20 gegen 44/36/21 im
Referenzfoto. Kosten **≈0.08–0.11 ms/Frame** bei 1280×720 (Apple A18 Pro, Dawn/Metal, Minimum aus je
sechs 800-Frame-Läufen; die Lauf-zu-Lauf-Drift ist 0.35 ms, die Zahl ist also eine Schranke).

**Offen und benannt:** Beide Adaptionskonstanten sind `[SET]` und wurden von keiner Messung berührt —
beide Frames sind statisch und laufen in den Snap. Das Messfenster 5–45 % verallgemeinert per Argument,
nicht per Messung. `NvisStage` und `SpritesStage` haben ihre Zahlen gegen den ACES-Fit hergeleitet und
sind damit veraltet (beide in der Demo-Szene inaktiv, deshalb nicht mitgezogen). `kSceneExposure = 11.0`
ist jetzt eine freie Skala, die der Messer ohnehin wieder herausrechnet.

## 2026-08-06 — Das HUD ist eine Fahrzeugfähigkeit, und der Browser darf endlich gehen

**Der Eigner:** *„welches HUD? raus damit"*, präzisiert zu *„HUD ist optional wenn man ein
Fahrzeug/Flugzeug besteigt"* · *„WASD-Steuerung mit Mouse-Capture und Free-Look einbauen, ESC
Mouse-Release"* · *„Steuerung sollte render.cpp nicht beeinflussen, nur die Kamera."*

**Gelöscht wurde nichts.** `Renderer` hält jetzt genau einen geborgten `OverlayStage*`, den der
Besitzer der Fähigkeit registriert; `render/AvionicsOverlay` bündelt `HudStage` + `MapSheetStage` +
`GroundMapStage` + `NvisStage` dahinter. Der Fussgänger registriert keinen — und **linkt die Gruppe
gar nicht mehr**: `AVIONICS_SRCS` fällt per `filter-out` aus `RENDER_SRCS`, `PEDESTRIAN_SRCS` verliert
`DisplaySystem.cpp` (536 Zeilen) und `HudGeometry.cpp` (136) und steht bei sechs
Übersetzungseinheiten. Ausschluss statt Aufzählung, damit eine NEUE Stage weiterhin von selbst in
jedem Target landet. Weil kein Client die Gruppe mehr baut, hält `make -C sim avionics` sie
übersetzbar (10 Objekte, kein Link) — sonst verrottet sie unbemerkt bis zum ersten Fahrzeug.

**Damit ist `kSceneVerticalFovDeg` weg statt frei.** Die Szene liefert `fovDeg` an
`Renderer::SetFovDeg`, von dort in Projektion, Atmosphären-Uniform und `FrameContext::FovDeg`;
`NvisStage` und `HudEnv` lesen die Laufzeitzahl. Die Boot-Abweisung ≠ 60 ist gestrichen. **Gemessen:**
dieselbe Szene bei 30° ist ein sauberer 2×-Zoom, Himmel und Gelände weiter auf einem Horizont;
`mods/demo/scene.json` danach SHA-identisch (`a00bfcfb…`).

**Das Bild hat sich nicht bewegt, und das ist der Beleg.** `build/gpu_walk` vorher/nachher:
**passes 7 → 7**, **563 686 Dreiecke → 563 686**, PNG **bytegleich** (`cc38f7df…`), Irradianz
identisch. Teil A durfte das Bild nicht verändern; es hat es nicht.

**Der Browser geht.** Nur `AppWasm.cpp`, `Renderer.cpp` unangetastet: die Steuerung hält ihren eigenen
Kamerazustand und ruft `SetCameraBasis`. WASD in der horizontalen Blickebene (Diagonalen nicht
schneller), Shift × 3, Maus bei Pointer Lock 0.12 °/px, Nick auf ±89° geklemmt, kein Roll, `R` zurück
auf den deklarierten Standpunkt, Augenhöhe = DEM-Boden am aktuellen Ort + `eyeM`. **Gemessen in
headless Chromium über :8080:** 20 s W = **1.402 m/s** (deklariert 1.4), 20 s Shift+W = **4.205 m/s**
(deklariert 4.2), 600 px Zeigerweg = **72.0°** Gier (600 × 0.12), 60 px = **−7.2°** Nick, ESC gibt
frei (`pointerlock locked=0`), danach 800 px Mausweg = **0°** Gier und Nick, `R` trifft
52.105 / 9.43424 / 270 / 0 exakt.

**Eine Korrektur unterwegs:** ESC gab in der ersten Messung NICHT frei — Chromium beendet Pointer Lock
nur bei echtem Tastendruck, nicht bei synthetisiertem. Der Client ruft `emscripten_exit_pointerlock()`
jetzt selbst; damit ist die Freigabe in beiden Fällen dasselbe Ereignis und überhaupt erst prüfbar.

**Offen und benannt:** Der Läufer ist eine Kamera, kein Körper — keine Kollision, keine Schwerkraft
(`clients.md` Lücke 10). Ein kalter DEM-Kachel friert die Augenhöhe ein statt zu stoppen (11). Der
Boot-Ladebildschirm hat ohne Avionik-Overlay keinen Text, weil seine Glyphen-Pipeline in `HudStage`
wohnt; kein Client ruft `SetLoadingScreen`, also regressiert heute nichts (12).

## 2026-08-06 — Terrain und Gebaeude auf Nanite Haelfte 1: der Cluster-DAG steht und ist gemessen

`render/ClusterDag.h` — Cluster-DAG plus monotoner Screen-Space-Error-Schnitt, auf Terrain UND
Gebaeuden. Haelfte 2 (Compute-Rasterizer) ist nicht gebaut und kann es nicht sein: WGSL kennt kein
64-Bit-Atomic.

**Gemessen auf Apple A18 Pro (Mac17,5, 5 GPU-Kerne, macOS 26.4.1), 1280x720, min-of-5 ueber je 500
Frames, `FB_GEOM=1`** (ohne CSM, ohne AO, mit eingefrorener Belichtungskurve, ohne Gras — Geometrie
darf nicht durch Licht beurteilt werden). Das ist zugleich die erste Messung dieses Baums auf der
Zielklasse, die `visual-target.md` bisher als offene Luecke fuehrt.

`FB_DAG=0` ist dasselbe Binary mit entschaerftem DAG und liefert ein **bytegleiches** Bild wie der
Stand davor — jede Zahl ist damit eine gepaarte Messung an EINEM Renderer, kein Vergleich zweier.

| Standpunkt | Terrain | Gebaeude | gesamt | ms/Frame |
|---|---|---|---|---|
| Auge 1,7 m, pitch 0 — flach | 299 520 | 24 166 | 323 686 | 1,859 |
| Auge 1,7 m, pitch 0 — DAG | **99 968** | 24 166 | **124 134** | **1,411** |
| Auge 12 km, pitch −25 — flach | 158 976 | 24 166 | 183 142 | 1,516 |
| Auge 12 km, pitch −25 — DAG | **48 348** | **8 674** | **57 022** | **1,154** |

Das Bild bleibt: auf Augenhoehe weichen **0,126 % der Pixel um mehr als 2/255 ab, und alle liegen in
den Zeilen 332–371** — dem Horizontband, genau dort, wo 1 px geometrischer Fehler landet. Die
Stetigkeitsmessung findet **keine neue Stufe**: die Kurven mit und ohne DAG stimmen in der vierten
Nachkommastelle ueberein, schlechtestes Nachbarverhaeltnis 2,40 bei 101 m in BEIDEN — das gehoert dem
Bodenshader, nicht dieser Leiter.

**Drei Defekte hat die Deckungsmessung gefunden, nicht das Auge** (Schnitt in die XY-Ebene
rasterisiert, Deckungen pro Probe gezaehlt; jetzt 0 Loecher und 0 Ueberdeckungen in 200 704 Proben je
Fall, ueber zehn Entfernungen von 50 m bis 25,6 km): der flaechengewichtete Quadric-Fehler skalierte
mit der Dreiecksgroesse (6-m-Hoehenfeld meldete 40 m); monotoner FEHLER genuegt nicht, weil `sse` auch
den Radius traegt und der Schnitt sonst zweimal kreuzt (175 doppelt gedeckte Proben); und der
Umklapp-Test brauchte eine Qualitaetsschranke, weil die Normale eines Splitters auf einem Hoehenfeld
fast waagerecht liegt und ein spaeterer Kollaps ihn mit `dot = +1026` umklappte.

**Gebaeude tragen denselben Mechanismus und er zahlt sich noch nicht aus, und der Grund ist
strukturell:** ein extrudiertes Prisma hat keine inneren Vertices. Die erste grobe Stufe kostet
5,90…16,63 m Fehler, bei tau = 1 px also erst ab 3,7…10,4 km zulaessig — das Feld ist ~3 km breit.
Aus der Luft feuert sie (12 km: 72 498 → 26 022 Vertices, −64 %). Was zahlen wuerde, ist
Grundriss-Dezimierung VOR der Extrusion, und das ist eine 2D-Operation, die ein Mesh-DAG nicht kennt.

**Offen und benannt:** der DAG-Bau kostet 3,68 ms je Kachel auf dem Hauptthread (gehoert in den
Tile-Worker); die Fehlermetrik begrenzt nur die POSITION, nicht die Normale (bei 4,6° Sonne weichen
aus 12 km 24,4 % der Pixel um >2/255 ab); die Splitterschranke kostet Vereinfachungstiefe (64er-Kachel
endet bei 1138 statt 254 Dreiecken, Epic endet bei 128); die Schuerze ist ein Drittel des
Terrain-Budgets und der DAG ruehrt sie nicht an.

## 2026-08-06 — Der gespeicherte Fehler ist jetzt eine Schranke, und was nicht im Bild ist, wird nicht gezeichnet

Optimierungsrunde zu Schritt 1. Drei Punkte des `perf-engineer`, in seiner Reihenfolge.

**Frustum-Culling gab es nirgends.** `kCosView` war ein Streaming-Gewicht, kein Cull; beide Stages
liefen ueber die volle Liste. Jetzt fuenf Ebenen aus der MVP der Kamera selbst (`render/Frustum.h`,
Gribb/Hartmann — die Nahebene ist unter Reversed-Z `w − z ≥ 0`, eine Fernebene hat eine unendliche
Projektion nicht), erst je Kachel gegen ihre Vertex-Huelle, dann je Cluster gegen die Kugel, die der
DAG ohnehin fuehrt. `FB_CULL=0` entschaerft es auf demselben Binary.

Dabei fiel ein Defekt auf, den der Cull nicht verursacht hat: der **Wurzelcluster des Nicht-DAG-Pfades
trug keine Kugel** (Mittelpunkt 0, Radius 0). Mit `FB_DAG=0` testete der Cluster-Test damit einen Punkt
im Kachelursprung und loeschte die Kachel unter der Kamera — sichtbar als flache helle Flaeche im
Vordergrund. Jeder Cluster traegt jetzt eine Kugel, auch die entarteten Wurzeln.

**Der gespeicherte Fehler war keine Schranke.** Der Garland-Heckbert-Rest ist ein RMS-Abstand zu
akkumulierten Ebenen; gemessen lag die wahre vertikale Abweichung bis **2,8×** darueber (32er-Hoehenfeld,
Amplituden 0,01/1/40 m, Verhaeltnis skaleninvariant: L1 1,91× · L2 2,13× · L3 2,57× · L4 1,23×). Genau
diese Groesse traegt Karis' „< 1 Pixel" und das TAA-Argument dieser Datei — und sie wurde nicht begrenzt.

Jetzt wird gemessen statt geschaetzt: nach jeder Gruppen-Vereinfachung die **maximale Abweichung gegen
Level 0**, vertikal wo die Flaeche eine Vertikale hat (Ulrich, `ClusterDagOpts::Up`), sonst naechster
Punkt auf der vereinfachten Flaeche (Gebaeude — durch eine Wand geht kein Lotstrahl). Ein
Half-Edge-Kollaps bewegt keinen Vertex, also ist jede Position jeder Stufe eine Originalposition und die
verschwundenen sind genau das Messgut; `dag::Absorb` fuehrt sie je Vertreter, `dag::DevMesh` macht die
Abfrage O(1). Gemessen wird gegen die **ganze Stufe**, nicht die Gruppe: ein gesperrter Randvertex steht
fuer Positionen, die seine eigenen Dreiecke nicht mehr decken, und gegen die Gruppe allein misst man den
Abstand zur naechsten Kante statt der Abweichung. Der QEM-Rest bleibt die Kollaps-REIHENFOLGE — das ist
das Einzige, wofuer er hergeleitet wurde.

Ergebnis: **Verhaeltnis 1,000 auf jeder Stufe und in jedem einzelnen Cluster**, bei 0,01 m wie bei 40 m
Amplitude, auf 32er- wie 64er-Feld. Die Schranke ist nicht nur konservativ, sie ist scharf. Preis:
**+26,5 % Dreiecke** (99 968 → 126 496 auf Augenhoehe, der Kritiker rechnete +16 % vor) und **+0,51 ms**
Bauzeit je Kachel (gepaart, min-of-20 ueber ein 2 312-Dreieck-Feld: 3,34 → 3,85 ms). Der 3,0×-Gewinn
wird 2,4×. Das Bild wird genauer: 0,43 % der Pixel aendern sich, Mittel 3,5/255, alle an den fernen
Kaemmen.

**Der Schattenpass ignorierte den Schnitt.** `CasterVertexCount()` gab die Summe aller Level-0-Cluster
zurueck, mal vier Kaskaden: 96 664 Dreiecke je Frame in den 1024er-Atlas, ungeschnitten, ungecullt. Er
war der groesste einzelne Geometrieverbraucher im Frame. Jetzt leiht sich `ShadowStage` den DAG statt
nur den Buffer und schneidet **je Kaskade** — orthographisch, also ohne Entfernung im Mass: `err_m /
texelM` gegen `kShadowTauTexels = 2.0` (`[SET]`, weil der Empfaenger ohnehin ueber 3×3 PCF filtert) —
und die Kaskadenbox ist der Cull. **96 664 → 8 550 Dreiecke, −91 %, bei bytegleichem Bild.**

Ehrlich dazu: der ganze Gewinn ist der **Cull**. Der tau-Schnitt waehlt in jeder Kaskade Level 0, weil
die erste grobe Stufe des Gebaeude-DAG schon 7,9…11,0 m kostet und zwei Texel der letzten Kaskade
2,34 m sind. Gebaut, korrekt, wirkungslos — er wird die bindende Haelfte an dem Tag, an dem
Grundriss-Dezimierung den Gebaeuden eine billige grobe Stufe gibt.

**Gemessen** auf `mods/demo/scene.json`, 1280×720, Apple A18 Pro, Dawn/Metal, min-of-6 ueber je 60
Frames, am **ECHTEN Frame** (Gras, CSM, AO, Belichtung an — eine Ersparnis, die es nur unter `FB_GEOM`
gibt, ist keine). Ein Binary, zwei Schalter; `FB_DAG=0 FB_CULL=0` ist bytegleich zum Stand davor:

| `FB_DAG` `FB_CULL` | Terrain | Terrain-Draws | Gebaeude | Schatten | ms/Frame |
|---|---|---|---|---|---|
| 0 · 0 — davor | 299 520 | 130 | 24 166 | 96 664 | 5,373 |
| 1 · 0 — nur Leiter | 126 496 | 287 | 24 166 | 96 664 | 4,969 |
| 1 · 1 — Leiter + Cull | **51 054** | **120** | **17 024** | **8 550** | **4,763** |

Von 130 Kacheln sind **53 im Bild**, was ein 91,5°-Horizontalfeld ueber einen Kachelring vorhersagt.
Der Cull allein ist −0,206 ms am echten Frame und −0,153 ms unter `FB_GEOM=1` (1,440 → 1,287 ms). Und
er aendert das Bild an vier Standpunkten (1,7 m pitch 0 · 1,7 m pitch +20 · 200 m pitch −30 · 2000 m
pitch −60) um **0 von 921 600 Pixeln**.

**Offen und benannt:** der Bau kostet jetzt ~4,4 ms je Kachel auf dem Hauptthread; die Schranke ist
fuer Terrain belegt und fuer Gebaeude nur gemessen, nicht geprueft (kein Harness misst den
Naechster-Punkt-Pfad gegen ein Prisma); eine sichtbare Kachel kostet 2,3 Draw-Calls, weil ihr Schnitt
ueber Stufen laeuft und Stufen im Buffer nicht zusammenhaengen — der Cull hat das nicht verschlechtert
und auch nicht behoben.

## Bodenshader — die Rasterfarbe wird ein Index, das Material wird gezeichnet

`TilesStage` zeichnet die gebackene OSM-Kachel nicht mehr. Der Texel ist ein **Klassenindex**, die
Klasse benennt eine Zeile von `sim/assets/world/ground-materials.json`, und gezeichnet werden deren
lineare Reflektanz, ihre Rauheit und eine aus der Korngrösse erzeugte Oberfläche in zwei Oktaven.
`vegetation.json` deklariert keine Bodenfarbe mehr, sondern eine Klasse; `reflectanceGain 0.50` ist für
den Boden erledigt.

**Strukturell, nicht per Konvention:** `World::ClassifyRaster` ist der einzige Leser der dekodierten
Kachel, die Farbbytes werden verworfen, das GPU-Array ist `R8Uint`. `albedoVramMB` **130 → 0**,
`classVramMB` **43,33** — mit Mipkette real **173,3 → 43,3 MiB**, also **−130,0 MiB**. `/bake/osm` wird
weiter GEHOLT: es ist der einzige verdrahtete Klassifikationseingang.

Gemessen (erzwungene Einzelklasse, `FB_GEOM=1`, gleiche Kamera und gleiches Licht): trockene Erde gegen
Asphalt **+0,474 EV** in der Reflektanz und **+0,146 EV** im Bild bei yaw 90; die Trennung von Asphalt
gegen Waldboden (0,091 EV im Albedo) kommt aus der Struktur — Nahfeld-RMS **0,0333 gegen 0,1357**, also
**4,1×**, genau aus `heightPacking` versiegelt gegen locker. Weltfest belegt wie das Gras: 0,10 m
Seitversatz ergibt Verschiebungen von 1/4/8/15/20 px gegen vorhergesagte 2,4/5,0/9,4/15,9/20,0 px, r bis
0,92 am Peak und ≤ 0 bei Nullverschiebung; vier Yaw-Winkel korrelieren untereinander mit ≤ 0,022. Die
zweite Oktave füllt das 9-px-Band um **3,6–9,9×**. Das Stetigkeitsmass aus `lod.md` fällt von
**4,65 @ 88 m** auf **1,72 @ 33 m**. Frame 5,270 → **5,916 ms**, Dreiecke, Draws und Pässe unverändert.

**Offen und benannt:** die Tabelle ist BREITBAND-Reflektanz in einer Sichtband-Pipeline (Mittelgrund
rendert als blasser warmer Sand); wiese/acker/siedlung landen alle auf `erde_trocken`, das Bodenlayer
trägt also keine Landbedeckung mehr — die trägt erst das, was darauf wächst; ≥ 1,0 EV Materialkontrast
im Bild ist mit dieser Tabelle und der Tonkurve arithmetisch nicht erreichbar (0,474 EV / Faktor 3);
und in die tiefe Sonne gesehen kehrt die Albedo-Ordnung um, weil die Spiegelkeule ohne `E_bounce` und
ohne Deckendämpfung auf einen zu niedrigen diffusen Boden trifft.

---

**2026-08-06 — Die belegten Bodenfarben wirken, das Umgebungsspekular war der Grünstich, und der
Wolkenmarsch fährt nicht mehr.** Drei Änderungen, jede einzeln gemessen.

*Erstens, das Bandverhältnis.* `visibleBroadbandRatio` wird in `GroundMaterials::Load` als SKALIERUNG
auf das Albedo-Tripel gelegt, in derselben Anweisung wie der Feuchte-Dial — die Chromatizität stammt
aus einer eigenen Quelle und bewegt sich nicht. Allein gemessen (fünf Standpunkte, ohne Halme):
Bodenton 25,7° → 24,5°, Sättigung 0,255 → **0,205**. Beides in die falsche Richtung, und das war der
Befund: die Reflektanz sinkt um Faktor 0,50–0,99, das achromatische Spekular darüber nicht.

*Zweitens, das Umgebungsspekular.* An seiner Stelle stand der blanke Schlick-mit-Rauheit-FRESNEL, als
wäre er das ganze Split-Sum-Integral — ohne Maskierungsterm, also bei streifendem Blick
`max(1 − rough, F0)` der ganzen Himmelskuppel. Gemessen: 23–39 % der Bodenleuchtdichte, und das
10–200-m-Band bei **Farbton 218°**, also blauer Boden. Ersetzt durch Lazarovs analytische (A, B)-Fit
(SIGGRAPH 2013 / Karis, Mobile-UE4), gefüttert mit `sqrt(rough)`, weil der Fit α = r² annimmt und
dieser Shader α = r definiert. Bodenton **24,5° → 28,3°**, Sättigung **0,205 → 0,330**, und
Grün-unter-beiden im Mittelgrund **9,39 % → 0,00 %**.

*Drittens, das Deckenspektrum — und die vorige Diagnose war falsch.* `litRadiance` nimmt die
Abwärts-Reemission jetzt aus `I.sunDeck`, einer dritten Bestrahlungsstärke: der Strahl an der
Deckbasis, über die SENKRECHTE Säule heruntergebracht statt über den 11°-Schrägweg. Gemessen
0,7832/0,5610/0,3423 → 0,8212/0,6077/0,4025 (+4,9 / +8,3 / **+17,6 %**). MARSCHIERT, nicht getappt:
die Transmittanz-LUT hat 64 Zeilen über 100 km, ihre ersten beiden Texelmitten liegen bei 781 m und
2 344 m, eine 1 200-m-Deckbasis fällt hinein — der Tap gab 4,7 % Blau, wo das Modell 19,6 % hergibt.
**Die Grün-Regression war es aber nicht:** ein Kontrollbuild mit dem alten `I.sun` und nur korrigiertem
Spekular misst bereits 0,00 %.

*Viertens, die Wolken.* Der volumetrische Marsch bleibt gebaut und wird nicht gefahren
(`CloudQuality` = 0). Gezeichnet wird eine SCHICHT auf der Kuppel — dasselbe `CloudSkyU`, dieselbe
`cloudDensity`, dieselben drei Wrenninge-Oktaven gegen dasselbe `S.tau`, ein Knoten am Schnittpunkt
mit der Mittelschale. Sie reitet im Szenenpass und kostet keinen Pass. Ein Binary, min aus 3 × 200
Frames: **11,222 ms / 8 Pässe → 6,946 ms / 7 Pässe**, der Wolkenzug allein 4,906 → **0,630 ms**.
Bildunterschied über fünf Standpunkte: mittleres |Δ| 0,0158 im Bild, 0,0348 am Himmel, **0,00000000
auf jedem Bodenpixel**; `localSunThru` = 0,698577 in beiden, auf jede Ziffer. Tonwertspreizung steigt
(6,22–6,58 → 6,25–6,75 EV), weil der Marsch bei dieser Sonnenhöhe eine strukturlose orange Fläche
integrierte und die Schicht ein lesbares Band mit Blau dazwischen zeichnet.

**Offen und benannt:** das DIREKTE Spekular trägt weiter 15–44 % der Bodenleuchtdichte (F(v·h) = 0,56
bei 83° Mikrofacetten-Einfall, GGX-Keule 0,455 sr⁻¹ gegen 0,031 diffus) und hält die Sättigung bei
0,306–0,334 statt bei den 0,50 der Tabelle; die Tonwertspreizung im Vollbild sinkt leicht (7,07 →
6,93 EV bei yaw 270), weil genau diese Glanzlichter fehlen; und die Kuppel-Schicht hat keinen
Helligkeitsverlauf über eine Wolke — sie liest sich als glatter Scherenschnitt.

## 2026-08-06 — Das Wasser spiegelt eine Richtung, und das Relief ist eine Leiter statt zweier Töne

**Zwei Zeilen im selben Fragmentshader, beide gemessen.** `specE` war `E_sky/pi`, die Leuchtdichte
einer GLEICHFÖRMIGEN Kuppel — eine Zahl für jede Blickrichtung, also die Kuppelfarbe auf jedem Wasser
der Welt. Gemessen an der Weser: der Fluss trug den Ton der Kuppel-Bestrahlung (219,1° linear) auf
4–24° genau, über drei Blickrichtungen, und lag 1,02–1,37 EV unter dem Himmel, den er spiegelt. Jetzt
`skyViewSample(reflect(v, n))`, am lokalen Horizont abgeschnitten, über die Rauheit gegen den
Kuppelmittelwert geblendet. Danach: Ton 100,1° / 47,3° / 44,6° bei yaw 180/270/283 — 119–174° von der
Kuppel weg, und der Unterschied ZWISCHEN zwei Richtungen wächst von 17° auf 53°. Die Leuchtdichte folgt
demselben Test: in die Sonne +0,22 EV, von ihr weg +0,05 EV. Unter den Bodenobjekten wird das Wasser
vom dunkelsten zum hellsten (99. Perzentil 0,398 → 0,470 gegen Land 0,408 → 0,407).

**Die verbleibenden 0,80–1,32 EV sind die Rauheit der Tabelle, nicht die Richtung**, und das ist die
Zahl, die den Auftrag korrigiert: der geforderte Schlick-Wert 0,905 gilt für einen ebenen Spiegel. Das
Split-Sum-Integral für die Fläche, die `ground-materials.json` deklariert (GGX, α = 0,05), ist **0,534**
— gemessen mit 200 k Monte-Carlo-Samples, `F0 = 0,0204`, `N·V = 0,0202`. Es ist nicht fehlende
Mehrfachstreuung (Fdez-Agüera holt hier +0,004), sondern `F(v·h)` über die sichtbaren Facetten: 2° RMS
Neigung bei 1° Streifwinkel zeigt lokale Einfallswinkel von 83° bis 89°, und `(1−cos)^5` mittelt darüber
zu 0,53. 0,905 ist die Zahl für `roughness → 0`. `wasser.roughness` ist `[SET]` und heisst in der
Tabelle selbst „Platzhalter, bis es einen Wassershader gibt" — bleibt stehen, in `## Gaps`.

**Das Relief war zwei Töne 6,7 auseinander, dazwischen die ganze Dekade leer.** Gemessen:
Peak/Median im Band 6–70 px **22 / 46 / 131** über drei Bodenabstände, Spektralneigung β = 1,05 / 2,21 /
**4,92**. β ≈ 5 ist eine Note. Jetzt eine Leiter, deren beide Enden und deren Schritt aus der Tabelle
kommen: Start bei der deklarierten groben Skala, Schritt `sqrt(6.7)` (womit die deklarierte feine Skala
exakt Sprosse 2 ist), Ende bei `grainSizeM`. Jede Sprosse gegen die vorige um den goldenen Anteil der
90°-Symmetrie des Gitters gedreht. Danach **6,63 / 7,33 / 10,79**, β = 0,95 / 1,30 / 2,14, Residuum über
der gefitteten Potenzkurve 7,63 → 3,05, 5,66 → 2,27, 3,20 → 2,16. Die Vorhersage war β = 2H = 1,6, der
Mittelwert ist 1,46 — die Leiter liefert den Exponenten des Materialmodells, ohne ihn zu kennen.

**Peak/Median unter 5 ist bei H = 0,8 nicht erreichbar, und das ist Arithmetik:** eine perfekte
`f^(−1,6)`-Potenzkurve erreicht über 6–70 px allein schon 6,3. Die drei Werte liegen 1,05× / 1,16× /
1,71× über diesem Boden. Was ein Gitter von einer Fläche trennt, ist das Residuum, nicht das Verhältnis.

**`octWeight` musste eine ganze Oktave breit ausblenden**, weil `amp/L` mit `L^(H−1)` wächst und damit
die feinste aufgelöste Sprosse die steilsten Hänge trägt: mit der alten Stufe (volle Amplitude bei 4 px
Periode) lag das Pixel-RMS im Nahfeld bei **0,1893** gegen 0,0073 der Zwei-Oktaven-Fassung — 26×. Mit
der Rampe (nichts bei 4 px, alles bei 8) **0,0566**. Was die Rampe wegnimmt, landet in der
Toksvig-Varianz.

**Weltfest blieb**, nachgemessen: zwei Bilder 0,10 m seitlich, Bandpass 4–24 px, gemessene Verschiebung
4 / 9 / 16 / 20 px gegen vorhergesagte 5,0 / 9,4 / 15,9 / 20,0 px, Korrelation 0,63–0,72 am Maximum
gegen −0,29…0,10 bei null.

**Kosten:** 6,663 → **6,975 ms** (+4,7 %), Dreiecke 314 362 und `classVramMB` 32,5 unverändert, keine
Pass-, Draw- oder Bindungsänderung. Die fünf Anker hielten: Bodenton max Δ 0,09°, Sättigung max Δ 0,006,
grün-unter-beiden max Δ 0,012 pp, Tonwertspreizung max Δ 0,01 EV.

## 2026-08-06 — Schritt 2 abgenommen, und die Klassenstabilität wird eine Spec-Zeile

`sim-critic` nimmt den **Bodenshader ab**: `NO DEFECTS` über neun Azimute und sieben Pitchwinkel, alle
Bilder im selben Lauf gerendert. Die beiden Blocker auf seiner eigenen Metrik nachgeprüft — Wassermaske
gegen zwei erzwungene Klassen validiert (151 784 / 151 784 px, 0 Fehlklassen), Wasserton streut über
vier Azimute um **35,5°** gegen 1,3° des Landes, und die Helligkeit steigt **monoton**, je näher der
Blick an den Sonnenazimut 282,6° rückt. Das ist Spiegelverhalten und kein Kuppelmittel.

**Zwei meiner Zielmarken sind widerlegt, nicht getunt** — beide stehen in [`goal.md`](goal.md) §2, damit
sie niemand erneut erhebt: Fresnel 0,905 gilt nur für `roughness → 0` (Split-Sum für α = 0,05 ist
**0,534**, 200 000 Monte-Carlo-Samples, Multi-Scatter +0,004 bei A+B = 0,877), und Peak/Median < 5×
liegt unter dem arithmetischen Boden (ein perfektes `f^−1,6`-Gesetz erreicht **6,3** by construction).

**Der Standpunkt steht im Wald.** OSM gibt 78 % Wald über 76 m, die Klassenkarte `laubmischwald` auf
98 % der Bodenpixel innerhalb 50 m — und im Bild steht kein Baum. Daran hängt auch die Leere der Ferne
(RMS-Kontrast 0,023–0,032 bei 200–800 m gegen 0,63 bei 25–50 m): **ein Wald ohne Bäume, kein Boden ohne
Struktur.** Gehört Schritt 5, blockiert Schritt 2 nicht, macht aber Schritt 3 unbeurteilbar.

**Der Eigner meldet die Klassifizierung zum zweiten Mal springend.** Der Mip-Fix der Vorrunde war nur
eine der Achsen; die zweite ist die **Tile-Zoomstufe** — eine Array-Ebene je residentem Tile, jede in
ihrer eigenen Zoomstufe bei gleicher Texelzahl, also verdoppelt sich die Bodenauflösung des
Klassenrasters pro Stufe. [`render/classification.md`](render/classification.md) `## Spec` führt beide
Achsen jetzt als Tabelle und verbietet zeitliche Glättung ausdrücklich: die Abnahme ist ein **identischer
Klassenindex an einem festen Weltpunkt über eine ganze Laufstrecke**, auch innerhalb einer Übergangszone.

**Das Ausfransen der OSM-Kanten bekommt seine Spec, bevor es gebaut wird.** Eigner: *„strassen haben
harte kanten. waldränder und feldwege sind etwas difuser."* Die Übergangsbreite ist ein Datum am
**geordneten Klassenpaar**, nicht eine Shader-Konstante — eine gebaute Kante ist ein Bauteil, eine
gewachsene eine Zone. Die Breiten werden belegt statt gesetzt; bis dahin `[SET]`. Stabilität schlägt
den Übergang: kollidieren beide, fällt der Übergang.

**Die zweite Ursache des Klassen-Springens lag im Grasfeld, nicht im Bodenshader.** Die Klasse des
Bodens stand schon (Frontlage x = −37,244 m ± 0,03 über 4 m Gehweg); gesprungen ist die ART der Halme.
Das 49×49-Höhen-/Klassenfeld war AUF DAS AUGE zentriert, also tasteten seine Stützstellen das
Klassenraster an wandernden Punkten ab und die bilinearen Gewichte, aus denen ein weltfester Halm sein
Template zieht, wanderten mit — gemessen: Halmdeckung in einem festen Weltstreifen 0,026 → 0,151 →
0,026 mit einer Periode von exakt 2,0 m, der alten Feldweite. Das Feld liegt jetzt auf demselben
Gradnetz wie die Halmzelle (`render/CoverGrid.h`); über Gehwege von 0,37–4,0 m ändert **keine** von
3 525–3 822 gemeinsamen Gitterzellen ihren Bucket. Offen bleibt der Streaming-Fall: ein z13-Elter
antwortet für sein z14-Kind, bis das Kind da ist (49,97 % nach 8 Pässen, 0,00 % nach 20). Das
Ausfransen ist nicht gebaut — Stabilität ging vor.

## 2026-08-06 — Das springende Gras war nicht die Klasse, sondern ihr Leser

Meine Hypothese war die Tile-Zoomstufe. Sie ist **widerlegt und die Widerlegung ist die bessere Zahl**:
innerhalb einer Zoomstufe teilen sich alle Kacheln ein Texelgitter, also kann nur ein Split eine Grenze
verschieben, und der z13→z14-Split liegt bei `SpanM(z)·kSseK/kEdgeTau` = 3000,8 × 623,54/384 = **4873 m**
— dort überspannt ein z14-Texel (±2,93 m) 0,60 mrad = **0,41 px** bei 688 px/rad. Der Mechanismus ist
real und unterhalb eines Pixels.

**Der Fehler saß im Konsumenten.** Die Bodendeckung tastete die Klasse auf einem Gitter ab, das an der
Kamera hing; die Klassentextur war unschuldig. Trennscharf gemessen an einem festen WELTstreifen über
neun Läufe zu je 0,5 m: Halmdeckung schwankt **5,8×** mit Periode exakt **2,0 m** (dem Feldabstand),
während die Klassenkante darunter um 0,057 m steht. Zwei unabhängige Terrainproben bestätigen die
Unschuld der Klasse: Klassenflächen in einer festen Weltbox konstant auf ±4 m² von 5042 m², also
Grenzversatz 0,005 m.

**Die Behebung ist das Gradnetz, und die verworfene Alternative ist die lehrreiche.** Ein Metergitter
braucht einen Ursprung; jeder verfügbare ist entweder eine Sitzungseigenschaft — dann sind zwei Clients
über die Art eines Halms uneins — oder ein projiziertes Gitter, und das **driftet 0,26 m pro 2 m
Nordbewegung** bei lon 9,43° (`Δ = X·tan φ·Δφ`, Ostskala 643 900 m). Langsamer als der Fehler, nicht
anders. Preis des Gradnetzes ist die rechteckige Zelle (1,00 m Nord × 0,613 m Ost), also 78 × 49 statt
49² Feldstützstellen: Upload 38,4 → 141,9 KB, Frame-Zeit 8,32 → 8,16 ms, beides im Rauschen.

Nachweis in der vom Eigner verlangten Identitätsform (`FB_FIELD_DUMP=1`, Zellen nach absoluter
Gitterzelle geschlüsselt): **0 abweichende Zellen** über fünf Läufe bis 4,0 m, Übergangszone
eingeschlossen. Streifenschwankung 161 → 9 %, 56 → 15 %, 57 → 12 %; der Rest ist monoton mit dem Azimut
statt periodisch, also das Billboard, kein Klassenwechsel.

**Offen und dieselbe Meldung:** ein ankommendes Tile benennt den Boden unter sich um, weil das Feld
seine Klasse vom Patch des GEZEICHNETEN Blattes nimmt — ein z13-Elternteil antwortet für sein z14-Kind.
**49,97 %** von 3 822 Zellen weichen nach 8 Streaming-Durchläufen ab, 0,00 % nach 20. Im Stehen
konvergiert es, beim Laufen in frisches Gelände kreuzt man es dauernd. Eigene Runde, läuft. *(Nachtrag:
die 49,97 % waren ein Feld ohne Deckung, nicht der Fehler — siehe die Runde vom selben Tag.)*

## 2026-08-06 — Die Klasse wird auf EINER Zoomstufe gelesen, und "noch nicht bekannt" ist ein Zustand

Die Meldung des Eigners war zwei Runden alt und hing an einem Satz aus `classification.md`: *ein Filter
darf entscheiden, was GEZEICHNET wird, nie was DA IST*. Das Bodendeckungsfeld nahm seine Klasse vom
Raster des Blattes, das gerade gezeichnet wurde — also antwortete ein z13-Elternteil für sein z14-Kind,
solange das Kind unterwegs war. Jetzt rechnet die Gitterzelle ihre eigene Länge und Breite aus (eine
Zelle IST ein ganzes Vielfaches von `kCellM` Bogenmaß, also exakt) und liest `kMaxZ`; wessen Kachel
fehlt, erzeugt **nichts** statt eine Vermutung.

**Erst die Korrektur der eigenen Zahl.** Die 49,97 % nach 8 Durchläufen, die hier standen, waren kein
umbenannter Boden, sondern ein Feld ganz ohne Deckung, das als Klasse 0 gedruckt wurde. Im Stehen feuert
der Fehler gar nicht: der Streamer steigt tiefenzuerst ab, unter der Kamera liegt z14 ab dem ersten
gezeichneten Durchlauf, und die alte Fassung ändert zwischen Durchlauf 11 und 60 **0 von 3 822** Zellen.

**Der Fehler feuert beim LAUFEN, und dafür gibt es jetzt `--walkE/--walkN`** — Meter pro
Streaming-Durchlauf, die Kamera bewegt sich also, während die Kacheln ankommen. Vorher wechselten
**237 / 256 / 482** Zellen auf drei Strecken ihre Klasse, nachher **0 von 97 410 / 188 435 / 704 150**.
Und alle Wechsel lagen in einer Übergangszone (5,91 % / 4,73 % / 1,98 % der Zonenzellen gegen 0,000 %
im Inneren) — genau die Form, die ein gröberes Raster vorhersagt: es verschiebt eine GRENZE, es
übermalt keine Fläche. Am Standpunkt: 0 abweichende Zellen bei 1, 2, 4, 8, 20 und 60 Durchläufen.

**Die naheliegende Form ist für den Terrain-Shader tot, gemessen statt vermutet.** Alle 130 gezeichneten
Blätter der Referenzszene auf `kMaxZ` auszudrücken sind `Σ 4^(14−z)` = **11 776** Klassenkacheln =
**2 944 MiB** und **11 776 Array-Ebenen** gegen eine Gerätegrenze von 2 048 — Faktor 5,75. Für die
Bodendeckung dagegen ist dieselbe Regel gratis: die Scheibe hat 44,16 m Radius, eine z14-Kachel 1 502 m,
also höchstens vier Seiten. Der Preis ist ein Nullsummenspiel, weil `EnsureNear` seinerseits kein Raster
mehr holt: `classPulls : nearFills` = **1:2 / 3:3 / 15:15**. Frame-Zeit nicht auflösbar (6,995 gegen
7,084 ms bei 0,69 ms Streuung), Pässe, Draws und `classVramMB` unverändert.

**Was offen bleibt und jetzt eine Zahl hat:** der Terrain-Shader liest weiter das gezeichnete Blatt.
Bei 1,0 m/Durchlauf weichen **0 von 921 600** Pixeln vom konvergierten Bild ab, bei 16 m/Durchlauf
**2,23 %** — er hinkt nur, wenn das Auge den Streamer überholt. Eine Klassenauflage nur für die Nähe
wäre die Reparatur und ist abgelehnt: nah exakt, fern gefiltert, ist wieder eine Klasse, die vom
Betrachter abhängt.

## 2026-08-06 — Der ideale Waldrand ist ein Trugbild

Recherche zu den Übergangsbreiten, weil ich sie sonst erfunden hätte. Befund: die 20–30-m-Zone aus den
Merkblatt-Grafiken ist ein **Leitbild aus der romantischen Landschaftsmalerei** (Gehlken 2014) und real
fast nie vorhanden; wo sie vorkommt, ist sie Brache. Die geografisch nächste Messung — Lewark 1971,
Hann. Münden, ~300 km Randlänge, gleiche Buche-auf-Kalk-Landschaft — findet **zwei Drittel aller Ränder
unter 5 m**. Buche bildet einen bodentiefen Steilrand mit ~1 m, Fichte 0,5 m, nur Eiche/Edellaub in
Süd-/Westlage erreicht 5 m. Abschlag obendrauf: Krüsi und Lauterbach rechneten den gemähten Ackerrain
dem Krautsaum zu, obwohl der Rain unabhängig vom Gehölz existiert.

**Ein Paar trägt zwei Zahlen:** Gesamtbreite und Anteil auf Seite A. Am Waldrand liegt die Zone zu ~80 %
INNERHALB des Waldpolygons (Mantel im Kronentrauf, Saum unter dem Mantel), bei Straße und Wasser zu
100 % auf der Landseite. Eine richtungsabhängige BREITE existiert dagegen nicht — dafür fand sich kein
Beleg. **Drei Übergänge sind keine Gradienten:** Waldrand (vertikaler Stapel unter 5 m, in der Fläche
verflochtenes Mosaik mit gebuchteter Grenzlinie), unbefestigter Feldweg (fünf Bänder, vier harte
Kanten), verbautes Ufer (Materialwechsel an scharfer Deckwerkskante). Tabelle mit Quellen in
[`render/classification.md`](render/classification.md) `## Spec`.

Zwei eigene Korrekturen: der Ort ist **Hastenbeck/Halvestorf bei Hameln, Niedersachsen** (NWG, nicht
LWG NRW), nicht Fürstenberg/Höxter; und die Klassen stehen in `vegetation.json` (9 Templates), nicht in
`ground-materials.json` (16 Materialien).

## 2026-08-06 — Der Standpunkt stand im Wald, und das war die halbe Wüste

Die Klasse springt beim Laufen nicht mehr. Der Reststand aus der Vorrunde — ein ankommendes Tile
benennt den Boden um — ist behoben, indem die Bodendeckung ihre Klasse nicht mehr vom gezeichneten
Blatt nimmt, sondern jede Gitterzelle über ihre eigene Lat/Lon auf einer `kMaxZ`-Seite auflöst; eine
Zelle ohne Daten erzeugt **kein Gras** statt falsches Gras.

**Die geerbten 49,97 % haben nichts gemessen** und das gehört zum Ergebnis: Der alte Dump gab bei
Durchlauf 8 `covered = 0` aus, den Initialwert, den der Vergleich als Klasse 0 las. Im kalten Stand
ändert schon der alte Code 0 von 3 822 Zellen. Der Fehler feuert beim **Laufen**, und dort ist er jetzt
beziffert: 237 / 256 / 482 Zellen mit zwei Klassen auf drei Strecken (1,0 / 4,0 / 16 m je Durchlauf)
gegen **0** danach. **100 % der Umbenennungen lagen auf einer Klassengrenze**, keine im Inneren — genau
das, was ein gröberes Raster vorhersagt.

**Die Ausweitung auf den Terrain-Shader ist an einer Zahl gestorben**, nicht an einer Meinung:
130 gezeichnete Blätter über z9…z14 brauchen `Σ 4^(14−z)` = **11 776** Klassenkacheln = 2 944 MiB und
11 776 Array-Ebenen gegen `maxTextureArrayLayers` 2048. Faktor 46. Der Terrain-Shader liest weiter das
gezeichnete Blatt, beziffert: **0 px von 921 600** bei 1,0 m je Durchlauf, 2,23 % erst bei 16 m — er
hinkt nur, wenn das Auge den Streamer überholt.

**Standpunkt verschoben: 52,10499/9,43424 → 52,10602/9,43453, yaw 270 → 280.** Der alte Punkt lag
per Punkt-in-Polygon INNERHALB eines Waldpolygons, 31,4 m von dessen Kante. Der Boden unterm Auge war
`waldboden`, und das Bild las sich als Wüste — nackter Waldboden mit Grashalmen darauf, weil die Bäume
darüber Schritt 5 sind. Die Wüstenanmutung war nie ein Shader- und nie ein Grasfehler. Der neue Punkt
steht auf `wiese` (modale Vorlage 1702 von 3822 Zellen gegen vorher `laubmischwald`), Waldkante 25 m
auf 117°, nächstes Windrad 340 m auf 250° mit OSM-`height` 134 m — das einzige im Nahfeld mit
Höhenangabe. Geländeprofil geprüft: der Windradfuß liegt 15 m UNTER dem Auge, die Spitze steht bei
+19,0° gegen eine Bildoberkante von 30°. `fovDeg` ist die **vertikale** Öffnung, das horizontale Feld
also 91,5° bei 16:9, und 250° liegt im Sektor 234…326° der deklarierten Blickrichtung.

Boden 100,596 m (`/elev` antwortet 100,60), Sonne el 11,202 / az 282,601, 7 Pässe, 312 442 Dreiecke,
`classVramMB` 32,5. Der wasm-Client baut mit dem neuen Szenenfile, ist im Browser aber **nicht
nachgemessen**; die alten Browserzahlen sind gestrichen statt übernommen.

## 2026-08-07 — Der Prüfstand ist ein Schalter am Frame-Orakel, kein dritter Client

`doc/goal.md` §3 verlangt seit je, dass eine Pflanze ZUERST allein gerendert wird; es gab ihn nicht.
Gebaut als `gpu_walk --rig <template>` — **kein dritter Client**, weil ein Client genau Einstiegspunkt
plus Ausgabemedium ist und der Prüfstand beides unverändert lässt. Ersetzt werden nur die EINGABEN:
die Welt (kein `World::Open`, keine Kachel, kein OSM, kein DEM, kein Netz) und das Licht (deklariert,
nie geerbt). Was das Subjekt zeichnet, bleibt `GroundCoverStage` über `Renderer::SetGroundField` —
derselbe Aufruf, den `World` macht.

Der Boden ist Studiomobiliar und heißt so: `render/stages/BenchGroundStage`, 18 % neutral, mit
kalibriertem Raster, gespleißt aus denselben `SurfaceLight.h`/`ShadowSample.h`/`CloudShadow.h` wie
jede beleuchtete Fläche. Selbst-abgeschaltet, kein zusätzlicher Pass.

39 Bilder aus 7 Ansichten × 4 Lichtern (`art-director`: Auflicht/Gegenlicht el 11°, reines Himmelslicht
als geschlossene Decke über `cloudSunThru`, Turntable 360°; `botanist`: `portrait a b closeup_hd tuft
sward eye`). EINE Belichtung für den ganzen Lauf, `KeyEv` −3,887, am Auflicht gemessen — sonst sind
vier Lichter nicht vergleichbar. Objektiv 30° = 44,78 mm; `closeup_hd` schaltet auf 355,6 mm bei
0,500 m Arbeitsabstand, weil ein Makro ein längeres Objektiv ist und keine nähere Kamera.

**Zwei Messungen statt Schätzungen.** `sward` ist 1,000 m² als 720×720-Ausschnitt und misst
**13,02 %** Deckung — über die TIEFE, nicht über die Farbe: die Farbdifferenz gegen dieselbe Fläche
ohne Pflanze zählt die Schirmverschattung mit und ergab 64,7 %. Der isolierbare Kleinstbereich ist ein
Deckungsfeld-Quad, gemessen **1,228 m × 2,000 m** — feiner kann Deckung nicht deklariert werden, also
gibt es keine „Einzelpflanze", und das steht als Lücke 13 statt als Bild.

`_wind` und `_season` werden **verweigert**: der Wind erreicht den Shader nicht, eine Jahreszeit ist
nicht modelliert, drei bzw. vier identische Dateien wären eine Lüge. `--rig-height 25` rahmt jede
Ansicht ohne Codeänderung neu (`eye` schwenkt von −3° auf +11,63°, `sward` bleibt 1 m²).

Nebenbefund und echter Defekt: `CameraBasisEcef` lieferte bei Pitch ±90° eine **Nullbasis** — das
Kreuzprodukt verschwindet und die substituierte Länge 1.0 ließ den Seitenvektor bei null. Der Grenzwert
ist die Horizontale des Yaw; ohne das gab es kein Nadir-Bild. Die Szene bleibt bitgleich:
`sim/walk-demo.png` vorher/nachher **0 abweichende Pixel**, 312 442 Dreiecke unverändert.

## 2026-08-07 — Das weiße Band war ein Loch, und darunter lag eine Registrierung

Ich hatte den weißen Streifen über der Halmbasis als nicht bandbegrenztes Detailrauschen des
Bodenshaders vermutet und aus der Entsättigung geschlossen, dass etwas Achromatisches mit voller
Amplitude eingemischt wird. **Beides falsch, dreifach widerlegt:** der Klassen-Visualisierungspfad kehrt
vor Relief, Licht und AO zurück und zeichnet flaches Rot — das Band bleibt; Specular, AO und Schattenpass
einzeln aus ändern es auf drei Stellen nicht; und der Tiefenpuffer im Band ist **exakt 0,0**, dort wurde
nie Geometrie rasterisiert. Die Entsättigung ist keine Wäsche, sondern eine binäre Mischung: 48,3 % der
Pixel exakt (195,192,176), 9,9 % exakt (82,76,69) — Wolkenschicht gegen Himmels-LUT unter dem Horizont,
durch das Loch gesehen. Zwei-Punkt-Modell mit p = 0,830 sagt σ = 42/44/40 voraus, gemessen 45/50/50.

**Die Ursache: Höhenorakel und Netz tasten das DEM nicht an derselben Stelle ab.** Das gezeichnete
Gelände liegt **0,266 m über** der Höhe, auf die die Kamera gesetzt wird, unabhängig von der Augenhöhe.
`TerrainLoader.cpp` nimmt Index `frac·cols`, `terrain.cpp` legt Stützstelle *i* auf `i/(cols−1)`. Die
Differenz ist `frac` Texel — null an der West-/Nordkante, ein volles Texel an der Ost-/Südkante, und ein
z13-Texel misst hier 11,74 m. **`tiles/src/elev.c` trägt dieselbe Rechnung**, also stimmen Client und
Server miteinander überein und beide widersprechen dem Netz; deshalb stand es.

**Entschieden wird das nicht durch Vorliebe, sondern durch Zoomkonsistenz:** Texelmitte
(`frac·cols − 0,5`) liefert 100,909 / 100,882 / 100,907 bei z13 / z14 / z15 — derselbe Punkt durch drei
Raster. `frac·cols` liefert 100,596 und ist der Ausreißer.

**Preis: Schritt 2 wird neu abgenommen.** Zwei von fünf Ankern brechen — Bodenton 0,214° gegen Toleranz
0,09°, Grünanteil 0,94 pp gegen 0,012 pp. Das ist es wert; ein grünes Häkchen ist kein Grund, jeden
künftigen Körper einen Vierteilmeter unter die Erde zu setzen. Der Agent hat den Fix ausdrücklich NICHT
hinter der Abnahme angewendet, sondern seinen Preis gemessen und die Entscheidung vorgelegt — richtig.

**Als Betrug abgelehnt, mit Messung:** `kNearM` 0,05 → 0,01 entfernt das Band und ist beweisbar
bildneutral (`zn` steht nur in der z-Zeile der Projektion), würde aber eine deklarierte 0,30-m-Nahsicht
still aus 0,034 m rendern. Einzelheiten in [`world/terrain.md`](world/terrain.md) `## Gaps`.

## 2026-08-07 — Der Streufilz ist so rotbraun wie der Löss, und damit ist die Kante keine Materialfrage

`grasfilz` ist die 17. Klasse von `ground-materials.json` und schließt die vom Botaniker benannte
Lücke: unter einer Wirtschaftswiese liegt kein Boden. Chromatizität **1,000 : 0,674 : 0,280** und
`visibleBroadbandRatio` **0,579**, beides Pfad B über die zwei ECOSTRESS-Spektren *Avena fatua* litter
(vh354/vh355, UCSB ASD) — Grasstreu, als Streu gemessen. Die Pipeline ist an den eigenen Zahlen der
Datei verifiziert: sie reproduziert Quercus-Streu 1,000:0,720:0,429 / 0,654 und die Nadelmischung
1,000:0,606:0,348 / 0,514 auf jede gedruckte Stelle. `albedoBroadband` bleibt **[SET] 0,20**, geklammert
von zwei gemessenen Kurzwellen-Verhältnissen, die sich um 17 % widersprechen (0,199 über laubstreu,
0,234 über erde_trocken) — und die Gegenprobe der Methode am eigenen Streu-Paar der Tabelle
**scheitert** (0,836 statt 0,700), was Material und Packung sauber trennt.

**Die Vermutung beider Kritiker ist widerlegt, nicht bestätigt.** Grasstreu hat G/R 0,674 gegen 0,675
des Lösses — dasselbe Rotbraun auf 0,001, nur das Blau trennt sie (0,280 gegen 0,382). Der Materialtausch
bewegt R−G an der Deckungskante um 4 Codes (+30,2 → +26,1) und die Helligkeit um 26 (110,9 → 137,3).
**Ein Filz repariert die Substanz und nicht die Kante.**

Die Kante bewegt `swardClosure`, ein Templatefeld, das die Bodenreflexion der Zeile gegen
`mix(colorSrgb, drySrgb, dryFraction)` zieht — die drei eigenen Deklarationen des Templates, keine
Shaderkonstante. `wiese` deklariert **1,0**, hergeleitet: bei 44,16 m und 1,70 m Auge steht der Blick
2,205° über der Ebene, Beer-Lambert mit LAI 3 und G = 0,5 gibt eine Lückenrate von 1,2e−17. Gemessen bei
Auge 6,0 m, Pitch −12°, nach Distanz gebinnt: R−G außen **+30,2 → +7,0**, Δ(R−G) gegen die Narbe
**44,2 → 21,0**, ΔL **−13,0 → +17,0**.

**Beide Latten reißen** (|Δ(R−G)| < 6, |ΔL| < 5), und der Rest ist gemessen statt geraten: bei Closure
1,0 tragen Boden und Halme **dieselbe Albedo** und rendern trotzdem 21 Codes in R−G und 17 in L
auseinander. Das ist der Lichtpfad — die Halme tragen `occ` (⟨occ⟩ = 0,389) und eine
Vorwärts-Transmission bis 2,5×, der Boden keines von beidem. **Keine Albedo kann das schließen**; das
Aggregat gehört dorthin, wo `occ` wohnt.

Nebenbefund derselben Messung: der Deckungsshader liefert die deklarierte `dryFraction` 0,30 als
flächengewichtete **0,129** aus (`kWholeDry` 0,35, `kTipRun` 0,25) — dieselbe Regel wie bei der Dichte,
ein Feld weiter. Und das Fade-Band ist ein dunkler Ring: L **123,9 → 56,7 → 86,4 → 140,8** über
32–36 / 36–40 / 40–44,2 / 44,2–48 m, vor und nach dieser Runde identisch.

Zwei phänologische Fehler korrigiert, beide an das **Datum** der Szene gehängt und nicht an einen
Parameter: `wiese.forbs` tauscht Wiesenkerbel (Blütezeit IV–VII) gegen Wilde Möhre (V–IX) und
Wiesen-Flockenblume (VI–X/XI); `acker` steht auf 0,12 m Stoppel mit `dryFraction` 0,92 statt 0,55 m
halbreifem Weizen, und seine `drySrgb` behält ihre Leuchtdichte und bekommt die gemessene
Stroh-Chromatizität. Die Acker-Änderung allein bewegt 5 679 px der Referenzansicht.

Die fünf Anker von Schritt 2 halten, fünf Yaws, gepinnte Binärdatei: Bodenton max Δ **0,401°**,
Sättigung **0,001**, grün-unter-beiden **0,000 pp**, Tonwertspreizung **0,198 EV**, Weltfestigkeit
identisch (Spitzenverschiebung +18 / +16 px vorher wie nachher). Grün-dominante Bodenpixel steigen um
bis zu **2,02 pp** — die beabsichtigte Richtung.

## 2026-08-07 — Die Wiese ist eine Wiese, und sie kostet 56 ms

Deckung **13,02 % → 93,51 %** auf dem neuen Prüfstand (1 m² senkrecht, kalibriert). Die Arbeit steckt
in EINER Ableitung, die vier Defekte gleichzeitig erledigt: die Vorlage deklariert Halme/m², Breite und
Bestandshöhe, aber nicht die **Länge** eines Blattes — ein Blatt, das mit 65° austritt und sich
überbiegt, erreicht weit weniger Höhe als es lang ist. `L = Höhe / ⟨sin φ⟩`, `⟨sin φ⟩ = 0,5696`
(Monte-Carlo über die deklarierte Neigungs- und Bogenverteilung, 4·10⁵ Halme × 128 Stationen). Daraus
fallen L:B **47,9:1** (Band 40–55), LAI **4,63** (1–5), Deckung **95,2 %** (95–100), mittlere Neigung
**65,0°** (55–75), Spitzenauslenkung **0,413** (0,30–0,50), und Spitzenhöhen mean 0,30 / p90 0,51 /
p99 0,64 m — **geschlossene Blattmasse und aufgeschossene Halme sind dieselbe Population.**
`kBladesPerM2 = 150` und der `widen`-Mechanismus sind ersatzlos weg.

**Der fehlende Gegenlichteffekt hatte eine andere Ursache als den fehlenden Term:** das kamerazugewandte
Billboard. Eine kamerafeste Breite macht jede Normale zum Blickvektor, dann hat `N·L` denselben Betrag,
egal auf welcher Seite die Sonne steht. Gegenlicht ÷ Auflicht im p95 (szenenlinear, weil die Tonkurve im
Handbetrieb geschlossen und invertierbar ist): **0,93 → 2,16**.

**Bestandslicht als Physik statt Kurve:** der Strahl wird über seinen Schrägweg abgebaut
(`G(el)/sin el` = 2,220 bei 11° gegen 0,655 im Nadir), der Abwärtsfluss mit `sqrt(1−ω)` mal dem
hemisphärisch integrierten Koeffizienten (0,795 bei LAI 4,6). Beide mit dem Strahlgesetz zu dämpfen legte
55,4 % des Bildes auf Code 0; die **Differenz** ist das diffuse Feld im Bestand. `G(el)` ist dieselbe
Blattwinkelpopulation, aus der die Geometrie gebaut ist. Boden gegen Halm im Nahfeld 103,5/104,2 →
**113,9/121,3**, Bodenanteil 14,48 % → **0,79 %**.

**Das Budget ist gemessen und gerissen: 56,26 ms gegen 33,3 ms, Faktor 1,69.** 1 393 949 Halme,
12 545 541 Dreiecke. **50,9× die Dreiecke kosten 11,4× die Zeit — füllbegrenzt, nicht vertexbegrenzt.**
Ursache benannt und offen: es gibt kein LOD, ein 11-mm-Blatt in 8 m ist 0,9 px breit und kostet trotzdem
einen Fünf-Segment-Streifen. Einzelheiten in [`goal.md`](goal.md) §5.

## 2026-08-07 — Der Streufilz behebt die Substanz und NICHT die Kante

Neues Material `grasfilz` (17. Klasse), Chromatizität aus zwei ECOSTRESS-Spektren von *Avena
fatua*-Streu, Ableitungsweg **zuerst gegen die publizierten Werte der Datei verifiziert** (Eichenstreu,
Nadelstreu, Grüngras, Flachreflektor — alle auf die gedruckte Stelle reproduziert).

**Meine Synthese „zwei Kritiker, ein Fehler" ist widerlegt.** Grasstreu hat G/R = **0,674**, Löss
**0,675** — identisches Rotbraun bis auf die dritte Stelle, nur Blau unterscheidet sich (0,280 gegen
0,382). Und der Grund liegt tiefer: bei voller Bestandsschließung tragen Terrain und Halme **dieselbe
Albedo** und stehen immer noch 21 Codes in R−G und 17 in L auseinander, weil die Halme `occ`
(⟨occ⟩ = 0,389) und eine Transmissionskeule bis 2,5× tragen und das Terrain keins von beidem.
**Die Fade-Kante ist ein Beleuchtungsbruch, kein Farbbruch** — keine Albedo kann sie schließen. Latte
|Δ(R−G)| < 6 und |ΔL| < 5 nicht erreicht (40,1 → 21,0 über fünf Schließungsstufen, monoton, mehr ist
nicht zu holen).

**Die Methode der Materialdatei prüft sich selbst und fällt durch:** `SW(Nadeln)/SW(Eiche)` = 0,836, wo
die Tabelle 0,700 impliziert — 17 % Widerspruch in einer abgenommenen Datei. Gemeldet, nicht kaschiert.

**Neue Instanz derselben Fehlerklasse wie die Dichte:** `vegetation.json` deklariert `dryFraction: 0.30`,
der Shader realisiert flächengewichtet **0,129**. Code überstimmt eine Deklaration, ein Feld weiter.

Phänologie korrigiert: `acker` 0,55 m stehender Halbreifweizen → **0,12 m** Stoppel bei `dryFraction`
0,92 (Mähdrescher-Schnitthöhe 0,10–0,15 m, `[SET]` auf die Mitte); `forbs` Wiesenkerbel (blüht IV–VII,
Saumart) → Wilde Möhre + Wiesen-Flockenblume. Die Jahreszeit ist **nicht** modelliert und wurde nicht
eingeführt — die Werte hängen an einem Datum statt an einem Parameter, eingetragen in `## Gaps`.

## 2026-08-07 — Die DEM-Registrierung, entschieden von der Quelle statt von meiner Probe

Behoben in einer Runde über Client, Netz und Server: **ein DEM-Texel ist eine FLÄCHE**, die Abtastung
liegt auf `frac·n − 0,5`, und alle drei gehen durch **einen** Ausdruck (`fb_texel_index` in
`tiles/src/tilemath.h`, vom `sim/Makefile` auf die vier Terrain-Übersetzungseinheiten gelegt).

**Die Quelle entscheidet, nicht die Probe.** `tilezen/joerd`, `mercator.py`: `_merc_bbox` ist die ÄUSSERE
Bounding-Box der Kachel, `dst_x_res = (bbox[2]−bbox[0]) / size` — durch 256, nicht durch 255 — und die
GDAL-Geotransformation verankert die Außenkante des linken oberen Pixels. Mitte von Texel *i* auf
`(i+0,5)/N`, vom Erzeuger der Daten.

Zwei unabhängige Bestätigungen. Über eine z13-Naht: `|A[:,255] − B[:,0]|` im Mittel **0,955 m** gegen
0,982 m für zwei benachbarte Spalten INNERHALB einer Kachel — ein Texel Abstand, keine Überlappung. Und
200 Zufallspunkte durch drei Zoomstufen:

| Abtastung | RMS(z13−z15) | RMS(z14−z15) | Verhältnis |
|---|---|---|---|
| `frac·n` (Orakel, alt) | 0,538 | 0,193 | 2,79 |
| `frac·(n−1)` (Netz, alt) | 0,316 | 0,201 | 1,57 |
| **`frac·n − 0,5`** | **0,056** | **0,058** | **0,96** |

Das VERHÄLTNIS ist der Beweis: ein Registrierungsversatz ist ein fester Texelanteil, halbiert sich je
Zoomstufe und muss ≈3 ergeben. 0,96 heißt, es ist keine zoomabhängige Komponente mehr da.

Abnahme: Netz − Orakel **0,2670 → 0,0382 m**, bei Auge 1,70 und 0,40 auf fünf Stellen gleich (also reiner
Höhenversatz, kein Skalenfehler). Client gegen Server ≤ **0,004 m**, gegen unabhängigen Sampler
≤ 0,0003 m. **Hangfall** am steilsten Punkt in 700 m (Gefälle 0,330 m/m): der Boden, auf den ein Körper
gesetzt wird, wandert um **−0,849 m**, und das Vorzeichen kippt gegenüber dem Referenzpunkt — der Fehler
war eine horizontale Verschiebung, keine Vorspannung. Lochschwelle der nahen Ebene 0,3015 → **0,088 m**.

**Zwei meiner Angaben korrigiert:** das gezeichnete Blatt ist z14, nicht z15 (`kMaxZ` = 14), und meine
„0,73 m bei +100/+100" mischten zwei Konventionen — es sind 0,023 m.

**Meine Annahme zum z13-gegen-z14-Rest ist gekippt:** das Orakel auf die gezeichnete Zoomstufe zu ziehen
hilft NICHT (RMS 0,404 gegen 0,383). Der Rest ist nicht das Raster, sondern dass `ChunkBuildEcef` 256²
auf 33² Stützstellen dezimiert = **46,9 m** Abstand bei z14. Der Hebel wäre, dass das Orakel die
gezeichnete FLÄCHE auswertet; das ist eine Architekturentscheidung (`fb_stream_ground` ist eine freie
C-Funktion ohne Zugriff auf `kMaxZ` und `Grid`) und steht beziffert in `world/terrain.md` `## Gaps`.

**Preis, gemessen:** Schritt-2-Anker bewegen sich stärker als von mir vorhergesagt — Bodenton 0,873°
statt 0,214°, Grünanteil 1,511 pp statt 0,94 —, weil das Gelände ein halbes Texel horizontal wanderte
UND die Kamera 0,313 m stieg. Die Weltfestigkeit ist mit der bisherigen Methode bei 1,39 Mio. Halmen
**nicht mehr auflösbar** (bandgefilterte Korrelation 0,19–0,41); beide Builds zeigen dieselbe monotone
Parallaxe, aber die Aussage trägt so nicht mehr und ist als offene Frage an den Kritiker gegangen.

## 2026-08-07 — Zwei Kritiker, ein Clamp, und meine Physik war keine

Beide Nachprüfungen NACHBESSERN, und beide zeigen unabhängig auf dieselbe Codestelle.

**Der Ambient im Bestand wird auf null GEKLEMMT, nicht gedämpft.** Belichtungsunabhängig dreifach
bewiesen: (1) Histogramm unter reiner Halbkugel — `portrait-skylight` hat **75,92 %** auf exakt Code 0
und die Codes 1…16 zusammen 1,15 %, Verhältnis **66:1**; eine gedämpfte Innenraumradianz erzeugt einen
langen BESETZTEN Schwanz, ein Dorn auf null mit leerem Schwanz ist ein Schnitt. (2) `closeup_hd-frontlit`
mit linearem Gain 6 nachgezogen — das Schwarz bleibt Schwarz, also liegt dort keine beschnittene
Struktur, sondern Leere. (3) **Die radiometrische Schranke:** die 18-%-Karte steht HINTER dem Bestand bei
Code 145, die Halme DAVOR bei Code 0; beide sehen dieselbe Halbkugel, Blattalbedo 0,10–0,14 gegen 0,18
der Karte. 145:0 verlangt Einstrahlung null.

**Die Ursache ist mein eigener Satz aus der Vorrunde.** „Die Differenz der beiden Transmissionen ist das
diffuse Feld im Bestand" ersetzt einen Clamp durch einen zweiten: eine Differenz zweier Exponentialterme
wird negativ und wird dann geklemmt — genau der Dorn. Ich habe das als Physik weitergegeben.

Der Sollwert, unabhängig hergeleitet: Botaniker `exp(−0,795·4,63) = 0,0246` = **5,3 Blenden** (nicht die
gemeldeten 6,9 — es fehlen 1,6, Verdacht: der Bestandsabbau wirkt zweimal); Art Director 2–5 % des
offenen Himmels = −4,3…−5,6 Blenden = **Code 3, nicht Code 0**. Beide nennen denselben Grund, warum es
nie null werden kann: **Mehrfachstreuung** — ein Blatt leitet bei 550 nm ~0,25 weiter oder wirft zurück.
Ein Bestand ist ein streuendes Medium; der heutige Term kennt nur Absorption.

**Ich hatte beide gefragt, ob der Hebel in der Belichtung liegt. Beide: nein.** *„Aus einem schwarzen Loch
würde ein graues Loch."* Der Zeh der Tonkurve fehlt wirklich, aber nachgeordnet — sonst kalibriert man
gegen eine Leere.

**Ein Ein-Ursachen-Fehler mit drei Symptomen:** die Rückseite des Streifens wird ungeleuchtet gezeichnet.
Halme kippen an einer harten Segmentgrenze in reines Schwarz und laufen dahinter grün weiter; dasselbe in
Subpixelbreite ergibt schwarze Haarlinien quer über den Himmel.

**Der Botaniker korrigiert seine eigene 8,7-mm-Regel gegen mich:** sie ist eine Regel über GRÖSSEN, nicht
über WINKEL. Der Kiel hat keine Länge, er ist die Normale — er wirkt auf jeder Entfernung. `o.nrm` hängt
heute nur von der Bogenstation ab und hat keine Komponente entlang `side`; Zeilenscan quer über die Halme
findet Läufe von **65, 75 und 192 px in exakt einer Farbe**. Derselbe Mangel lässt jeden Halm, dessen
`side` parallel zur Blickrichtung steht, auf null Breite kollabieren — das war der unbeglichene Preis der
Billboard-Entfernung. Eine laterale Normalenkippung behebt beides für null Dreiecke.

**Die Deckungszahl der Vorrunde ist nicht zertifiziert:** 19,1 % von `sward-skylight` sind exakt (0,0,0),
also unentscheidbar Halm oder Boden. Die Deckung liegt zwischen **73,1 % und 93,1 %**; die gemeldeten
93,51 % setzen voraus, dass alles Schwarze Vegetation ist. Unter einer Halbkugel kann im Nadir kein Pixel
exakt null werden.

**Reihenfolge korrigiert — ich hatte AA als eigene Runde nach der Halmform geplant, beide Kritiker
widersprechen:** der Zerfall dünner Halme in gestrichelte Linien ist **verlorene Deckung, kein
Kantenproblem**, und kein Post-Filter holt eine Kante zurück, die nie gerastert wurde
(`visual-target.md` §2 zitiert Karis dazu selbst). Neue Ordnung: **Shading (diese Runde) → LOD mit
deckungstragender Fernstufe → Wind → TAA**, mit einem Wegwerf-FXAA davor. Der Wind gehört dazwischen,
weil TAA auf bewegtem Laub an den Motion Vectors hängt. Die Fernstufe darf **nicht durch Ausdünnen**
entstehen — das ist genau das Popping, das TAA danach nicht auflösen kann. Und die Fade-Kante gehört in
dieselbe Runde wie das LOD, weil das Terrain eine Gras-Aggregatschicht mit Verdeckung und Transmission
braucht: **ein Bau, zwei Defekte.**

**Der Prüfstand hat selbst vier Mängel** und ist an genau den Stellen blind, für die er gebaut wurde:
`closeup_hd` ist leer (10 verschiedene Farben in 921 600 px, `frontlit` und `backlit` unterscheiden sich
um 1 Code auf 412 Pixeln), `sward` hat nur EIN Licht, die 18-%-Karte dient als Substrat unter der Pflanze
statt als Randreferenz, und `turn180` ist nicht `backlit`. Eigene Runde, läuft.

**Vierte Instanz von „Deklaration wird nicht realisiert":** `grasfilz` steht korrekt in
`ground-materials.json` mit 1 : 0,674 : 0,280, der gerenderte Boden misst linear 1 : 0,830 : 0,726 — und
zwar identisch auf 0,5 m, 3 m und 20 m, was Dunst ausschließt.

## 2026-08-07 — Schritt 2 abgenommen, Schritt 3 nicht, und das Werkzeug misst sich selbst

**Schritt 2 ist wieder abgenommen** — und zwar nicht über meine Ankertabelle, sondern über eine
unabhängige Prüfung gegen den Kachelserver: pro Bildspalte die oberste Tiefentrefferzeile in einen
Elevationswinkel umgerechnet (mit Off-Axis-`f` und Erdkrümmung 1/1,13) und gegen das DEM-Maximum
entlang derselben Peilung bis 16 km gehalten. **13 von 14 Spalten unter 0,09°, also unter einem Pixel**
(10,88 px/Grad); der einzige Ausreißer bei 14 km ist die Abtastschrittweite der Prüfung selbst. Dazu
0 Löcher, 0 Nähte (13 Ein-Pixel-Spitzen in 84 358 Fernterrain-Pixeln = 0,015 %), Streaming schon bei
`--warm 60` bitgleich mit `--warm 900`.

**Der Renderer ist nicht deterministisch, und das entwertet die Messungen der ganzen Runde.**
Unveränderte Kommandozeile, drei stabile Ausgaben: `grasL` 0,1491 / 0,1711 / 0,4748, **1,66 EV
auseinander**. A gegen B 39,02 % der Pixel > 2 Codes, 5,33 % > 32, max 151. Der Diff ist
**ausschließlich die Narbe**, pro Halm strukturiert; Himmel und Terrain bitgleich, `blades`,
`terrainTiles` und `blackLog2` in allen Fällen identisch. Trigger nicht isoliert (`--warm 30/240/1200`,
`FB_NODEMCACHE=1`, `FB_AO=0`, Vorlauf bei anderem yaw).

Preis: drei von vier Ankern schwanken zwischen zwei **identischen** Läufen stärker als zwischen den
beiden Builds des DEM-Fix — Bodenton 2,291° gegen gemeldete 0,873°, Grünanteil 3,688 pp gegen 1,511,
Tonwertspreizung 0,528 EV gegen 0,266. Nur die Sättigung liegt außerhalb des Rauschens. `CLAUDE.md`
Prinzip 6 gilt hier wörtlich.

**Die Weltfestigkeit ist wieder messbar — im Tiefenpuffer statt im Bild**, und die neue Methode ist der
alten überlegen: Restfehler **3,8 mm** gegen eine Nullhypothese bei 60,5 mm, Signal/Boden 8,8:1, und sie
wird mit steigender Dichte **besser** statt schlechter, weil sie eine starre Transformation fittet statt
Bildtextur zu korrelieren. Rezept und Zahlen in [`render/renderer.md`](render/renderer.md) §1.9.

**Der fehlende Eigenschatten der Narbe, beziffert:** Bestandsoberkante sd **0,162 m** (aus derselben
Tiefenrekonstruktion, n=253), bei 11,2° Sonne also Schatten von **0,82 m** Länge auf einem 0,25-m-Raster.
Erwartet 1,74 EV zwischen Sonnen- und Schattenfleck (aus `skyDiffuseHorizY` 0,0491 gegen `totalHorizY`
0,1642), gemessen **0,42 EV, richtungslos**. `FB_CSM=0` ändert `grasL` um 0,0001, der Schattenpass führt
7014 von 1 456 000 Dreiecken — kein Halm. Eine `occ`-Keule ist eine 1-D-Funktion der Höhe und kann kein
Muster erzeugen. **Das ist der Grund, warum die Narbe trotz richtiger Deckung, Breite und Gegenlicht wie
ein Teppich liest** — und es ist genau das, was der Einzelpflanzen-Prüfstand nicht zeigen kann.

**Die Fade-Kante hat eine große Bildfolge**, größer als der Ring selbst: im yaw-100-Frame liegen
**21,5 % aller Bodenpixel** jenseits von 42 m, und dort fällt der Grünanteil von 73 % auf 9,6 %. Der Ring
selbst ist bei Pitch 0 nur 4 px hoch, clippt aber auf **exakt Code 0** (bis 110 von 360 Pixeln einer
Zeile).

**Reihenfolge, endgültig — die beiden Kritiker widersprachen sich und ich entscheide gegen meine eigene
frühere Fassung:** `lod.md` begründet seine Auswahlregel selbst damit, dass die Sub-τ-Diskrepanz „the
class of error TAA is built to absorb" sei und dass es deshalb „no crossfade, no geomorph anywhere"
brauche — **die LOD-Konstruktion setzt TAA voraus.** Dazu liegt das Aliasmaximum bei **8–15 m**
(|Laplace| 0,257), mitten in der Deckungsscheibe, wo kein LOD je vereinfacht. Und ohne AA misst die
LOD-Abnahme den eigenen Rauschteppich: der Schritt an der Scheibenkante ist 0,218 → 0,072, der
Untergrund im Bestand liegt bei 0,22. Der Einwand des `art-director` — verlorene Deckung sei durch
keinen Filter zurückzuholen — trifft Post-Filter, nicht TAA mit Subpixel-Jitter, das genau diese Deckung
statistisch wiederherstellt. Sein zweiter Punkt bleibt und wird eingebaut: **die Fernstufe darf nicht
durch Ausdünnen entstehen**, und **Wind kommt vor TAA**, weil die Motion Vectors sonst zweimal gebaut
werden.

**Determinismus → Shading (Rückseiten, geklemmter Ambient) → Eigenschatten → Wind → TAA → LOD.**

## 2026-08-07 — Der Prüfstand kann jetzt leer von schwarz unterscheiden

`closeup_hd` war **nie leer, es war voll und schwarz** — und der Beleg brauchte keine Änderung, nur das
vorhandene Bild: der Himmelspass schreibt 144/177/216, **kein Himmelspixel kann Code 0 sein**, und
trotzdem waren die obersten vier Zeilen und die Horizontzeile zu 100 % exakt (0,0,0). Füllung
**89,60 %**, `blades=230770` für diese Ansicht. Alle vier von mir genannten Kandidaten einzeln
ausgeschlossen: Fleckposition (0,600 m freier Boden vor der Wand), nahe Ebene (0,05 gegen 0,600 m),
Kamera im Halm (Deckung ist 0 außerhalb des Flecks, der nackte Boden rendert), Zielrichtung.
Es ist derselbe geklemmte Ambient aus der parallelen Runde.

**Der eigentliche Mangel war, dass der Prüfstand leer nicht von schwarz unterscheiden konnte.** Jetzt
misst er `fillPct`, `cardPct`, `cardMedian` und `subjectMedian` aus dem Tiefenpuffer (drei Renders) und
meldet `rig subject_below_floor`, wenn Geometrie da ist und trotzdem nichts leuchtet — heute auf 4 von
45 Bildern, und die Warnung zeigt auf eine fremde Datei, statt sie anzufassen.

**Karte und Substrat sind getrennt, und der Zielkonflikt löste sich auf statt abgewogen zu werden:** Der
Grund für die neutrale Karte war, dass der klassifizierte Bodenshader mit seinen Defekten sonst auf das
Konto der Pflanze misst — aber `BenchGroundStage` war nie dieser Shader, sondern eine flache Ebene mit
einer deklarierten Konstante. Boden ist jetzt das deklarierte Substrat der Vorlage (`wiese` → `grasfilz`,
linear 0,1430/0,0964/0,0400), gelesen aus `ground-materials.json` und ausdrücklich **nicht** aus der
aufgelösten Zeile, weil `swardClosure` = 1,0 deren Bodenreflexion vollständig mit der Aggregatfarbe der
Halme überschrieben hat. Die 18-%-Karte steht aufrecht am linken Bildrand als **Geometrie** durch
denselben `litRadiance` — eine radiometrische Referenz, kein Bildschirmanstrich. `sward` bekommt per
Deklaration keine Karte: sein Bild IST der gemessene Quadratmeter.

**`turn180` ≠ `backlit`: Ursache gefunden, Gleichmachen verweigert.** `backlit` bewegt die Sonne, das
Turntable die Kamera; beide erreichen `sunRelDeg` 0, und die Karte — die einzige Fläche mit identischer
Orientierung in beiden — liest **67 in beiden**, das Licht ist also dasselbe. Die 30 Codes Unterschied am
Bestand liegen **innerhalb** der 42 Codes, die derselbe Stand über vier Kamerarichtungen streut
(64/88/94/106). Beide Zwangsangleichungen wären Schaden: die Kamera von `backlit` zu bewegen macht
Auf- und Gegenlicht zu zwei Kompositionen, die Sonne des Turntables zu bewegen macht acht identische
Bilder. Stattdessen ein latenter Fehler behoben — die Turntable-Sonne war hart auf 180° verdrahtet statt
blickrelativ — und beide Familien tragen jetzt `sunRelDeg` im Log. `turn000` ist `frontlit` auf das Bit.

**`sward` hat drei Lichter**, alle drei messen **93,51 %** Deckung bei Subjektmedian 106/105/88. Der
Einwand des Botanikers (schwarze Pixel unentscheidbar) ist damit beantwortet.

Szene byteidentisch: zwei Binaries aus demselben Baum, eines mit zurückgenommenen Änderungen,
**0 abweichende Pixel** bei gleichen 1 459 400 Dreiecken und 1 393 949 Halmen.

Offen: bei 90–98 % Füllung hat eine Makroansicht **keine Sichtlinie mehr zur Referenzkarte**
(`cardPct` 0 % bei `closeup_hd`); Ligula und Blattquerschnitt sind bei keiner Belichtung darstellbar,
weil das Halmmodell ein Band ohne Dicke ist; und `--rig-height 25` meldet ehrlich `fillPct = 0` auf allen
Ansichten außer `sward` und `eye` — eine 0,3-m-Narbe ist alles, was die Engine an Motiv hat.

## 2026-08-07 — Der Nichtdeterminismus war meine Orchestrierung

**`build/gpu_walk` wurde zwischen den Läufen des Kritikers von einem parallel arbeitenden Agenten
überschrieben. Die drei „Zustände" waren drei Binaries.** Direkt beobachtet: die md5 der ausführbaren
Datei änderte sich in 34 Minuten dreimal und **kehrte zu einem früheren Wert zurück** — genau die
gemeldete Phänomenologie „stabil über Minuten, dann kippt es". Die Zuordnung ist exakt: das angepinnte
Binary `052057d8…` rendert `md5 f0fbe616…`, einen der drei Hashes des Kritikers, Bit für Bit, mit
`grasL` 0,1711 gegen dessen 0,1711.

**Die Engine ist deterministisch.** 37 Läufe mit angepinntem Binary, **0 Kipper**, obere 95-%-Schranke
für einen Kipper pro Lauf 7,8 %. Und der Test, der `CLAUDE.md` Prinzip 6 direkt prüft: ein konkurrierender
`gpu_walk` streckte die Laufzeit von 11,8 s auf 74,0 s — **6,3× auf der Uhr, 0 Pixel Unterschied.**

Ausgeschlossen, jedes mit einer Messung: der **Server** (hermetische Wiedergabe aus einem eingefrorenen
428-Pfad-Korpus rendert bitgleich zum Live-Lauf; zwei Korpora 20 min auseinander, 428/428 Pfade,
0 abweichende Antworten; `baked=0 bake_fail=0 absent=0`), und das **Deckungsfeld** (`FB_FIELD_DUMP`
byteidentisch **zwischen den beiden Binaries, die verschieden rendern**). Damit ist auch meine
Kurskorrektur auf den Ersetzungspfad und den Bake-Zustand widerlegt.

**Drei Logwerte, die ich als Beweis für einen identischen Zustand behandelt habe, können sich mit dem
Grasbild strukturell gar nicht bewegen:** `blades` = Zellenzahl × Tabellenwert (das Feld geht nie ein),
`blackLog2` misst nur die Himmelsbestrahlung (das gerenderte Bild erreicht den Messer nie),
`terrainTiles` ist eine Sichtbarkeitszählung. „Die Logzeilen stimmen überein, die Bilder nicht" war kein
Rätsel.

**Zwei Agenten hatten mich gewarnt und ihre eigenen Binaries gepinnt** — einer schrieb wörtlich,
`GroundCoverStage.cpp` habe sich dreimal während seiner Messung geändert. Ich habe die Sorgfalt gelobt
und nicht verallgemeinert, und dann einen Kritiker rendern lassen, während ein Entwickler dieselbe Datei
neu baute.

Zwei Werkzeuge daraus: `sim/tools/determinism.py` (N Läufe, md5-Verteilung, Differenzkarte, **hasht die
ausführbare Datei vor jedem Lauf** und meldet einen Wechsel mitten im Experiment — gegen einen absichtlich
getauschten Binary verifiziert) und `sim/tools/tileproxy.py` (Aufnahme/Wiedergabe vor `fb-tiles`, für
hermetische Renders und zum Diffen der Serverantworten zwischen zwei Zeitpunkten).

**Nebenbefund, eigenständig zu beheben:** `FB_FIELD_DUMP` druckt nur Kanal 1 (Bucket). Kanal 0 (Höhe) und
Kanal 2 (Deckung) fehlen, also kann „der Dump ist identisch" das Feld allein nie entlasten. Hier trugen
`hCentre` und die hermetische Wiedergabe das Loch; im Allgemeinen tun sie es nicht.

Regel, ab sofort in [`goal.md`](goal.md) §7: **jede Messung pinnt ihr Binary**, und **kein Kritiker
rendert, während ein Entwickler dieselbe Datei hält.** Die vier Anker des `sim-critic` sind gegen
angepinnte Binaries neu zu messen, bevor eine dieser Zahlen geglaubt wird.

## 2026-08-07 — Der Ambient war nicht geklemmt, die Kurve schneidet ab

Zurückgenommen: meine Meldung, der Bestandsambient sei auf null geklemmt und das sei
belichtungsunabhängig bewiesen. Der Entwickler hat den Schwarzanker diagnostisch um 8 EV nach unten
gezogen (danach zurückgebaut, `grep DIAGNOSTIC` = 0) und die HDR-Verteilung der Halmpixel unter
`skylight` gemessen: **glatt und voll besetzt über 6,21 EV** (p1..p99), **42,42 % der Halmpixel unter dem
Anker**. Kein Dorn, ein Schnitt — in der Tonkurve, nicht im Shading.

Beide Kritikerbeweise zerlegt: der lineare Gain-Test kann geclippte 8-Bit-Daten nicht wiederherstellen,
also hat er nichts gezeigt; und das Karte-gegen-Halm-Argument beweist nur, dass der Halm mehr als 2,7 EV
unter der Karte liegt — was er berechtigterweise tut. Das 8-Bit-Histogramm ist mit beiden Erklärungen
verträglich, die HDR-Messung nur mit einer.

**Auch die vermutete Doppelwirkung des Bestandsabbaus gibt es nicht**, durchgespurt: `exp()` einmal im
Vertex, Quotient einmal im Fragment, `alb = col*occ` einmal, und `vis` teilt den Direktterm wieder heraus.
Die fehlenden 1,6 EV waren die Halbkugelgewichtung von `litRadiance` — Ambient bei `n·up = −1` von
**0,120 auf 0,738**, und der Sprung über die Zweiseitigkeitsgrenze (die harte schwarze Segmentkante) von
**2,60 EV auf 0,17 EV**.

Gebaut: Kiel als Normalendrehung um die Tangente (20°, `[SET]`, Randkontrast cos25/cos65 = 2,14),
`o.occ.x = max(T_tot, T_beam)`, und ein zweiter `litRadiance` mit `−nb` als durch die Blattspreite
transmittierter Himmel (`kLeafTrans` 0,85 war bereits deklariert). Null zusätzliche Dreiecke, kein neuer
freier Parameter.

| | vorher | nachher |
|---|---|---|
| Haarlinien `portrait-frontlit` | 61 | **0** |
| Schwarze Blöcke, dieselbe Ansicht | 50 223 px | **0** |
| Einfarbige Läufe quer über den Halm, `tuft` | Mittel 9,26 / max 198 / ≥64 px: 211 | **2,17 / 81 / 22** |
| Code 0 unter `skylight`, `eye` | 13,43 % | **1,44 %** |
| dito `portrait` | 70,89 % | 49,06 % |

**Der Nullbreiten-Kollaps ist NICHT behoben und wird nicht so gemeldet:** 643 Ein-Pixel-Spannen in
`b-frontlit` sind bitgleich vor und nach der Änderung. Eine Normalendrehung kann keine Silhouette ändern
— das braucht Geometrie (3 Vertizes je Station, +50 % Dreiecke) oder TAA.

Hergeleitete Restanforderung: `portrait-skylight` erreicht einstelliges Clipping erst, wenn der
Schwarzanker um **1,88 EV** fällt (`kBlackBelowA` 2,678 → 4,56). Bis dahin ist keine LOD-Zahl, die aus
Halmluminanz im unteren Bilddrittel gewonnen wird, etwas wert.

**Offen und eigenständig:** `kSelfShelter = 0,35` wird in `SurfaceLight.h` auf ein Blatt angewandt und
zieht netto **−0,53 EV** ab, obwohl ein glattes Blatt kein Subpixel-Relief hat und seine Selbstverdeckung
schon in `occ` steckt — falsches Modell für diesen Aufrufer. Und die Wolkendecke ist als **gleichförmig**
modelliert, während echter Overcast am Zenit ~3× heller ist (Moon–Spencer); das ist der einzige
Orientierungshinweis, der unter `skylight` übrig bleibt, und er fehlt.

## 2026-08-07 — Der Zeh statt des Ankers, und die Narbe wirft immer noch keinen Schatten

**Defekt 1, geschlossen.** Die Tonkurve klemmte am Schwarzanker; jetzt hat sie einen **Zeh** — unterhalb
`kToe = 0,0551` der Spanne läuft der Log-Ast in eine Exponentialfußkurve gleichen Wertes und gleicher
Steigung aus und erreicht Code 0 nur asymptotisch. `kToe = 0` reproduziert die alte Kurve bitgleich.
Anteil Code 0 unter `skylight` vorher → nachher: `portrait` **49,06 → 5,35 %**, `tuft` 7,50 → 0,29,
`sward` 15,82 → 0,43, `eye` 1,44 → 0,22; der Schwanz 1…16 wächst mit: 1,58 → **35,35 %**, 0,62 → 4,66,
1,12 → 10,43, 0,14 → 0,66.

**Die vorige Runde hatte die Größe falsch.** Mit `FB_TONE_PROBE` (Exponent 1, kein Zeh — die Kurve als
Lineal) und `FB_COVER=0` als Halmmaske gemessen: **61,2 %** der Halmpixel von `portrait` liegen unter dem
Anker, nicht 42,42 %, und p10 liegt bei **−9,237**, also **2,672 EV** unter dem ausgelieferten Anker
−6,565, nicht 1,88. Probe gegen das Bild geprüft: 48,84 % vorhergesagt gegen 49,06 % gemessen.

**Verworfen mit Messung: den Anker verschieben.** `kBlackBelowA` 2,678 → 5,350 schafft `portrait` auf
10,45 %, hebt aber den Himmelmittelwert 195,61 → **210,14**, das Terrain 164,86 → 172,14 und das
Himmelclipping 0,024 → **0,995 %** — Schwarz und Weiß sind EINE Spanne, und der Exponent wird von 1,092
auf 2,060 mitgezogen. Pattanaiks 5/32 bleibt, wo es ist; der Zeh ändert nur, was DARUNTER passiert.
Gegenprobe: Zeilen 0–375 des Demo-Frames ändern **keinen einzigen Code**, der Tiefenpuffer ist byteweise
identisch, `passes` 7 und `draws` 130 unverändert; gezeigte Spreizung 5,11 → **8,02 EV** (Band ≥ 6).

**Defekt 2, NICHT geschlossen, aber beziffert.** Der Strahlweg wird jetzt entlang der Sonne integriert
statt senkrecht (ein Newton-Schritt, ein zusätzlicher Feldabgriff, `kSlopeFloor = 0,70` gegen einen
256-Schritt-Marsch hergeleitet: 0,247 EV mittlerer Fehler, Korrelation 0,929 — die Spaltenform, die er
ersetzt, korreliert **−0,080**, sie trug gar keine Richtung). Er bewegt die Abnahme um **0,01 EV**:
Makrokontrast 1,624 → 1,641 gegen Ziel 1,74 EV, Musterkorrelation zwischen zwei Sonnenpeilungen
0,348 → 0,334.

**Der Grund ist das Feld, nicht die Näherung, und er ist gemessen:** `standMod` hat sd **0,0266 m** und
Richtungssteigung p01/p99 **±0,142** gegen `tan(11,2°) = 0,194` — eine Fläche, die flacher als die Sonne
ist, verschattet sich nicht. Die GEZEICHNETE Geometrie tut es sehr wohl: die realisierte Spitzenhülle
derselben Population auf 3-cm-Raster hat sd **0,0781 m**, und ein Strahlmarsch darüber stellt bei 11°
**61,3 %** (Peilung 0°) bzw. **61,6 %** (90°) der Hülle in den eigenen Schatten. Dazu kommt, dass die
sichtbare Oberfläche per Konstruktion bei optischer Tiefe 0 liegt: `clamp(z/swardH,0,1)` sättigt für
jeden Halm, der über `swardH` hinausragt — genau die, die der Nadirblick sieht.

Kosten des Richtungsterms: **nicht auflösbar.** ABBA-balanciert, 15 Läufe à 300 Frames, driftete die
Maschine monoton 66,97 → 70,44 ms; die gepaarte Differenz kam mit **−0,35 ms** heraus, also unter dem
Rauschen. Mehr als „unter 1 ms auf ~70 ms" ist daraus nicht zu holen und wird auch nicht behauptet.

## 2026-08-07 — Der Zeh sitzt, der Eigenschatten scheitert am Feld

**Meine hergeleitete Ankergröße war falsch, und zwar im Histogramm, nicht in der Rechnung.** Der
Entwickler hat die Halmpopulation direkt gemessen, indem er die Tonkurve zu einem Lineal machte
(`FB_TONE_PROBE`, Exponent 1, kein Zeh — dann ist ein zurückgelesener PNG-Code exakt
`(log2 L − black)/(white − black)`), Halme maskiert über einen Paarrender mit `FB_COVER=0`. **61,2 %**
der Halmpixel liegen unter dem Anker, nicht 42,42 %; der nötige Fall ist **2,672 EV**, nicht 1,88. Die
Sonde ist gegen das Bild validiert, das sie erklärt: 48,84 % vorhergesagt gegen 49,06 % gemessenes
Code-0.

**Der Anker wurde abgelehnt, der Zeh gewählt — mit Messung und mit einem Quellenargument.** Schwarz und
Weiß sind EINE Spanne: den Anker um 2,672 EV zu senken zieht Weiß mit auf +2,479 und schleift den
Exponenten aus seiner eigenen Herleitung von 1,092 auf 2,060 — **Himmelsclipping mal 41**. Und
Pattanaiks 5/32 bleibt stehen, weil es eine Aussage über den Schwarzpunkt eines ANZEIGEGERÄTS ist; ein
Zeh ändert nur, was darunter passiert, und dazu sagt die Quelle nichts. Form:
`t = kToe·exp(tlin/kToe − 1)` unterhalb `kToe = 0,0551`, gleicher Wert und gleiche Steigung am Knie,
`kToe = 0` stellt die alte Kurve bitgleich wieder her.

| unter `skylight` | Code 0 | Codes 1–16 |
|---|---|---|
| `portrait` | 49,06 → **5,35 %** | 1,58 → **35,35 %** |
| `sward` | 15,82 → **0,43 %** | 1,12 → **10,43 %** |
| `eye` | 1,44 → **0,22 %** | 0,14 → **0,66 %** |

**Die Gegenprobe wurde stärker geführt als verlangt:** statt die Horizontprüfung von Schritt 2 zu
wiederholen, wurde nachgewiesen, dass der **Tiefenpuffer byteidentisch** ist (3 686 400 Bytes, `cmp`) —
und die Prüfung ist eine reine Funktion dieses Puffers, also kann ihr Ergebnis sich nicht bewegt haben.
Zeilen 0–375 des Demobildes ändern sich um keinen einzigen Code; 2,40 % des Bildes überhaupt.

**Der Eigenschatten ist gescheitert: 0,71 EV gegen 1,74.** Die Diagnose ist die eigentliche Nachricht.
`skylight` — komplett bedeckt, kein Direktstrahl — misst 1,562 gegen 1,635 bei Sonne: **die Sonne trägt
0,066 EV Makrostruktur bei, was da moduliert ist Halmdichte.** Und der Grund liegt im Feld, nicht in der
Näherung: das Bestandsdach, gegen das das Licht rechnet, hat sd **0,0266 m** und Richtungsgefälle
±0,142, während `tan(11,2°)` = **0,194** ist. **Eine Fläche, die flacher ist als die Sonne, beschattet
sich nicht selbst.** Die gezeichnete Geometrie dagegen hat auf einem 3-cm-Raster sd **0,0781 m**, und ein
Strahlmarsch bei 11° legt **61,3 %** davon in den eigenen Schatten.

Der gerichtete analytische Term wurde trotzdem gebaut und gegen einen 256-Schritt-Marsch validiert —
mittlerer Fehler 1,060 → **0,247 EV**, Korrelation −0,080 → **0,929**, `kSlopeFloor` 0,70 aus einem
Sweep — und bewegt die Abnahme um 0,01 EV. **Der Schattenpass ist mit einer Zahl verworfen:** Kaskade 0
löst 47 mm/Texel auf, ein Halm ist 11 mm breit; eine Karte, die ihn sieht, bräuchte 2 mm/Texel, also
4096² über 4 m Radius gegen ein 44-m-Feld — und damit die Aggregatstufe, die das LOD schuldet. Zweimal
bauen wäre der Fehler.

**Kosten nicht genannt, und das war richtig:** ABBA-gepaart über 15 Läufe driftete die Maschine
66,97 → 70,44 ms, die gepaarte Differenz kam mit **−0,35 ms** heraus — der Build mit der Änderung maß
schneller. Unter der Rauschgrenze, also keine Zahl.

Offen: `kToe` ist aus EINER Population hergeleitet (`wiese`, `skylight`, 11° Sonne); kein zweites
Template, keine zweite Sonnenhöhe, kein Nachtbild hat das Knie belastet.

## 2026-08-07 — Schritt 3 (Gras), Runde 4: der Wind erreicht die Pflanze

**Die Szene deklarierte seit Beginn `windDeg 250, windMs 6.0`, und der Vektor starb in
`World::SetWeather`.** Er hat jetzt einen zweiten, gelesenen Weg: `render/WindField.h` →
`Renderer::SetWind` → `GroundCoverStage`. Ein `sin(t)` im Shader wäre genau das, was `goal.md` §4
verbietet, und 1,39 Mio. Halme können keine 1,39 Mio. Körper sein. Die Auflösung ist eine Teilung, und
sie trägt nur, weil **beide** Hälften belegt sind: das **Feld** ist ein deklarierter Fluss mit
publizierten Eigenschaften, die **Antwort** ist die geschlossene Lösung der Biegegleichung des Halms,
je Instanz ausgewertet.

**Drei publizierte Relationen, und die dritte fällt aus den ersten beiden heraus.** Py, de Langre &
Moulia 2006 (JFM 568): die Phasengeschwindigkeit der Honami-Welle ist die Windgeschwindigkeit am
Bestandsdach (*„we have globally c ≈ U"*), ihre Frequenz ist die Eigenfrequenz der Pflanze und rührt
sich mit dem Wind nicht (Frequency Lock-in, `f/f0` = 1,06 bzw. 0,81), also ist die Wellenlänge
`λ = c/f` **keine dritte Deklaration**. Die deklarierte Szene landet bei `λ/h` = 1,46, mitten im
gemessenen Band 1–4 derselben Arbeit.

**Die Antwort ist Gosselin, de Langre & Machado-Almeida 2010 (JFM 650), Gl. (5.5), gelöst statt
zitiert.** `sim/tools/elastica.py` schießt das Randwertproblem `θ''' = Cy·cos²θ` über `Cy` 0,02…44,4;
die Spitzenauslenkung passt auf **0,2702°** und 0,65 % relativ, und **beide Asymptoten sind hergeleitet,
nicht gefittet** — `Cy/6` des linearen Kragarms bei kleiner Last, `π/2` bei unendlicher. Die *Form*
braucht überhaupt keinen Fit: `1−(1−s)³` hält über die ganze Familie auf 0,0655. Der Prüfstein ist der
**Vogel-Exponent**, den der eigene Löser produziert: **−0,000 / −0,870 / −0,961** über die
Laststufen gegen die publizierten −2/3 (Dimensionsanalyse) und −1,4 (deren eigener Fit).

**Gemessen im BILD und in den ZAHLEN, und die beiden teilen unterhalb der Konstanten keinen Code.**
240 Bilder zu 1/60 s, ein Standpunkt, eine Sonne, ein Streaming-Zustand — bewegt wird allein die
**Winduhr**, die absichtlich nicht die Himmelsuhr ist. Jeder Pixel wird durch seine eigene Tiefe auf ein
30-mm-Weltgitter entprojiziert, der oberste Treffer je Säule ist das Bestandsdach, der zeitliche
Mittelwert je Zelle entfernt die 0,26 m statische Rauigkeit exakt:

| | deklariert | Bild | Zustand |
|---|---|---|---|
| Frequenz | 2,3028 Hz | **2,293** | **2,2915** |
| Wellenlänge | 0,4392 m | **0,4367** | **0,4391** |
| **Phasengeschwindigkeit** | **1,0114 m/s** | **+1,0012** | **+1,0062** |

`c/U_h` = **0,990**. Weltfest ist zweimal belegt: die in Dritteln des Bandes gemessene Wellenlänge
bleibt bei **0,4259 / 0,4350 / 0,4380 m** über 0–3,9 / 3,9–7,8 / 7,8–11,7 m Bodenentfernung — eine am
Bildschirm klebende Welle müsste dort Faktor 3 zeigen —, und die §1.9-Registrierung der **Welle allein**
(zwei Standpunkte 0,5 m auseinander, je zwei Winduhren) findet **(−0,180, −0,480) m** gegen die
vorhergesagten (−0,171, −0,470), Rest 0,0374 m gegen 0,0554 m bei der Bildschirm-Hypothese.

**`_wind` ist eingelöst.** Der Prüfstand verweigerte die Ansicht, weil drei identische Dateien eine Lüge
gewesen wären; jetzt tragen vier Rahmungen je eine 0/6/12-m/s-Zeile auf identischer Geometrie, und sie
sind es tatsächlich: auf `tuft` unterscheiden sich 96,9–98,3 % der Pixel um mehr als zwei Codes.
Am Bestandsdach der Szene gemessen sinkt es um **12,6 / 37,7 / 56,9 mm** bei 6 / 12 / 24 m/s.

**Die Kosten sind genannt, weil sie diesmal weit über der Rauschgrenze liegen: +18,09 ms, sd 0,13.**
ABBA-gepaart über fünf Blöcke `FB_WIND=1/0/0/1` auf einem gepinnten Binary — **+26,6 %** auf eine
68,1-ms-Kontrolle. Ursache ist Vertex-ALU: sechs zusätzliche `sin`/`cos`-Paare und sechs
Rodrigues-Drehungen je Vertex über 15,3 Mio. Vertices, also **1,18 ns/Vertex**. Die Schicht war schon
1,69× über dem Budget; bezahlt wird das in der LOD-Runde.

Offen und benannt: `kGustAmp` = 0,5 ist die einzige Zahl im Fluss ohne Quelle und die, auf die das Bild
am stärksten reagiert; die Antwort ist **quasistatisch**, obwohl die Anregung durch das Lock-in exakt
auf der Resonanz sitzt — dort antwortet ein Halm über die Dämpfung, nicht über die Steifigkeit; die
Amplitudenabnahme zum Ausblendrand ist nur in der **Frequenz** sauber belegt (2,298–2,315 Hz über vier
Entfernungsbänder), nicht in der Amplitude, weil dem 30-mm-Weltgitter jenseits 22 m die Pixel ausgehen.

## 2026-08-07 — Der Wind ist Physik, und der Anker ist publiziert

**Py, de Langre & Moulia 2006, JFM 568:425–449** liefert den Anker wörtlich: *„the combined parameter
c = λf, which is the phase velocity, would not show any lock-in behaviour: we have globally c ≈ U as in
a Kelvin–Helmholtz instability"* — `U` gemessen mit einem Hitzdraht direkt über dem Bestandsdach. Dazu
Frequenz-Lock-in `f = f₀` unabhängig von `U` (f/f₀ = 1,06 Luzerne, 0,81 Weizen, beide = 1 innerhalb des
Sukhatme-d-Tests).

**Die drei Beziehungen sind nicht unabhängig:** aus `c ≈ U` und `f = f₀` folgt `λ/h = Ur` zwingend. Wer
zwei deklariert, erzwingt die dritte — deklariert wurden die beiden gemessenen, und die dritte kam mit
`λ/h = 1,46` innerhalb des publizierten Bandes heraus.

| | deklariert | Bild | Zustand |
|---|---|---|---|
| Frequenz | 2,3028 Hz | 2,293 Hz | 2,2915 Hz |
| Wellenlänge | 0,4392 m | 0,4367 m | 0,4391 m |
| **Phasengeschwindigkeit** | 1,0114 m/s | **+1,0012 m/s** | +1,0062 m/s |

`c/U_h = 0,990`. Bild und Zustand stimmen auf **0,5 %**, und die beiden Pfade teilen unterhalb der
Konstanten keinen Code. Die deklarierten 6,0 m/s liegen auf 10 m; das Bestandsdach sieht 1,0114 m/s über
das Logprofil (d/h 0,67, z0/h 0,13, beide innerhalb der publizierten Bänder, Sensitivität ±10 %).

**Die Biegung wurde gelöst statt zitiert:** die Elastica `θ''' = Cy·cos²θ` aus Gosselin, de Langre &
Machado-Almeida 2010, JFM 650, numerisch integriert. Die Validierung ist, dass der Löser den
publizierten **Vogel-Exponenten reproduziert** — Theorie −2/3, gemessene V = −0,000 / −0,870 / −0,961
über steigende Last. Prüfung, nicht Anpassung. Die geschlossene Spitzenauslenkung hat beide Asymptoten
hergeleitet (`Cy/6` linear, `π/2` im Grenzfall), nur das Knie ist auf 0,65 % gefittet, und die Biegeform
`θ(s)/Θ = 1−(1−s)³` braucht **keinen Fit** (max. Fehler 0,0655 über die ganze Familie).

**Weltfestigkeit, umsonst und trotzdem der beste Beweis der Sitzung:** eine bildschirmfeste Welle hat
konstante Wellenlänge in PIXELN, also eine mit der Entfernung wachsende in Metern. Über drei
Entfernungsdrittel von 0 bis 11,7 m gemessen: **λ = 0,4259 / 0,4350 / 0,4380 m**, 2,8 % Streuung gegen
den Faktor 3, den die Nullhypothese zeigen müsste. Zusätzlich die §1.9-Methode auf der Welle allein:
vorhergesagt (−0,1710, −0,4698) m, best fit (−0,1800, −0,4800) m, Residuum 0,0374 gegen 0,0554 m bei
Nullverschiebung.

**Die Szene bekam keine zweite Wahrheit:** der Renderer hat jetzt eine **zweite Uhr**
(`SetWindClock` neben `SetSkyClock`), und `--seq N --seq-dt S` rückt nur diese vor — ein Standpunkt, eine
Sonne, ein Streaming-Zustand, eine bewegte Größe. `_wind` auf dem Prüfstand ist damit eingelöst
(0/6/12 m/s, 96,9–98,3 % der Pixel unterscheiden sich um mehr als 2 Codes).

**Kosten: +18,09 ms, sd 0,13, auf 68,1 ms Kontrolle = +26,6 %**, ABBA-gepaart über fünf Blöcke mit
angepinntem Binary. Ursache exakt: 6 Sinus-Kosinus-Paare und 6 Rodrigues-Drehungen je Vertex über
15,3 Mio. Vertizes = **1,18 ns/Vertex** — Vertex-ALU, nicht Füllrate.

**Zwei Selbstkritiken des Erbauers, beide berechtigt:** `kGustAmp` = 0,5 ist die einzige Zahl im Fluss
ohne Quelle und die, auf die das Bild am stärksten reagiert. Und die Antwort ist quasistatisch gerechnet,
obwohl das Lock-in die Welle **genau auf** die Eigenfrequenz legt — dort antwortet ein Halm über die
Dämpfung, nicht über die Steifigkeit, und aerodynamische Dämpfung in Luft ist stark. Die wahre Amplitude
liegt vermutlich **unter** der statischen. Benannt, ungemessen.

**Übergabe an TAA, ausdrücklich hinterlassen:** die einzige zeitabhängige Größe ist die Phase `wave.z`,
und das Uniform trägt `wave.w` = dieselbe Phase einen Frame früher; `windBend`/`windTipAngle`/
`windBendShape` nehmen den Winkel als Argument statt eine Uhr zu lesen. Eine Vorframe-Vertexposition ist
dieselbe Vertexfunktion an `wave.w`.

## 2026-08-07 — Schritt 3, Runde 5: TAA. Die Deckung, die nie im Bild war, kommt über die Frames zurück

**Die Übergabe der Windrunde hat gereicht, wörtlich.** `bladeStation` nimmt den Kippwinkel als Argument,
also ist die Vorframe-Position dieselbe Funktion an `wave.w`. Neu hergeleitet wurde nichts; dazugekommen
sind genau zwei Größen im `FrameContext` — die Vorframe-View-Projection und der Schritt des Auges — plus
die beiden Jitter, deren Differenz der Resolve **einmal** abzieht, für die tiefenrekonstruierte und die
vertex-geschriebene Hälfte gleichermaßen.

**Bauform:** Subpixel-Jitter (Halton(2,3), 8 Phasen) auf demselben Term der Projektion, auf dem schon der
Boresight-Versatz sitzt — eine Scherung des Frustums, keine Verschiebung der Welt. Dazu ein **zweiter
Farbanhang** an der Szenen-Pass (`rg16float`, Sentinel −1e4 = „weltfest") und ein eigener Resolve-Pass
zwischen Occlusion und Tonemap. Pass-Zahl 7 → **8**, nativ wie im Browser. Verworfen: FXAA/SMAA (kann
keine Deckung erfinden), TAA im Anzeigeraum (8 Bit Historie, und der Overlay-Pass verseucht sie), Resolve
im Tonemap-Pass per MRT (spart einen Pass, macht aber aus einer Klasse zwei Aufgaben).

**Weltfest bleibt weltfest, und der Beweis ist ganzzahlig statt korreliert** — was zählt, weil genau die
Bildkorrelation aus §1.9 bei 1,39 Mio. Halmen gestorben ist. `FB_JITTER=1.0,0.0` muss das Bild von
`FB_JITTER=0,0` sein, um **exakt ein Pixel** verschoben: mittleres |ΔY| **0,020 Codes** über 482 263
Bodenpixel, 0,207 % über 2 Codes, Tiefenpuffer **bitgleich** bis 35 m. Dieselbe Rechnung ohne die
Verschiebung: 10,667 Codes und 43,8 %. Kein Parallaxe-Anteil, in 0–3 m dieselbe Verschiebung wie in
44–80 m.

**Die fünf Abnahmen** (gepinntes Binary `a386ccc0`, beide Arme dasselbe Binary über `FB_TAA=0/1`):
Kanten 7,740 % → **1,757 %** in der Szene und 8,034 % → **1,091 %** auf `eye-frontlit`. |Laplace| im
Band 8–15 m, dem gemeldeten Maximum, 0,2398 → **0,0850**; über allen Bodenpixeln 0,2392 → 0,1074. Von
den 648 Ein-Pixel-Spannen bleiben **373**, und die Zahl, die die Krankheit wirklich misst — eingeschlossene
Bruchstücke bei gleicher Nachweisschwelle — fällt von 59/160/546 auf **23/79/431** (Schwelle 2/8/32
Codes). Gegen eine 16×-Referenz (16 gepinnte Subpixel-Phasen, in linearem Licht gemittelt): RMSE 11,432
→ **3,959**, Nyquist-Oktave 4,305× → **0,956×**, Mittelband 2,242× → **0,924×**.

**Kein Ghosting, und die naheliegende Messung dafür war falsch.** Residuum gegen den Vorframe-Schritt zu
korrelieren ist **verzerrt**: beide Terme enthalten das Aliasrauschen desselben Frames mit gleichem
Vorzeichen, das gibt +0,63 auch bei null Nachzieheffekt. Ersetzt durch dieselbe Pose auf zwei Wegen —
**gefahren** (Frame k einer Sequenz) gegen **gesetzt** (Kamera hingestellt, Historie verworfen, 240
Frames eingeschwungen). Ergebnis: **0,148 Frames Nachlauf** bei 3,8 px/Frame Bildbewegung = **0,56 px**,
und das gefahrene TAA-Bild ist der geisterfreien Antwort *näher* (4,00 Codes) als ein gar nicht
geglättetes Bild derselben Pose (5,01).

**Zwei Konstanten wurden gemessen statt gesetzt.** γ = **1,5** und nicht Karis' 1,0: bei 1,0 verliert
diese Szene ein Viertel ihres echten Details (Mittelband 0,753×), bei 2,0 kommt die Nyquist-Oktave über
1,0 zurück. Und die Rückkopplung ist **keine Konstante** — bei 21,8 px/Frame filzt eine Historie, die
jeden Frame weit versetzt nachgeschlagen wird; `α = clamp(0,1 + |v|_px · 0,015, 0,1, 0,85)` ist der
größte Anstieg, der das Versprechen im Schwenk noch hält (Nyquist 1,118× gegen 2,504× bei 0,030).

**Die Kosten sind die eigentliche Nachricht: +20,33 ms, und der Resolve ist davon 0,798 ms.** Der Rest
ist der Bewegungsvektor des Grases — 3,11 ms für den zweiten Anhang über 1,4 Mio. Halme und **12,23 ms**
für die zweite Auswertung der Biegegleichung je Vertex. Was diese 12 ms kaufen, ist gemessen und klein:
**18 % weniger Nachlauf** (0,204 → 0,168 Frames) bei 6 m/s. Speicher **17,578 MiB**, GPU-seitig — der
WASM-Heap bleibt bei exakt 256 MB, die Historie kostet den Browser nichts, was er hätte wachsen müssen.
**Verworfen mit Messung:** die Vorframe-Station erster Ordnung spart 4,63 ms und irrt sich um bis zu
**56 Codes** an den nächsten Halmen, weil die Welle 0,48 rad Phase je Frame läuft — kein kleiner Winkel.

**Offen und benannt:** ein Schwenk weicht auf (0,299 des Standbild-Mittelbands bei 3,8 px/Frame), und
der übliche Ausgleich, ein Schärfer auf dem Resolve, ist nicht gebaut. Die Occlusion liegt außerhalb der
Akkumulation. Ein Disocclusion-Test über die Tiefe fehlt; die Nachbarschaftsklammer trägt den Nachlauf
allein.

## 2026-08-07 — TAA: die gestrichelten Halme waren verlorene Deckung

Subpixel-Jitter in der Projektion (Halton(2,3), 8 Phasen, ±0,5 px) auf **demselben Term** wie der
Boresight-Versatz — eine Scherung des Frustums, keine Verschiebung der Welt; `camRay()` zieht ihn ab,
damit Himmel und Sonne denselben Strahl abtasten wie die Geometrie. Bewegungsvektoren als zweiter
Farbanhang mit Sentinel −1e4 = weltfest: weltfeste Pixel werden im Resolve aus der **Tiefe**
rückprojiziert (exakt, kostet keinen Schreibvorgang), nur die Bodendeckung schreibt echte Bewegung, und
die opaken Stages danach schreiben den Sentinel — sonst erbt eine Fassade die Geschwindigkeit eines
Halms. Pässe 7 → 8.

| | ohne | mit |
|---|---|---|
| Kanten > 40 Codes, Szene | 7,740 % | **1,757 %** |
| dasselbe, `eye-frontlit` | 8,034 % | **1,091 %** |
| \|Laplace\| bei 8–15 m | 0,2398 | **0,0850** |
| RMSE gegen 16×-Referenz | 11,432 | **3,959** |
| **Nyquist-Oktave relativ zur Referenz** | **4,305×** | **0,956×** |
| Nachlauf bei 3,8 px/Frame | — | 0,148 Frames = **0,56 px** |

Die Nyquist-Zeile ist die Aussage: 0,956× heißt aufgelöst, nicht weichgezeichnet.

**Zwei Messungen verworfen, weil sie falsch sind.** (a) Die naheliegende Geistermessung — Residuum gegen
den Vorframe-Schritt korrelieren — ist verzerrt, weil beide Terme das Aliasrauschen desselben Frames
enthalten: **+0,63 bei null Nachzieheffekt**. Ersetzt durch dieselbe Pose gefahren gegen gesetzt; das
gefahrene TAA-Bild liegt der geisterfreien Antwort näher (4,00 Codes) als ein ungeglättetes derselben
Pose (5,01). (b) **Meine eigene Abnahmegröße war die falsche:** die 1-px-Spannen fallen 648 → 373, die
2-px-Spannen **steigen** 499 → 582 — weil ein korrekt aufgelöster subpixelbreiter Halm 1–2 px breit
*ist*. Die Krankheit ist die Lücke, nicht die Breite; eingeschlossene Bruchstücke bei gleicher Schwelle
59/160/546 → **23/79/431**. Das Instrument wurde vorher gegen die publizierten 643/498 kalibriert und
reproduzierte 648/500.

**Weltfest ganzzahlig statt korrelativ:** ein Jitter von exakt einem Pixel muss ein exakt um ein Pixel
verschobenes Bild ergeben. Gemessen mittleres |ΔY| **0,020 Codes**, 0,207 % über 2 Codes, Tiefenpuffer
bitgleich bis 35 m; ohne die Verschiebung 10,667 Codes. Diese Probe trägt bei jeder Instanzdichte, wo die
Bildkorrelation aus §1.9 versagt.

Zwei Konstanten gemessen statt gesetzt: γ = **1,5** (bei Karis' 1,0 verliert die Szene ein Viertel ihres
echten Details, bei 2,0 kommt die Nyquist-Oktave über 1,0 zurück) und die Rückkopplungsrampe
`clamp(0,1 + |v|px·0,015, 0,1, 0,85)`.

**Die Windübergabe hat wörtlich gereicht:** `bladeStation` nimmt den Kippwinkel als Argument, also ist
die Vorframe-Position dieselbe Funktion an `wave.w`. Neu hergeleitet wurde nichts.

**Kosten 20,33 ms (83,31 → 103,64 ms), Speicher 17,578 MiB GPU, Browser-Heap unverändert 256 MB.**
Aufgeschlüsselt: Resolve **0,798**, Bewegungsanhang 3,112, **Vorframe-Blattstation 12,23**. Der Erbauer
markiert den letzten Posten selbst als faul: *„12 der 20 ms sind eine zweite Auswertung der
Biegegleichung je Vertex, und sie kaufen 18 % Nachlauf. Ein aggregiert gezeichneter Halm hat keine
Station, die man zweimal auswertet."*

Offen: ein schneller Schwenk weicht auf (bei 3,8 px/Frame hält der Resolve 0,299 des Standbild-
Mittelbands), der übliche Ausgleich — ein Schärfer auf dem Resolve — ist nicht gebaut und nicht gemessen.

## 2026-08-07 — Was andere tun, und wo niemand eine Antwort hat

Recherche zur Fernfelddarstellung von Gras, ausgelöst von der Eignervorgabe *„wenn Geometrie nicht mehr
nötig ist, wird sie zur Fragmentfarbe des Bodens."* Belegte Befunde, Quellen in der Agentenmeldung:

| Titel | Befund | Quelle |
|---|---|---|
| **Ghost of Tsushima** | über 1 Mio. Kandidaten, **83 000 gerendert, 2,5 ms**. Wir zeichnen 1 393 949 — **Faktor 17** | sekundär, drei unabhängige Mitschriften |
| dito, Fernfeld | *„render artist-created textures at that terrain location instead of the underlying material"* — **die Zielform ist belegt**, aber die Texturen sind gemalt, und das ist bei uns ausgeschlossen | sekundär |
| dito, Anti-Popping | vor dem Kachelwechsel drei von vier Halmen ausblenden **und** die Halmform zur niedrigen Stufe hin überblenden | sekundär |
| **SpeedTree** | *„Grass models also do not use billboards for the lowest LOD since there is no LOD in the grass system"* + *„The far clipping plane is used to keep the grass's population restricted to a short distance."* **Unsere Referenz umgeht die Frage** — Bäume bekommen Billboards, Gras wird weggeschnitten | primär (Unity-Spiegel des Runtime-SDK-Handbuchs) |
| **Horizon Zero Dawn** | schrumpft statt wegzulassen: *„we vertically push the vertices of the mesh down · Displacement = [Percentage of Object Height] based on Distance to Camera"*, dazu *„we scale the whole animation part down"* — **die Animation fährt mit herunter**, also genau `goal.md` §4 in einem ausgelieferten Spiel | primär |
| **Unreal 5.7** | *„once Nanite clusters become small enough they seamlessly transition to voxels… without resorting to billboards or LODs in the distance"*, *„near pixel-sized aggregate voxels that preserve triangle details, animation, and material properties"*. Epic selbst: *„use caution when shipping with it."* Der generische Mechanismus, aber ein **Volumen**, keine Fragmentfarbe | primär |
| Unreal, Kosten | *„Masked-out pixels cost nearly as much as drawn pixels."* Erklärt unsere Füllbegrenzung (50,9× Dreiecke für 11,4× Zeit) | primär |
| Unreal 5.1 | *„Preserve Area … prevents thinning out of geometry at far distances when enabled for foliage"* — Cooks Aggregaterhaltung als ausgelieferter Schalter | primär |
| **Witcher 3** | **umgekehrte Richtung:** Draufsicht des Geländes als „Pigment Map", die Grashalme werden **daraus eingefärbt**. Kommen beide aus einer Quelle, kann die Kante farblich nicht springen — löst aber die **Verdeckung** nicht, und die trägt bei uns die Differenz | primär (OCR der GDC-2014-Folien) |
| Red Dead 2, Death Stranding | **nichts gefunden.** Kein Vegetationsvortrag in „Advances in Real-Time Rendering" über alle Jahrgänge | — |

**Zwei Ergebnisse, die etwas entscheiden.** Erstens: Die Größenordnung, in der ein ausgeliefertes
Open-World-Spiel arbeitet, ist **einige zehntausend gezeichnete Halme**, nicht 1,4 Millionen. Zweitens:
**Für die Fernstufe von Gras gibt es keinen Stand der Technik, an dem man sich entlanghangeln kann** —
SpeedTree schneidet weg, GoT malt, Unreal voxelisiert seit einer Version und warnt davor. Das ist kein
Grund, es nicht zu bauen; es ist der Grund, es selbst zu belegen.

**Ehrlichkeit der Quellenlage:** jede Ghost-of-Tsushima-Zeile ist sekundär. Der Originalvortrag ist ein
20-MB-Bilddeck ohne Textebene, drei unabhängige Mitschriften stimmen überein, aber **keine Quelle nennt
die Umschaltentfernungen in Metern**, und keine sagt, ob die 2,5 ms nur das Gras sind.

## 2026-08-07 — Vier Eignerentscheidungen, die den Entwurf tragen

**1. Die Fernstufe IST der Bodenshader.** *„wenn geometrie nicht mehr nötig ist, wird sie zur
fragmentfarbe des bodens."* Damit stoßen nicht mehr zwei Systeme aneinander — es gibt keine Kante zu
verstecken, weil es keine Grenze zwischen zwei Systemen gibt, nur einen Maßstabswechsel in einem.
Vertrag in [`render/vegetation.md`](render/vegetation.md) `## Spec`.

**2. Das Optimierungsziel ist 16,67 ms, nicht „50 % Last".** *„720p60 ist das optimierungs target, dann
läuft 720p30 mit sicherheit."* Eine Frame-Zeit kann „50 % GPU-Last" nicht messen, 16,67 ms kann sie.

**3. Verfall ist diskret: drei Epochen × drei Stufen, eine Auswahl.** Damit entfällt die
Interpolationsfrage, statt vertagt zu werden — und jede Stufe muss für sich verteidigbar sein, was
prüfbar ist. Nicht bauen; nur zwei Indizes durchreichen. `epoch` und `decay` kommen heute **nirgends**
im Code vor.

**4. Die einzige LOD-Konstante ist das Budget.** τ, Radius, Halmzahl, Segmentzahl werden **gelöst**.
Konflikt mit Prinzip 6 aufgelöst durch drei Ebenen: Budget deklariert · Kostenmodell **gemessen und dann
festgeschrieben** · alles Übrige gelöst. Die gemessene Frame-Zeit darf berichtet werden, **nie
zurückwirken** — sonst gibt das Tempo das Ergebnis.

**Dazu: ein Pixel, eine Schattierung.** *„jeder bildpunkt sollte ungefähr die gleiche rechenzeit
bekommen."* Das ist der Visibility Buffer mit Deferred Texturing (Guerrilla, Horizon Forbidden West,
ausdrücklich für Laub), und daraus fällt eine szenenunabhängige Latte: **16,67 ms / 921 600 px = 18,1 ns
je Pixel.** Und es ist dieselbe Aussage wie Entscheidung 1 von der Kostenseite: wenn ein ferner Pixel
fünfzig Halme überdeckt und **eine** Schattierung bekommt, muss diese eine das Aggregat sein.

## 2026-08-07 — Gras ans Ende, und Gras ist statisch

**Gras rutscht ans Ende der Reihenfolge** (*„zuletzt machen oder garnicht"*). Begründung aus der
Recherche: Bruneton & Neyret haben den **Wald** gelöst (0,6 ms shader-map bei 180 000 Bäumen) und Gras
ausdrücklich als ihre offene Zukunftsarbeit benannt. Wir haben die Aggregatmaschine am Fall ohne
Präzedenzfall gebaut statt am gemessenen. Dazu die eigenen Zahlen: **101 von 103,64 ms für eine Schicht
ohne jede Silhouette**, während der lauteste Bildfehler *„ein Wald ohne Bäume"* ist — 98 % der
Bodenpixel innerhalb 50 m sind `laubmischwald` und es steht kein Baum darin.

**Gras ist statisch, und alles unterhalb der Baumgröße auch** (*„fallout 4 war komplett statisch und das
sah gut aus"* — eine der drei deklarierten visuellen Referenzen des Projekts, also unser eigener
Maßstab). Das streicht **33,43 ms**: Wind im Vertex-Shader 18,09 · TAA-Vorframe-Blattstation 12,23 ·
TAA-Bewegungsanhang 3,112. **103,64 → ~70 ms durch eine Entscheidung statt durch Optimierung.**

Und es löst die schwerste offene Frage der Recherche auf: **niemand hat je ein Aggregat unter
Windmodulation gezeigt.** Die Fernvarianz des Ozeanpapiers enthält kein `t`; das Waldpapier schreibt
*„trees cannot be animated to move in the wind."* Beide publizierten Lösungen sind im Fernfeld statisch —
wir wären die Einzigen gewesen, die es versuchen.

Prinzip 6 bleibt unberührt: *„nichts bewegt sich von selbst"* beschränkt, WIE sich etwas bewegen darf,
nicht DASS etwas stillsteht. Ein statischer Halm behauptet nichts und braucht keinen Anker. Verboten
bleibt eine kleine Restbewegung „für den Eindruck". Die Windarbeit bleibt erhalten — `WindField`,
Elastica-Löser, zweite Uhr, `--seq` — für Zweige und Rotor, also für das, was sich bewegen soll.

## 2026-08-07 — Das Orakel wartet auf die Welt, nicht auf eine Passzahl

Die vorige Runde hat den Befund gestellt, diese behebt ihn — zwei Defekte, beide gemessen.

**Defekt 1: `--warm N` garantierte keine Residenz mehr.** Seit dem Kachel-Pool lädt asynchron; dieselbe
Szene, 640x360: `--warm 240` -> `progress` 0,49, `--warm 600` -> 1. Jeder Vergleich bei fester Passzahl
verglich seither teilgeladene Szenen. **`--warm` ist jetzt die OBERGRENZE**, gewärmt wird bis
`World::Resident()` — Geometrie-Zielschnitt vollständig, der 3x3-OSM-Gebäudeblock dekodiert, kein
Gebäude-DAG in Arbeit. Greift die Grenze, ist es `warm_ceiling_reached`, Exit 2, **kein PNG**.
`BuildingField::Build()` scannt dafür den ganzen Block statt beim ersten Treffer abzubrechen und meldet
`PendingTiles()` als Zahl.

**Defekt 2: das Bild hing am Ankunftszeitplan der Kacheln.** Reproduziert mit dem alten Verfahren
(`--warm 900`, kein Reset): drei Läufe, drei Hashes, **0,428 / 0,469 / 0,476 %** abweichende Pixel bei
bitgleicher Tiefe. Behoben durch `ResetTemporal()` nach der Residenz plus `TemporalSettleFrames()`
weitere Frames **ohne Weltschritt**. Dazu gehörte ein zweiter Fund: `ResetTemporal()` leerte den
Akkumulator, ließ aber die **Jitter-Phase** stehen — eine Settle-Strecke, die bei Phase `n mod 8`
beginnt, besucht dieselben acht Positionen in gedrehter Reihenfolge und die Rückkopplung gewichtet sie
ungleich. `TemporalJitter::Reset()` gehört dazu, sonst bleibt das Bild eine Funktion der Aufwärmlänge.

**Die Settle-Zahl ist gemessen, nicht gesetzt:** `--settle N` gegen `--settle 512`, Pixel mit mehr als
zwei Codes Unterschied — 3924 bei 0, 22 bei 48, **7 bei 128**, und 7 bei 192/256/384. 128 ist das Knie,
`kTemporalSettleFrames` steht dort. Die verbleibenden 7 px und die 0,15–0,47 % Ein-Code-Pixel sind das
letzte Bit der f16-History: die Akkumulation läuft in einen GRENZZYKLUS der acht Jitter-Phasen, nicht in
einen Fixpunkt, deshalb wird der Unterschied gegen eine *andere* Settle-Länge nie null.

**Abnahme.** `FB_TILEWORKERS=n` (nativ) erzwingt verschiedene Ankunftsreihenfolgen — 1/2/4/6 Worker
erreichen die Residenz nach **517/437/343/260** Pässen. 16 Läufe `tools/determinism.py` über diese vier
Breiten: **ein** Hash, `b9a48a34…`. Sequenzen (`--seq`) und der Subject-Bench (`--rig`, 39 Bilder)
ebenso. Das Referenzbild hat sich gegenüber dem alten `--warm 900`-Stand um **8,46 %** der Pixel (0,42 %
über 2 Codes, max 39) bewegt — der alte Stand war mitten in der Akkumulation aufgenommen.

`FB_TAA=0` bleibt als **Messwerkzeug**: es entwaffnet Jitter und History, behält aber Pass, Puffer und
Passzahl. Drei Aufgaben — den Beitrag des Filters beziffern, den Kritikern das ungefilterte Bild zeigen,
und die deterministische Grundlinie liefern (ohne TAA ist das Frame über 1/2/6 Worker byteidentisch,
gemessen).

**Nicht behoben, mit Messung: die schwarzen Flecken an Gebäuden.** Reproduziert im Dorf
(`--stepE -13 --stepN 2035 --yaw 90`): 834 Pixel, die mehr als 25 Codes unter ihrem 7x7-Median liegen,
auf Gebäudewänden im mittleren Bereich (mittlere Entfernung der Fleckenpixel **243 m** gegen 31 m im
Bild). **Sie sind kein TAA-Artefakt** — ohne TAA sind es 834, mit TAA 724. Ausschluss: AO-Stärke 0
ändert nichts (836), Schatten aus lässt sie stehen (491), erst die eingefrorene Tonkurve (`FB_GEOM=1`)
lässt sie verschwinden (149) — sie liegen also im HDR-Radianzbild der Gebäude und werden vom Zeh der
gemessenen Kurve auf schwarz gedrückt. Die Tiefe an den Flecken zeigt zwei Wandebenen ~4 cm auseinander:
überlappende OSM-Grundrisse, die auf Subpixelbreite abwechselnd die besonnte und die beschattete Seite
zeigen. Was der Eigner als „laufen hinterher" sieht, ist die 0,56-px-Nachlaufzeit von TAA auf einem
flackernden Muster — die Ursache liegt in der Gebäudegeometrie, nicht im Filter. Offen.

## 2026-08-07 — Das Referenzbild ist nicht reproduzierbar, und die Ursache ist TAA

Beim Aufräumen gemessen: **dasselbe Binary liefert je Sitzung einen stabilen, aber unterschiedlichen
Hash.** Innerhalb einer Sitzung 4–5 Läufe byteidentisch, über Sitzungen `d6a2` → `8b25` → `7ea6`.
Geometrie, Belichtung und Einstrahlung sind dabei bitgleich (`terrainTris=49062 buildingTris=16025
blades=98200`); der Unterschied liegt in **3 961 von 921 600 Pixeln**, max. Kanaldelta 18, Zeilen
318–719 — die Bodenhälfte.

Nachgewiesen durch Ausschluss und Kontrolle:

| | `--warm 240` | `--warm 480` |
|---|---|---|
| Vorgabe | `7ea6…` | `7eba…` |
| photometrische Glättung aus | `7ea6…` | `7eba…` |
| **TAA aus** | `e3d0…` | **`e3d0…`** |

**Die TAA-History konvergiert bei stehender Kamera nie exakt** — der Jitter rotiert weiter und die
History ist ein undichter Akkumulator. Das Frame ist damit eine Funktion der Aufwärmpasszahl und, über
Sitzungen, davon, in welchem Pass welche Kachel ankam.

**Folge für die Abnahme: `sim/walk-demo.png` ist nur bei fester `--warm`-Zahl UND ausgeglichenem
Streaming ein Referenzbild.** Ein Frame-Orakel muss die History zurücksetzen und danach eine feste Zahl
Frames laufen, nachdem das Streaming konvergiert ist — sonst ist es eine Funktion des Zeitplans statt
der Szene. Offen, gehört dem Renderer.

## 2026-08-07 — `ground-cover.md` aufgelöst statt gelöscht

Die Halmgeometrie ist weg (`GroundCoverStage`, `CoverGrid.h`, das CPU-Bodenfeld in `World.cpp`), aber
`doc/render/stages/ground-cover.md` beschrieb zwei Dinge in einer Datei: die gelöschte Stufe **und** den
Bodenshader, der weiterlebt. Die 756 Zeilen sind deshalb **aufgeteilt** worden, nicht entsorgt.

| wohin | was |
|---|---|
| `render/stages/terrain.md` | Bodenmaterial (Klassenliste, Feldtabelle, BRDF-Lesart, Reliefexponent), der Aggregatterm als eigener Spec-Abschnitt, Gradnetz, Level-0-Klasse, Klassenstapelung, alle Materialmessungen, Munsell-/ECOSTRESS-Wissen, Quellen 18 und 25–29 |
| `render/vegetation.md` | die Windkette (`WindField.h`, Elastica-Fit, Py/Gosselin) als `## Knowledge` samt Quellen 30–33, plus deren `[SET]`-Lücken |
| gelöscht | Instanzpfad, Halmform, Kiel, Fünf-Segment-Streifen, Horste als Geometrie, Deckungsscheibe, Fade-Kante, Feldbefüllung, Winddurchreichung an den Halm |

Als **Wissen** überlebt, mit Messung, in `## Gaps` der aufnehmenden Datei: die Deckung kostete 10,3 ms
von 17,0 ms GPU (61 % des Frames) für zehn Meter Band · der Übergang verfehlte \|Δ(R−G)\| < 6 und
\|ΔL\| < 5 · der Eigenschatten steht bei 0,326/0,402 EV gegen ein Ziel von 1,74 · Farbabgleich schließt
keine Übergabe (21 Codes R−G, 17 L bei **identischem** Albedo) · die deklarierte Radius-Auswahl bewegte
die Bodenprofil-Energie um Faktor 23 auf 8 m · Bruneton & Neyrets Drei-Pixel-Kriterium hat ohne nahe
Geometrie keinen Gegenstand mehr. `FB_TAU` hat wieder genau einen Besitzer, `render/ClusterDag.h`.

Der Vermerk über die **~466 verlorenen Zeilen gemessenen Zustands** ist mitgewandert und steht jetzt in
`terrain.md` `## Gaps`. Nichts davon ist rekonstruiert worden: eine erinnerte Messung ist keine.

`verify-trees` bewegt sich dadurch nicht — es zählt Verzeichnisse, und `doc/render/stages/` ist weiter
belegt: 9 Waisen vorher, 9 nachher.

## 2026-08-07 — Der Baumgenerator ist C++, und er liefert den Prototyp Bit für Bit

`~/Git/wasm-tree` ist neu implementiert, nicht portiert: sieben Dateien in `sim/src/world/`, C++17,
`namespace outshine::World`, eine Klasse je Datei — `TreeVec3.h` · `TreeRandom.h` · `TreeSpecies` ·
`TreeMesh.h` · `TreeGrower` · `TreeLeaf` · `LeafAngleDistribution`. Keine zweite C-Insel;
`world/terrain/` bleibt die einzige. Die 16 Arten-JSON liegen **byte-identisch** unter
`sim/assets/world/species/`. `make treebench` misst, `walk` und `wasm` linken mit.

**Die Abnahme ist bestanden, und zwar in ihrer härtesten Form.** Der Prototyp-Kern wurde nativ gebaut
und ausgeführt; verglichen wurden alle 16 Arten × 5 Puffer: **80 von 80 byte-identisch**, 2 159 272
Floats und 996 096 Indizes, 12,6 MB. Keine Abweichung war zu benennen, weil es keine gibt.

**Topologie ist werkzeugkettenunabhängig, Koordinaten sind es nicht.** Zählungen und Indizes stimmen
über `-O0 -O1 -O2 -O3 -Os`, `-ffp-contract=off` und **emcc `-O1` unter node**; die Positionen driften
nativ↔wasm um höchstens 6,8e-6 der Einheitshöhe (Normalen 4,8e-5). `-ffast-math` bricht es: `kiefer`
verliert eine Nadel, weil `(int)(0.85f/0.0085f)` exakt auf 100 sitzt.

**Ein Baum wird je ART erzeugt, nie je Instanz** — 0,18–1,10 ms (Mittel 0,417) und 390–2042 kB
(Mittel 770). 5000 Bäume je Instanz wären 2,1 s und **3,7 GB**; je Art sind es 12 MB.

**`G(el)` wird jetzt am gewachsenen Baum GEMESSEN**, nicht deklariert — der Kreis, den Gras nie
geschlossen hat. Aus `leaf_pts` folgt mit „die Spreite rollt frei um ihren Stiel" analytisch
`E_roll|n·s| = (2/π)·√(1−(u·s)²)`; übrig bleibt ein Mittel über Population und Azimut. Prüfung, die
niemand eingebaut hat: die drei Nadelbäume landen auf der isotropen Vorhersage (mittlere Stielhöhe
π/2−1 = 32,70°; gemessen 32,63 / 33,05 / 33,52°, G flach bei 0,494–0,503). Bäume sind nahezu sphärisch,
`Sward.h`s Wiese ist erectophil — mit den Grasskonstanten läge eine Buchenkrone im Zenit 17,3 % zu hoch.

**Der Fund der Runde ist `spread_m`.** Beide Meterfelder standen in allen 16 Dateien und wurden vom
Prototyp NIE gelesen, also nie geprüft. Gemessen: gewachsene Kronenbreite gegen `spread_m/height_m`
zwischen **0,66× und 4,56×**, Mittel 1,44. `fichte` deklariert 35 m × 6 m und wächst 27 m breit.
`spread_m` ist heute kein Parameter, sondern ein Kommentar.

Kein Rendern, keine Platzierung, kein LOD, kein Wind — genau ein Netz aus genau einer Deklaration.
`verify-layers` grün (148 Dateien), `verify-trees` unverändert bei 9 Waisen (kein neues Verzeichnis).

## 2026-08-07 — Die schwarzen Wände sind kein Ambient-Fehler, sondern ein Loch statt eines Nachbarn

**Die Hypothese des Eigners ist widerlegt, und zwar arithmetisch.** Die Halbkugelkorrektur, die den
Bestandsambient bei `n·up = −1` von 0,120 auf 0,738 hob, saß NIE im gemeinsamen Header. Sie ist der
zweite `litRadiance`-Aufruf in `Sward.h` mit `−nAgg` und `kLeafTrans` 0,85 — durch die Blattspreite
durchgelassener Himmel. Die Tabelle folgt exakt aus `nachher(n) = vorher(n) + 0,85·vorher(−n)` mit
Blattalbedo 0,22: `0,120 + 0,85·0,727 = 0,738`, `0,727 + 0,85·0,120 = 0,829`, Mitte `0,4235·1,85 =
0,784`. Eine opake Mauer hat keinen durchgelassenen Himmel; Gebäude hatten die Korrektur nie und
dürfen sie nicht bekommen. `(1 + n·up)/2` ist für eine isotrope Kuppel korrekt und bleibt.

**Gemessen, was die abgewandte Wand wirklich bekommt** (Referenzszene, Kamera 210 m vor einem
freistehenden Hof, Sonne 11,20°, `FB_TONE_PROBE=-14,2` als Lineal, 1188 px reine Wand): ohne AO
`log2 L = −4,038`, flach über 0,31 EV. Die physikalische Schranke einer senkrechten Fläche —
`0,5·E_Himmel,h + 0,5·0,12·(E_Himmel,h + E_Sonne,h)` = 0,0548 gegen geliefert 0,0444 — liegt **0,303 EV**
höher, und diese Lücke ist vollständig `kSelfShelter`. 0,3 EV sind nicht schwarz.

**Schwarz macht die Verdeckung.** `FB_GEOM`-Paar auf einer sonnenabgewandten Wand isoliert genau das
AO (der Direktterm ist dort ohnehin null): AO-Faktor Median **0,730**, p10 **0,361**, Minimum **0,226**
auf einer Wand, bei der nichts im Umkreis von 0,9 m steht. Auflösungsreihe bei identischer Kamera —
Scheibenradius **2,67 px → 0,662**, **5,34 px → 0,861**, **8,02 px → 0,885**: kein Rauschen, sondern
ein auflösungsabhängiger Bias mit fester Moiréstruktur. `kAoMinPx` = 2,5 lässt genau das Band durch,
in dem die Schätzung keine Geometrie mehr trägt.

**Repariert wurde nicht der Schätzer, sondern was aus seinem Fehler wird.** Ein Verdecker ist eine
Fläche, kein Loch: was er dem Himmel nimmt, gibt er zu `kGroundBounce` zurück. Eine Zeile in
`SurfaceLight.h` — `alpha = 1 − ambFrac·(1 − kGroundBounce)` — und das Compositing bleibt ein `mix()`,
die Zahl bleibt an einem Ort. Wandmaske 9 488 px: Pixel unter Code 64 **1 548 (16,32 %) → 135 (1,42 %)**,
p01 43 → **63**, p50 101 → 104, p90 118 unverändert; HDR p01 −6,249 → **−5,773** (+0,476 EV). Himmel
und Wolken (Zeilen 0–375) **bitgleich**, Belichtungsanker unverändert, Frame-Mittel 146,084 → 146,226.

**Nicht behoben und ausdrücklich nicht so gemeldet:** die graue Sprenkelung bleibt. `AoStage.cpp` lag
während dieser Runde in der Hand eines anderen Agenten; verifiziert wurde in einem privaten Baum mit
byteidentischer `SurfaceLight.h` (`make walk` und `make wasm` grün, `verify-layers` grün, zwei Läufe
md5-gleich).

## 2026-08-07 — Schwarze Schatten: der AO-Puffer trug eine Diagnosezeile, zum zweiten Mal

Eignerbefund: *„gebäudeschatten sind momentan komplett schwarz"*. Er hat recht und die Zahl sagt, wie
weit: eine beschattete Fassade in Hamelns Altstadt las `log2 L = −7,330` (sRGB 13,13,13 — **neutral**
unter blauem Himmel, und genau das ist der Verräter, denn Himmelslicht ist hier 0,030/0,051/0,089).

**Die Ursache ist eine Zeile, und es ist dieselbe Sorte wie letzte Runde im Tonemap.** `AoStage.cpp`
endete auf

```wgsl
return vec4f(ao * 1.0e-9 + select(0.0, 1.0, orient > 0.0), abs(orient), 0.0, 1.0);
```

`orient = dot(nrmA, p0)`, und `nrmA` wird zwei Zeilen darüber so gewählt, dass dieses Produkt **nie**
positiv ist. Der Rotkanal — der einzige, den das Compositing liest — war also **0 auf jedem Pixel, auf
dem der Pass lief**; das `1.0e-9` hielt bloß den Compiler davon ab, das gerade berechnete AO
wegzuoptimieren. `lit = scene.rgb · mix(ao, 1, alpha)` mit `alpha = 0,12` für eine rein
himmelsbeleuchtete Fläche ist **−3,06 EV**.

Gemessen mit dem Tonemap als Lineal (`FB_TONE_PROBE=-16,4`, Exponent 1, kein Fuß, ein PNG-Byte ist
`log2 L`): Fassade **−7,330 → −4,193**, also **+3,137 EV** gegen die 3,06 EV, die allein das
Compositing vorhersagt. In der Referenzszene selbst trifft es nur die ferne Stadt: **67 von 691 200
Pixeln** über 20 Codes, alle im Gebäudeband bei y ≈ 358–362, von 44 auf 125.

**Der Sonne-Schatten-Kontrast am Boden ist jetzt messbar und beträgt 1,31 EV.** Profil senkrecht über
eine Gebäudeschattenkante auf **einem** Belag, Plateaus ≥ 1 m vom Wandfuß: besonnt **−3,79 ± 0,09**,
beschattet **−5,10 ± 0,05**, Übergang **ein** 10-px-Schritt, keine Rampe. Der Farbton kippt mit dem
Illuminanten — (220,202,188) gegen (183,195,218).

**Die 1,74 EV der Abnahme sind nicht die Zahl dieser Szene, und der Unterschied ist hergeleitet.**
1,74 EV = `totalHorizY/skyDiffuseHorizY` ist das **klare** Verhältnis; die Szene deklariert
`cloudCover 0.55`. Löst man die Gleichung aus `doc/render/lighting.md` §2 nach der Strahltransmittanz
auf, die 1,31 EV erzeugt, kommt **τ = 0,620** heraus — plausibel für 0,55 Bedeckung. Dieselbe
Gleichung bei τ = 1 gibt **2,18 EV**, und bei τ = 1 **mit `kSelfShelter` = 0** gibt sie **1,743 EV**,
also die Abnahmezahl auf drei Stellen. Die Latte ist damit die klare, ungeschützte Schranke genau
dieser Gleichung, und das gebaute Modell liegt **0,44 EV** darüber — vollständig `kSelfShelter`.
`kSelfShelter` wurde **nicht** angefasst: eine physikalische Konstante zu verstellen, damit eine
Abnahmezahl passt, ist Fälschung, und die vorige Runde hat den Term auf einer Wand bereits mit
−0,303 EV vermessen.

**Kein Pixel liegt auf Code 0** — in drei Frames, 691 200 Pixel je Frame, Minimum 8/255 auf einem
Dachstuhl tief im Hof (`mix(kAoFloor, 1, 0.12)` = 0,34). Frame-Zeit gegen ein in derselben Sitzung
gebautes Basis-Binary, `walkbench.py`, p99 über vier Geschwindigkeiten: **18,29 / 19,76 / 19,69 /
20,16** gegen **19,37 / 20,79 / 20,43 / 20,30** ms, 0,00 % über 2,5 Vsync-Perioden in allen acht
Läufen. Der Shader hat zwei Operationen verloren.

**Was offen bleibt:** die beiden AO-Messungen der Vorrunde (Auflösungsbias 0,662/0,861/0,885, Median
0,730) wurden **durch eine Stufe hindurch genommen, die 0 zurückgab** — es sind Rekonstruktionen aus
einem `FB_GEOM`-Paar, keine Ablesungen des Puffers, den das Compositing benutzt hat. Sie sind zu
wiederholen. Und: zweimal in zwei Runden hat eine Debugzeile im einzigen gelesenen Kanal wochenlang
überlebt, weil das Bild **plausibel** blieb. Ein Shader, dessen Ausgabe nie gegen eine gerechnete
Erwartung gehalten wird, hat kein Tor.

## 2026-08-07 — Die L-Taste stand schon; was fehlte, war der Beweis und der Pfad im Dokument

Der Standpunktlog war gebaut (`AppWasm.cpp` `PostShot`, `Snapshot.h`, `SimHost.cpp` `POST /shot/*`,
`gpu_walk --snapshot`) und **nirgends nachgewiesen**. Ein Mechanismus ohne Messung ist eine Behauptung,
also die Kette einmal ganz durchgezogen:

Headless Chromium auf die ausgelieferte App, 75 s einschwingen, ein `L`:
`{"ev":"shot_posted","pngBytes":759942,"http":200}`, eine Zeile an `sim/shots/shots.jsonl`.
`gpu_walk --size 1280x720 --snapshot shots/shot-20260807T111409Z-001.json` stellt sie nach und
**subtrahiert die abgeleiteten Größen selbst**: `dGroundM −3,133e−05 m`, `dSunElDeg −3,549e−05°`,
`dSunAzDeg −4,458e−05°`. Das ist der Teil, den ein Pixelvergleich nie zuordnen könnte.

**Die Abweichung ist eine Zahl:** 1280 × 720, **89,75 % bitgleich**, **99,44 % innerhalb von 2 Codes**,
mittleres |Δ| 0,188, p99 = 1, Maximum 61. Jedes Pixel über 8 Codes — **2 087, also 0,226 %** — liegt im
51 Zeilen hohen Band `y 330…381`: die ferne Stadt am Horizont, der Streaming-Schnitt der beiden
Clients, nicht der Standpunkt.

**Der feste Pfad steht jetzt in `doc/clients/clients.md`** — `sim/shots/shots.jsonl` auf dem Host,
`SHOT_ROOT=shots/` im Container, von `up.sh` gemountet. Die Zeile ist selbsttragend: `camera`,
die `scene`-Identität, gegen die `Matches()` prüft, und ein `derived`-Block, der nur da ist, um
abgezogen zu werden. PNG zuerst, dann die Zeile, die es benennt.

**Der Browser trägt die AO-Reparatur mit**: dasselbe Bild, dieselben grauen Fernhäuser statt schwarzer.

## 2026-08-07 — Die Klassifizierung: gelesen, gerechnet, entschieden — und nicht gebaut

Dritte Aufgabe der Runde, und sie bleibt offen. Was fertig ist, ist alles, was **vor** der ersten
Codezeile hätte stehen müssen, und eine Messung, die nur **jetzt** möglich war.

**Die beiden gemeldeten Sperren gab es nicht mehr.** `OsmVector::Str` und `OsmField::Str` lesen
String-Tags; `OsmField` ist der gemeinsame Vektorspeicher in Geodätik über Kachelnähte, und er
deklariert bereits `{"buildings", "land"}` — `land` hatte nur keinen Leser.

**Der Kachelserver wurde gelesen, wie angewiesen, und keine seiner drei Antworten ist die erwartete.**
`w3_landcolor` bildet 60 `kind` auf **RGB** ab, nicht auf Klassen — die Aufzählung ist wertvoll, die
Abbildung ist eine Palette. Die Prioritätsordnung **zwischen** Ebenen ist deklariert und wird
übernommen (`ocean` → `land` → `water_polygons` → `sites` → `street_polygons` → `buildings`, dann
Linien); **innerhalb** von `land` gibt es keine — es ist die Emissionsreihenfolge des Anbieters, der
letzte gewinnt, bei 2,41 % Überlappung. Das ist der Fund: implizit heißt nicht deterministisch.
`w3_roadstyle` liefert **Texel an einer 1024er Referenz**, keine Meter, und wird **nicht** übernommen.

**Der strukturelle Blocker, und er macht daraus eine eigene Runde.** `World::kMaxZ` = 14. Das feinste
Kachelklassenraster, das dieser Build irgendwo auf der Erde erzeugen kann, ist `SpanM(14)/TS` =
1502,33/512 = **2,9342 m je Texel**, per Konstruktion. Die Vektoren in dasselbe Array zu rastern würde
die Semantik reparieren und **jede Abnahmezahl unverändert lassen**. Das Nahfeld braucht einen
weltverankerten Cache neben dem Quadtree, mit **Gewichten statt Index**.

**Die Vorher-Messungen, und sie sind nach der Bake-Löschung nicht mehr nehmbar:**

| | gemessen | hergeleitet |
|---|---|---|
| gerade OSM-`land`-Grenze im Klassenraster | **RMS 1,192 m** (12 Segmente ≥ 117 m, je 40 Proben); 0,955 m über die 11 unkreuzten; schlechtestes Einzelresiduum 14,5 m | Quantisierung auf ±½ Texel ist gleichverteilt, RMS = `Texel/(2√3)` = **0,847 m**. 13 % Abweichung |
| Wege im 3×3-z14-Block um den Standpunkt | **144 von 206 Straßen-Features (69,9 %), 86,4 km, erreichen das Klassenraster gar nicht** | **und das ist der Client, nicht der Bake.** `fb_lod_line_ok` sperrt bei `lod_ts < 1024`, `bake_native` reicht das angeforderte `tex` als `lod_ts` durch, und beide Clients fragen `tex=512` (`AppWalk.cpp:402`, `AppWasm.cpp:331`). Bei `tex=1024` zeichnen alle 144 |
| Wohnstraße, gezeichnete Breite | **3,52 m** bei realen 5,5–6,0 m | `w3_roadstyle` skaliert MIT `tex` — die Meterbreite ist auflösungsinvariant: 3,52 m bei 512, 1024, 2048 und 4096 |
| Bach und Fluss | **4,40 m, beide gleich** | eine Strichbreite für 1–3 m und für 10–40 m, bei jeder Auflösung |
| Klassen-VRAM gegen `tex` | 32,5 / 130 / 520 / **2 081 MB** bei 512 / 1024 / 2048 / 4096 über 124 Kacheln | `tex²` × 1 B × 124 |
| bilineare Stützweite des Bodenshaders | ±2,934 m — schmaler als **5,868 m** kann nichts grasfrei sein | |

**Die Deklaration gehört in `vegetation.json` und ein drittes File wäre ein Defekt.** Jede der neun
Vorlagen trägt schon eine Schlüsselliste — `keySrgb`, also **die Farbe, die der Server gewählt hat**.
Genau diese Kopplung ist zu schneiden: der Schlüssel gehört an das Tag. Eine weitere OSM-Ebene ist
dann eine Zeile, weil `layer` ein String ist, den `OsmField::Layer` ohnehin auflöst.

**Drei Ausgänge, drei Behandlungen:** kein OSM-Datum am Ort (14,48 % der Referenzkachel) → deklarierte
Vorgabe und ein **Zähler**; Tag da, Tabelle kennt es nicht → **`Log::Error` mit dem Tag**; Kachel
fehlt oder Parse scheitert → **`Log::Error` mit dem Ort**.

**Eine eigene Korrektur, weil eine falsche Zahl in `doc/` schlimmer ist als keine.** Die erste Fassung
dieses Eintrags las die 144 fehlenden Wege als Eigenschaft des Bakes. Sie sind eine Eigenschaft von
`tex=512`, einer `[SET]`-Konstante in beiden Clients, die nichts misst. Der Bake hat die Wege; wir
fragen sie nicht ab. Was `tex` **nicht** kauft, ist Breite — `w3_roadstyle` skaliert mit, also bleibt
die Wohnstraße bei jeder Auflösung 3,52 m breit. Auflösung kauft Anwesenheit und Kantenschärfe
(RMS fällt mit `Texel/(2√3)` auf 0,423 / 0,212 / 0,106 m) und kostet `tex²` VRAM.

**Nicht gebaut und nicht als gebaut gemeldet.** Der Bake bleibt serverseitig ohnehin bestehen
(Eignerentscheid) — hier bleibt er auch clientseitig, weil `vegetation.json` heute über `keySrgb`
ausschließlich an ihm hängt.

## 2026-08-07 — Ein Baum, mit Höhe, und die Genauigkeit kommt aus der Darstellung

Entscheidung des Eigners, in vier Schritten gereift und am Ende einfacher als der Anfang:

**EIN räumlicher Index, kein Quadtree neben einem Oktree.** Zwei Indizes wären zwei Wahrheiten — dieselbe
Fehlerklasse, die diese Sitzung dreimal gekostet hat (zwei Klassenpfade, zwei DEM-Abtastungen, zwei
Klassenmodelle).

**Quadtree mit Höhenausdehnung je Knoten**, vertikal geteilt nur dort, wo der Inhalt es verlangt. Der
Eigner nannte es zuerst „Oktree-Schale, N km dick, in Sektoren geteilt" — das ist dasselbe, bis auf die
Frage, ob die Höhe mitteilt oder nur begrenzt. Begründung für die Säule: Gelände, Gebäude, Vegetation und
Bauwerke kleben sämtlich an der Oberfläche; ein Flugzeug in 10 km liegt in der SÄULE seiner Zelle, und
ein Frustum schneidet Säulen so zuverlässig wie Würfel. Ein Oktree passt schlecht auf eine Kugel
(Würfelkugel oder entartete Zellen) und die Schale ist zu über 99 % Luft. **Geteilt wird, wenn es sich
rechnet, nicht weil die Struktur es vorsieht** — dieselbe Regel wie beim LOD, wo die einzige deklarierte
Zahl das Budget ist.

**Die Genauigkeit kommt NICHT aus der Struktur.** `float64` ECEF löst am Erdradius rund einen Nanometer
auf; Millimeter sind millionenfach übererfüllt. Zellursprünge braucht es dafür nicht. Der Renderer
rechnet bereits kamerarelativ — die Regel wird allgemein: **`float64` ist die Wahrheit, die Umrechnung auf
kamerarelatives `float32` passiert EINMAL und SPÄT, an einer benannten Stelle.** Jeder Zwischenschritt,
der absolut in `float32` rechnet, verliert die Genauigkeit, bevor die Umrechnung sie retten könnte — und
man sieht es nicht, weil das Ergebnis um einen halben Meter danebenliegt und plausibel aussieht. Genau
diese Klasse hat heute zweimal zugeschlagen.

**Der Baum ist ein abgeleiteter INDEX, kein Speicher.** Das ECS hält die Wahrheit (Position als
Komponente in `float64`), der Baum beantwortet Wo-Fragen und **darf nie eine Position besitzen, die das
ECS nicht auch hat.** Gelände ist dabei keine Entität — Kachelgeometrie wird gestreamt, nicht simuliert.
Für einen Index ist das gleichgültig, für einen Speicher wäre es falsch. Reihenfolge: erst der Baum, dann
das ECS darauf; der Baum trägt schon heute Kacheln, ein ECS ohne Index wäre sofort langsam.

**Was mit dem Umbau von selbst verschwindet**, statt dokumentiert zu werden: `kMaxZ` als geteilte Grenze
für drei Datenarten mit verschiedenen Maxzooms (Terrain 15, Vektoren 14, Luftbild 19), gesetzt auf die
schwächste und **ohne Herkunft**; die Aufteilungsregel mit ihrer Begründung „gleiche Albedo-Auflösung bei
gleicher Entfernung", deren Albedo gerade gelöscht wird; `kNodeCeil` und `kGrace`.

**Der Kachelserver spricht weiter Slippy.** `z/x/y` bleibt die Adressierung der Quelle; was fällt, ist der
Quadtree als Index der Engine.

**Heute existiert kein ECS.** `units/` hält 214 Zeilen — `Unit.h` mit `UnitPose`, `UnitArticulation`,
`UnitSignature` und einem virtuellen `Run(dt, units, world)`, konstruiert von nichts. Ein Rest der
Kampfschicht.

**Und der Grund für JETZT:** es ist noch nichts platziert. Bäume, Stauden, Windrad und Entitäten kommen
alle erst. Was nach der Umstellung platziert wird, sitzt richtig.

Offen und Voraussetzung: das Höhenorakel widerspricht dem gezeichneten Netz um **0,383 m RMS, max
1,89 m**, weil `ChunkBuildEcef` 256² auf 33² Stützstellen dezimiert (46,9 m bei z14). Ein Baum stünde
damit bis zu 1,89 m falsch. Behebung benannt und ungebaut: das Orakel wertet die GEZEICHNETE Fläche aus
statt das DEM ein zweites Mal.

## 2026-08-07 — Die Klassifizierung hat kein Raster mehr

**Gemessen, bevor gebaut wurde, und die Messung hat den Bau umgeworfen.** Der Auftrag war eine
weltverankerte Clipmap aus Deckungsgewichten. Die Frage „können shader nicht direkt mit den osm
vektordaten rechnen?" wurde vor der Fertigstellung gestellt und ist mit den echten Daten entschieden:
über dem 3×3-z14-Block am Referenzstandpunkt (20,3 km², 2246 Flächenmerkmale, 25501 Kanten nach dem
Verbreitern jeder Linie auf ihre erklärte Meterbreite) sieht ein Fragment bei 16-m-Beschleunigungszelle
im Mittel **3,87 Kanten und 2,11 Merkmale**, p99 16 Kanten, schlimmste Zelle 45 Kanten und 14 Merkmale.
Der Puffer wiegt **1,8 MB** gegen 32,5 MB Klassenraster. Damit war die Clipmap eine Optimierung ohne
Messung dahinter, und sie ist weggefallen statt fertiggebaut worden.

**Was das strukturell erledigt:** ein Raster hat eine Auflösung, und die Auflösung war dreimal die
Ursache dafür, dass die Klasse von der Kamera abhing (Mipstufe, Kachelzoom, Abtastgitter des
Verbrauchers). Ohne Raster ist die Fehlerklasse nicht mehr ausdrückbar, nicht mehr nur verboten.

**Vier Abnahmezahlen, vorher → nachher:** gerade Nutzungsgrenze RMS **1,192 → 0,180 m** (0,955 → 0,069 m
ohne kreuzendes zweites Merkmal), Wohnstraße **3,52 → 5,50 m** bei erklärten 5,5, Bach gegen Fluss
**4,40/4,40 → 1,98/11,98 m**, Grasfreiheit von „nichts unter 5,868 m" auf eine Ausfransung von
**max(0,05 m, footM)**. CPU gegen GPU: **100,0000 %** über 448837 Bodenpixel. Weltfestigkeit: zwei
Läufe, Kamera 600 m auseinander, Gitter neu verankert, **0 von 11496000** Abweichungen.

**Der Fund, der dreißig Stunden gekostet hätte:** `OsmVector` gab für JEDE Linie eine leere Geometrie
zurück — Ringe wurden nur bei ClosePath ausgegeben, und ein LINESTRING sendet keins. 206 Straßen und 12
Wasserläufe je Block kamen als `RingCount` 0 an, ohne Fehler und ohne Zähler. Genau der Defekt, gegen
den die drei getrennten Fehlerbehandlungen dieser Runde gebaut wurden, im eigenen Dekoder.

## 2026-08-07 — Der Wald steht verteilt, und die Höhe ist eine Eigenschaft der Art

Die Wand aus Stämmen hatte zwei Ursachen, keine davon im Vertexformat: `TreeField::Scatter` schrieb die
absolute ENU-Koordinate in den Instanzpuffer, während `vsBark` den Stand auf die ECEF-Achsen am Augpunkt
legt, und `eyeAsl` bekam den Boden unter der Kamera statt den Augpunkt. Gemessen mit
`build/gpu_walk` md5 `745f1d92`: `damuels` 219 240 Stände, east −888,2/+898,2 m, north −898,2/+898,2 m,
Fuß über Auge −111,9/+417,3 m gegen ein `/elev`-Raster von −94,6/+362,7 m; `koenigssee` 179 181 Stände,
−898,3/+898,2 m, −898,2/+898,2 m, −19,5/+489,1 m.

Fünfter Instanzwert: der Größenfaktor aus `species.height_sigma`, dreieckig gezogen und bei
mu ± 2,4494897 sigma hart begrenzt, eigener Hashwurf gegen die Kopplung an die Gierung.
`buche.height_sigma = 0.066` ist hergeleitet aus hg/h100 = 0,94 bei N = 250/ha:
sigma/hg = (1/0,94 − 1) / (phi(Phi^-1(0,6))/0,4) = 0,0661. Gemessen 0,8385…1,1614 bei Mittel 0,99991
gegen hergeleitet 0,83833…1,16167, also 25,2…34,8 m.

Auf `build/out/koenigssee.png` stehen die Bäume einzeln, verdecken sich und tragen eine zackige Kante
gegen Himmel und Fels. Was fehlt und in `doc/render/renderer.md` `## Gaps` steht: der Feldpfad zeichnet
kein Blatt, der Stammdurchmesser ist mit 3,1 m bei 30 m Höhe rund viermal zu groß, die Dichte ist mit
862 Stämmen/ha drei- bis vierfach zu hoch, und fünfzehn Arten deklarieren kein `height_sigma`.
