# mig23 — offene Defekte

> Der Startpunkt der nächsten Runde. `doc/assets.md` §1: *„The critic's open defects survive"*.
>
> **Runde 2.** Was Runde 1 an Toren grün meldete, war teilweise unbelegt: elf Maße wurden an den
> Netzen VOR dem Parenting gemessen, während sechs Klappenkörper in der ausgelieferten Datei bis
> 3,3 m neben dem Flugzeug standen. Die Regel heißt seither **miss an der ausgelieferten Szene**
> — `check_norms`, `check_overlap`, das Silhouettentor und die Umschalttabelle lesen das fertige
> `.glb` zurück (eigener glTF-Leser, kein `bpy`-Reimport), und `check_placement` vergleicht jede
> Knotenweltmatrix und jede Körperpunktwolke gegen `node_table()` bzw. das gebaute Netz.
>
> Stand: 54/54/51/51 geschlossene Körper je Stufe, null ungewollte Durchdringungen, 11 Maße am
> **ausgelieferten `.glb`** gegen [PUB]/[BP], Silhouette 0,944 % + 0,20 pp gegen 2 % über 361
> Azimute, zweimal gebaut → bytegleich, 56–58 s.

---

## A — was Runde 2 geschlossen hat (mit Zahl vorher/nachher)

| War | Ist | Beleg |
|---|---|---|
| sechs Klappenknoten trugen `m_par @ m` statt `inv(m_par) @ m`; Netto-Weltmatrix der Netze `m_par²`, Versatz bis 3,3 m, z = −1,936 | Weltmatrix jedes der 18 Knoten stimmt auf **0,000 m / 0,016°**, jede der 54 Körperboxen auf **3·10⁻⁶ m** | `check_placement`, Sidecar `lods[].placement` |
| Prüfung las `me.v` vor dem Parenting | Prüfung liest das `.glb` | `read_glb` + `check_placement` |
| HK-Wurzelpunkt (2346, 615) lag 127 px innerhalb des Zapfens in leerer Fläche; HK-Pfeilung 11,66° | HK-Pfeilung **2,397°** aus 202 Punkten, Residuum 0,57 px über 1005 Zeilen; VK 18,753° aus 114 Punkten, Residuum 0,48 px (Literatur 18°45′) | `kPanelTeSweep`, `kPanelLeSweep` |
| Flügeltiefe bei u = 0,42 m: 1,771 m | **2,557 m** (+44 %); freitragende Paneelfläche 14,7 → **18,2 m²** | `kPanelRootChord` |
| Bugbein +3,35 m, Hauptbein −2,42 m, Radstand 5,77 m (alle `[SET]`) | **+5,505 / −0,648 / 6,153 m** aus dem ML-Seitenriss, kreisgefittete Räder (Residuum 0,93/0,95 px), Übertragung über Radomspitze + Flossenspitze | `kNoseGearPx`, `kMainGearPx` |
| Kalibrierung ruhte auf der Anpassung an beide Spannweiten, deren gespreizte Marke 36 px falsch lag | **Maßstabsbalken**, 1006,0 px für 5 m. Probe: Zellenlänge 16,685 m gegen [PUB] 16,7 → **−0,09 %** | `kPxM` |
| `kPxViewSum = 3797` (Marken gaben 3796/3794) | **3799** als Mittel zweier nachgemessener Marken (Pitotspitze 3792, Düsenaustrittsebene 3806) | `kPxViewSum` |
| Zapfen bei 304 px, aus zwei behaupteten Kreislagen | **309,75 px**, Mittel zweier KREISGEFITTETER Symbole (293,4 / 326,1 px, Residuen 0,99/1,10 px) | `kPivotXPx` |
| Höhenleitwerk stand 99 px = **0,49 m zu weit achtern** (VK bei Zeile 952 abgelesen, bei Zeile 900 eingesetzt) | beide Kanten gefittet, Wurzel-VK 2956,3 statt 3055 | `kTailRootLePx` |
| Randbogensehne 0,55 m `[SET]`, mittig um die Spitze gelegt | **0,294 m [BP]**, von der Spitzen-VK nach achtern | `kTailTipChord` |
| Bremsschirmverkleidung lief als Rohr nach VORN; Flugzeug endete 0,16 m zu früh | flache Verkleidung nach ACHTERN auf den gemessenen hintersten Zellenpunkt | `kChuteTipY` |
| `kSilSupShortfallPp = 0,03` „obere Schranke" | **0,20**, gemessene Reihe 361/1447/2887 → 0,9440/1,0309/1,0799 %, geometrische Fortsetzung +0,199 pp. Das Wort Schranke ist weg | `--refine-sil` |
| Alias-Begründung nannte 64/40/24/14 | 72/48/32/22 — die Segmentzahlen DIESES Assets | `_views` |
| `kLodSegments` unbenutzt | gelöscht | — |
| Netznamen kollidierten mit Knotennamen → Exporteur benannte sie in `.001` um | Netze heißen `<knoten>.skin` | — |

---

## B — sichtbar falsch (was das nächste Bild verbessert)

### 1. Die Kanzel liest sich nicht als Kanzel

Die Haube steht 0,32 m über dem Deck, die zwei Bügel sind sichtbar — **die Verglasung selbst ist
es kaum**. Im Dreiviertelbild von Runde 2 ist die Kanzel nur als schwacher Buckel zu erkennen.
Das Material wurde in Runde 1 von α 0,20 auf 0,55 gezogen; das ist ein Kompromiss, kein Beleg.

Offen: die Haubenkontur ist `[SET]` (fünf Stationen, `kCanopy*Px`), es fehlen Spriegel,
Rückspiegel, Schleudersitz und ein Cockpitboden. Ein Blick von vorn/oben zeigt eine leere Wanne.

### 2. Das Fahrwerk ist eine Skizze

Die Stationen sind jetzt gemessen (s. A), die KINEMATIK nicht. Die MiG-23 hat ein sehr
eigenwilliges Hauptfahrwerk (Zug-Druck-Kinematik mit zwei Streben und querliegendem Radträger).
Gebaut sind zwei Rundstäbe, ein Rad, eine Klappe und ein Schleppstab; im Seitenbild lesen sie
sich als Striche. Alle Einzugswinkel (`gear.nose` 95°, `gear.main.*` 88°) sind `[SET]`.

### 3. Einlauf und Rumpf stoßen mit einer Kante aneinander

Der Einlaufkasten ist ein sauberer Hohlkörper mit 1,35 m tiefem Kanal, aber er **verschneidet
sich nicht** mit der Rumpfseite, sondern steht als Quader daneben. Auf der echten MiG-23 läuft
die Einlaufaußenwand in den Rumpf über und die Grenzschichtplatte sitzt in einer Nut.

### 4. Zwei Durchbrüche von je 0,136 m² in der Draufsicht

Gemessen am gerenderten Alpha der Draufsicht (2000 px, 9,77 mm/px, Flutfüllung vom Bildrand):
zwei eingeschlossene Hintergrundgebiete von je **1426 px = 0,136 m²**, x = ±0,93 m,
y = −0,01 … +2,92 m — also ein 0,088 × 2,93 m langer Schlitz zwischen Einlaufaußenwand und
Rumpf. Das IST die Grenzschichtspalte (`kInletRampGapPx`), und am Original ist sie oben offen —
aber nicht auf 2,9 m Länge bis zum Boden durch. Runde-1-Kritik nannte 0,022 m² am Hauptfahrwerk;
diese sind mit der neuen Fahrwerks- und Pylonlage **verschwunden**.

### 5. Bei 72° klafft die Wurzelfuge zwischen Paneel und Handschuh

**Richtiggestellt gegenüber Runde 1:** die Lochfläche in der Draufsicht ist bei 56° Zapfendrehung
**identisch zu 16°** (dieselben zwei Schlitze aus #4, 0,136 m² je Seite) — das Paneel gleitet
unter den Handschuh, es entsteht KEIN zusätzlicher Lichtdurchlass. Was bleibt, ist eine **Kerbe
im Umriss**: die Paneelwurzel wandert beim Pfeilen nach innen und achtern, die Handschuhkante
bleibt stehen. Deshalb steht dieser Punkt jetzt hier unten und nicht mehr an erster Stelle.
Nachfolger: ein Körper `glove.seal.<side>` als Kreisringsegment um den Zapfen, Öffnungswinkel 56°.

### 6. Bauchflosse und Bremsklappen sind Platten

`ctl.ventral` ist ein 0,10 m dickes Prisma mit fünf Eckpunkten, die Bremsklappen sind Quader.
Die Bauchflosse hat am Original ein Profil und einen Knick; `kAirbrakeY` ist `[SET]`.

---

## C — Zahlen ohne Beleg (jede [SET]-Zahl dieses Assets)

| # | Konstante | Was fehlt |
|---|---|---|
| 7 | `kPivotZ = 0,16 m` | Höhe des Drehzapfens über der Triebwerksachse. Der Riss zeigt sie nur in einem Schnitt, den ich nicht kalibrieren konnte |
| 8 | `kPanelRootU = 0,42 m` | Trennfuge Paneel/Handschuh |
| 9 | `kPanelDihedralDeg = −1,0°` | V-Stellung des Paneels. Keine Quelle nennt sie |
| 10 | `kSlatChordFrac 0,16 · kFlapChordFrac 0,26 · kSpoilerChordFrac 0,14` und alle Ausschläge (20°/25°/45°) | **Belegt ist nur, DASS es Vorflügel, Landeklappen und Störklappen gibt** [WEB] |
| 11 | `kGloveTeY = −1,60 m`, `kGloveSawtoothU/Depth` | Der MLD-Sägezahn ist [WEB] belegt, seine Größe nicht. Gestützt: die gemessene Paneel-HK trifft an der Zapfenstation −1,608 m, also 8 mm neben `kGloveTeY` |
| 12 | `kTailAnhedralDeg = −10°`, `kTailHingeFrac = 0,28`, `kTailThk*` | Anhedral gestützt (Seitenrissprojektion 80 px über 2,5 m → 9,1°), aber nicht bemaßt. **Der Riss ist achtern verzogen:** Leitwerksspitzen 590/539 px beidseits, Zapfensymbole 293,4/326,1 px, Rumpfmitte vorn 792 gegen achtern 817 |
| 13 | alle Fahrwerks-Einzugswinkel | s. B#2 |
| 14 | `kNozzleThroatDia 0,86 m`, `kNozzlePetals 24`, `kNozzleDepth 0,55 m` | nur der Austrittsdurchmesser (1,245 m) ist [BP] |
| 15 | Profilform | Die **Dicke** ist [PUB] (TsAGI SR-12S, 6,5 % / 5,5 %), die **Form** ist eine symmetrische NACA-Vierziffernverteilung. SR-12S-Koordinaten sind nicht öffentlich |
| 16 | `kSilResCap = 1024 px` | Kostenentscheidung, keine Herleitung |
| 17 | `kRudderMaxDeg 25°`, `kRudderChordFrac 0,26`, `kAirbrakeY`, `kAirbrakeMaxDeg` | |
| 18 | Alle sieben Materialien | Ein Anstrich hat keine Norm |
| 19 | Rumpfoberkante 2400…2800 px, Formexponenten `kFusShapeKnots` | Exponenten aus den Schnitten А-А/Б-Б/В-В/Г-Г qualitativ abgelesen, nicht bemaßt |
| 20 | Rumpfhalbbreiten achtern von 1400 px | Der Grundriss ist dort verdeckt; fortgeschrieben |
| 21 | `kPylonGloveX/Y`, `kPylonFusX/Y`, `kGunY`, `kIrstY` | Alle Außenlaststationen sind gesetzt. `kPylonGloveY` und `kIrstY` sind in Runde 2 aus einem MESSBAREN Grund verschoben worden (Kollision mit dem jetzt gemessenen Fahrwerk), nicht aus einer Quelle |

---

## D — belegte Widersprüche, die stehen bleiben

### 22. Der gezeichnete Flügel ist 1,3 % größer als der veröffentlichte

**Der Riss ist mit sich selbst konsistent** — das ist nachgemessen, nicht angenommen. Jede
Hälfte in ihrem EIGENEN Zapfensymbol gemessen:

| | gespreizte Hälfte | gepfeilte Hälfte | Differenz |
|---|---|---|---|
| Abstand Zapfen → VK-Gerade | 209,4 px | 208,3 px | **1,1 px** |
| Winkel der VK-Geraden | 18,753° | 74,323° | **55,570°** (nominal 56) |
| Zapfen + Randbogenabstand gegen [PUB] | +1,30 % | +1,44 % | — |

Beide Stellungen sind also **gleichmäßig 1,3–1,4 % zu groß** gezeichnet, und die 56°-Drehung
zwischen ihnen stimmt auf 0,43°. Der Fehler steckt nicht in einer der beiden Stellungen, sondern
in der Panelgröße insgesamt. Deshalb: **Form aus dem Riss, Spannweite aus [PUB]** — `_solve_panel`
erzwingt beide veröffentlichten Spannweiten und schrumpft die Spitze dabei um 1,64 % gegen den
Riss (5,443 statt 5,534 m ab Zapfen, θ₀ 8,74° statt 8,02°).

Von der Mittellinie statt vom Zapfen gemessen fällt die Abweichung auseinander (+2,46 % gespreizt,
−0,65 % gepfeilt) — das ist der Heckverzug aus #12, nicht der Flügel.

### 23. Die beiden Ansichten haben verschiedene Maßstäbe

Pitotspitze bis Düsenaustrittsebene: Seitenriss 3435,5 px, Grundriss 3449,5 px — der Grundriss
ist **0,41 % größer** gezeichnet. `kPxViewSum` ist deshalb ein Mittel, kein exakter Wert; die
Streuung von ±7 px = ±35 mm trifft jede Größe, die eine Seitenrissstation gegen eine
Grundrissstation stellt.

### 24. Der Maßstabsbalken ist um 0,5–0,7 % zu kurz

Balken 4,97018 mm/px. Die gepfeilte Spannweite verlangt 5,00579 (+0,72 %), die Zellenlänge
4,99701 (+0,54 %), die gespreizte Spannweite 4,85138 (−2,39 %). Genommen wird trotzdem **der
Balken**: er ist die einzige direkte Maßstabsaussage des Risses und hängt von keiner Zahl ab,
die er prüfen soll; die Zellenlänge trifft er auf −0,09 %. Wer die drei Marken gewichtet
zusammenfasst, kommt auf 4,997 und macht das Modell 0,55 % größer — das ist eine Entscheidung,
die ein Nachfolger mit einer besseren Quelle treffen soll, nicht mit denselben drei Marken.

### 25. Die 16,7 m sind OHNE Pitotrohr — hergeleitet, nicht nachgeschlagen

Riss ohne Rohr 16,685 m (−0,09 % gegen [PUB] 16,7), mit Rohr 17,771 m (+6,4 %). **Keine Quelle
sagt es ausdrücklich.**

### 26. Der Riss zeigt die MiG-23M, das Modell ist die MLD

Eine bewusste Abweichung: **die Rückenflosse der M entfällt** [WEB, MiG-23ML]. Für die andere
Bodenlage der ML ist jetzt der ML-Riss herangezogen (Fahrwerk). Nicht behandelt: SOS-3-4-Anbau,
Wirbelerzeuger am Pitotrohr.

### 27. Die vierte Raste (33°) ist gebaut, aber unbelegt in ihrer Wirkung

`kSweepDetents` enthält 33° [WEB, MLD]. Ob die dort gemessene Spannweite stimmt, sagt keine
Quelle — sie folgt aus derselben Drehung wie 16/45/72.

**Geschlossen: die alte #21 (Höhe 4,52 gegen 4,82 m).** Der unabhängige ML-Riss gibt bei seinem
eigenen Maßstab (Rad- und Höhenmarke, 4,665 mm/px) 4,826 m — [PUB] 4,82 hat recht, die
Frontansicht des M-Risses nicht. Die Entscheidung von Runde 1, 4,82 m zu bauen, war richtig.

---

## E — Prüfungen, die dieses Asset NICHT hat

| # | Fehlt | Warum das zählt |
|---|---|---|
| 28 | **Kein Vergleich gegen ein Foto.** Alle Tore sind Selbstkonsistenz oder Riss | die 2-%-Silhouettenschranke misst L0 gegen L3, nicht L0 gegen die Wirklichkeit |
| 29 | **Die Flügelfläche wird nicht geprüft.** [PUB] nennt 37,35 m² gespreizt; das Netz baut 18,2 m² freitragende Paneelfläche plus einen Handschuh, dessen Bezugsfläche niemand definiert | eine Bezugsfläche ist eine Konvention, kein Maß — deshalb bewusst kein Band, aber es fehlt eine Aussage |
| 30 | **Kein LOD-Test der Knoten.** Dass `ctl.*` und `gear.*` auf allen vier Stufen dieselben Ursprünge haben, wird je Stufe gegen `node_table()` geprüft, aber nicht Stufe gegen Stufe | ein Knoten, der auf L2 woanders sitzt, springt beim Umschalten. Heute nur mittelbar gedeckt |
| 31 | **Die Umschaltweiten sind sehr kurz** (2,0 / 4,4 / 9,3 m) | Treiber ist allein die Facettierung des größten Rumpfradius; ein zweiter Treiber (kleinstes sichtbares Bauteil) fehlt |
| 32 | **Kein Test gegen den Renderer.** Ob `sim/src/render/` die Knotenkonvention (lokale +X = Achse) liest, ist ungeprüft | die Konvention ist im Sidecar erklärt und sonst nirgends verankert |
| 33 | **Das Silhouettentor konvergiert nicht.** 361/1447/2887 Azimute geben 0,9440/1,0309/1,0799 % | `kSilSupShortfallPp` ist eine geometrische Fortsetzung, keine Schranke — ein Nachfolger mit mehr Rechenzeit soll die Reihe verlängern |
