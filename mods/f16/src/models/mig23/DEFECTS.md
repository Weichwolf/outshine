# mig23 — offene Defekte

> Der Startpunkt der nächsten Runde. `doc/assets.md` §1: *„The critic's open defects survive"*.
>
> **Runde 1.** Das Asset ist grün an allen Toren, die es sich selbst stellt: 51–54 geschlossene
> Körper je Stufe, null ungewollte Durchdringungen, 11 Maße am **gebauten Netz** gegen [PUB]/[BP]
> gemessen, Silhouette 1,20 % gegen 2 % über 361 Azimute, zweimal gebaut → bytegleich, 43–46 s.
> Was hier steht, ist das, was **trotzdem** falsch oder unbelegt ist — gereiht nach dem, was ein
> Kenner auf einem Frame zuerst sieht.

---

## A — sichtbar falsch (was das nächste Bild verbessert)

### 1. Bei 72° klafft die Wurzelfuge zwischen Paneel und Handschuh

**Gemessen im gerenderten Bild (Draufsicht, `ctl.wingsweep.* = 56°`).** Das Paneel dreht korrekt —
Spannweite 7,779 m, beide Seiten symmetrisch —, aber seine Wurzel wandert dabei **von x = 1,934 m
auf 1,749 m nach innen und 0,96 m nach achtern**, während die Handschuhkante bei 1,934 m stehen
bleibt. Es entsteht ein offener Keil von rund 0,2 × 1,0 m je Seite.

Das ist kein Rechenfehler, sondern ein **fehlendes Bauteil**: die MiG-23 hat an dieser Stelle eine
Wurzeldichtung (der Handschuh überlappt das Paneel, das darunter gleitet). Nachfolger: einen Körper
`glove.seal.<side>` bauen, der den überstrichenen Sektor abdeckt — er ist ein Kreisringsegment um
den Drehzapfen mit dem Radius der Paneelwurzel und dem Öffnungswinkel 56°.

### 2. Die Kanzel liest sich nicht als Kanzel

Im ersten Bild war sie **gar nicht** zu sehen: die spaltenweise Extremwertsuche im Seitenriss kann
Haubenoberkante und Rumpfrücken nicht trennen, und der Rumpf bekam die Haubenlinie als eigene
Oberkante. Nach der Korrektur (Deck in der Cockpitbucht auf die Brüstungslinie gelegt,
`kFusStations` 2400…2800 px) steht die Haube 0,32 m über dem Deck und die zwei Bügel sind sichtbar
— **die Verglasung selbst ist es kaum**. Das Material wurde deshalb von α 0,20 auf 0,55 gezogen; das
ist ein Kompromiss, kein Beleg.

Offen: die Haubenkontur ist [SET] (fünf Stationen, `kCanopy*Px`), es fehlen Spriegel, Rückspiegel,
Schleudersitz und ein Cockpitboden. Ein Blick von vorn/oben zeigt heute eine leere Wanne.

### 3. Einlauf und Rumpf stoßen mit einer Kante aneinander

Der Einlaufkasten ist ein sauberer Hohlkörper mit echtem, 1,35 m tiefem Kanal (Geschlecht 1, keine
Durchdringung mit der dunklen Grundplatte) — aber er **verschneidet sich nicht** mit der
Rumpfseite, sondern steht als Quader daneben. Auf der echten MiG-23 läuft die Einlaufaußenwand in
den Rumpf über und die Grenzschichtplatte sitzt in einer Nut. Sichtbar im Viertelbild als harte
Stufe.

### 4. Das Fahrwerk ist eine Skizze

Die MiG-23 hat ein sehr eigenwilliges Hauptfahrwerk (Zug-Druck-Kinematik mit zwei Streben und einem
querliegenden Radträger). Gebaut sind zwei Rundstäbe, ein Rad, eine Klappe und ein Schleppstab. Der
Radstand `kWheelbase = 5,77 m` ist **[SET]** und durch nichts belegt; Bugbeinstation 3,35 m ebenso.
Die Spurweite 2,658 m ist [WEB] und wird am Netz geprüft, der Radstand nicht.

### 5. Bauchflosse und Bremsklappen sind Platten

`ctl.ventral` ist ein 0,10 m dickes Prisma mit fünf Eckpunkten, die Bremsklappen sind Quader. Die
Bauchflosse hat am Original ein Profil und einen Knick; die Bremsklappen sitzen an anderer Stelle
(`kAirbrakeY` ist [SET]) und haben drei statt zwei Blätter.

---

## B — Zahlen ohne Beleg (jede [SET]-Zahl dieses Assets)

| # | Konstante | Was fehlt |
|---|---|---|
| 6 | `kPivotZ = 0,16 m` | Höhe des Drehzapfens über der Triebwerksachse. Der Riss zeigt sie nur in einem Schnitt, den ich nicht kalibrieren konnte |
| 7 | `kPanelRootU = 0,42 m` | Trennfuge Paneel/Handschuh. Aus dem 72°-Riss geschätzt |
| 8 | `kPanelDihedralDeg = −1,0°` | V-Stellung des Paneels. Keine Quelle nennt sie |
| 9 | `kSlatChordFrac 0,16 · kFlapChordFrac 0,26 · kSpoilerChordFrac 0,14` und alle Ausschläge (20°/25°/45°) | Sehnenanteile und Grenzen der Klappen. **Belegt ist nur, DASS es Vorflügel, Landeklappen und Störklappen gibt** [WEB] |
| 10 | `kGloveTeY = −1,60 m`, `kGloveSawtoothU/Depth` | Handschuhhinterkante und Sägezahn. Der MLD-Sägezahn ist [WEB] belegt, seine Größe nicht |
| 11 | `kTailAnhedralDeg = −10°`, `kTailTipChord = 0,55 m`, `kTailHingeFrac = 0,28` | Anhedral aus Schnitt Г-Г abgeschätzt. **Zusatzbefund:** die Leitwerksspitze liegt im Riss beidseits ungleich weit außen (539 gegen 586 px, 0,23 m) — Rissverzug, genommen wird das Mittel |
| 12 | `kWheelbase`, `kNoseGearY`, alle Einzugswinkel | s. #4 |
| 13 | `kNozzleThroatDia 0,86 m`, `kNozzlePetals 24`, `kNozzleDepth 0,55 m` | nur der Austrittsdurchmesser (1,245 m) ist [BP] |
| 14 | Profilform | Die **Dicke** ist [PUB] (TsAGI SR-12S, 6,5 % / 5,5 %), die **Form** ist eine symmetrische NACA-Vierziffernverteilung. SR-12S-Koordinaten sind nicht öffentlich |
| 15 | `kSilResCap = 1024 px` | Feinste Rasterung des Silhouettentors. Bei der hergeleiteten L2→L3-Weite von 9,3 m füllt das Flugzeug mehr als das Zielbild; 1024 px ist eine Kostenentscheidung, keine Herleitung |
| 16 | `kRudderMaxDeg 25°`, `kRudderChordFrac 0,26` | |
| 17 | `kAirbrakeY`, `kAirbrakeMaxDeg` | |
| 18 | Alle sieben Materialien | Ein Anstrich hat keine Norm; die PBR-Werte sind plausibel gewählt, nicht gemessen |
| 19 | Rumpfoberkante 2400…2800 px, Formexponenten `kFusShapeKnots` | s. #2. Die Exponenten (2,0…2,9) sind aus den Schnitten А-А/Б-Б/В-В/Г-Г qualitativ abgelesen, nicht bemaßt |
| 20 | Rumpfhalbbreiten achtern von 1400 px | Der Grundriss ist dort von Flügel und Handschuh verdeckt; die Werte sind aus den lesbaren Stellen fortgeschrieben |

---

## C — Belegte Widersprüche, die stehen bleiben

### 21. Höhe: Riss 4,52 m gegen [PUB] 4,82 m — **6,3 %**

Die Frontansicht des Risses gibt Flossenspitze über Bodenlinie **906,5 px = 4,515 m**. [PUB] sagt
4,82 m. Beide Spannweiten und die Länge treffen auf demselben Maßstab **±0,3 %** — die Höhe ist der
einzige Ausreißer, also teilen Front- und Seitenriss keine Höhenbezugslinie.

**Entscheidung:** das Modell baut **4,82 m [PUB]**, und die Fahrwerksbeinlänge ist daraus
*gerechnet* (`kGroundZ = kFinTipZ − kHeight`). Der Preis: die Bodenfreiheit ist nicht gemessen,
sondern die Restgröße. Wer eine Beinlänge findet, dreht die Rechnung um und prüft die Höhe.

### 22. Der Maßstabsbalken ist um 0,33 % zu kurz

Balken: 4,96524 mm/px. Beide veröffentlichten Spannweiten verlangen 4,9800 bzw. 4,9865 mm/px.
Genommen wird **4,9814 mm/px** (Anpassung an beide Spannweiten), weil zwei weit auseinanderliegende
Marken stärker sind als eine. Der Riss ist gleichmäßig geschrumpft — Papier oder Scan.

### 23. Die 16,7 m sind OHNE Pitotrohr — hergeleitet, nicht nachgeschlagen

Der Riss misst Radomspitze→hinterste Leitwerksecke **16,648 m** (−0,31 % gegen [PUB] 16,7) und mit
Rohr **17,734 m** (+6,2 %). Ein Maßstab, der zwei Spannweiten auf 0,3 % trifft, kann eine Länge
nicht um 6 % verfehlen — also meint [PUB] die Zelle ohne Rohr. **Keine Quelle sagt das ausdrücklich.**

### 24. Der Riss zeigt die MiG-23M, das Modell ist die MLD

Eine bewusste Abweichung ist gebaut: **die Rückenflosse der M entfällt** („with the dorsal fin
extension removed", [WEB], MiG-23ML). Nicht behandelt sind: die andere Bodenlage der ML (leichteres,
neu ausgelegtes Hauptfahrwerk), der SOS-3-4-Anbau und die Wirbelerzeuger am Pitotrohr (nur als
Fugen angedeutet).

### 25. Die vierte Raste (33°) ist gebaut, aber unbelegt in ihrer Wirkung

`kSweepDetents` enthält 33° [WEB, MLD]. Die Kinematik gibt sie kostenlos her; ob die dort gemessene
Spannweite (13,006 m) stimmt, sagt keine Quelle — sie folgt aus derselben Drehung wie 16/45/72.

---

## D — Prüfungen, die dieses Asset NICHT hat

| # | Fehlt | Warum das zählt |
|---|---|---|
| 26 | **Kein Vergleich gegen ein Foto.** Alle Tore sind Selbstkonsistenz oder Riss. Die 2-%-Silhouettenschranke misst L0 gegen L3, nicht L0 gegen die Wirklichkeit | genau der Fehler, den der Tank in Runde 4 am Dachrandgeländer bezahlt hat |
| 27 | **Die Flügelfläche wird nicht geprüft.** [PUB] nennt 37,35 m² gespreizt; das Netz baut 14,4 m² freitragende Paneelfläche plus einen Handschuh, dessen Bezugsfläche niemand definiert | eine Bezugsfläche ist eine Konvention, kein Maß — deshalb bewusst kein Band, aber es fehlt eine Aussage |
| 28 | **Kein LOD-Test der Knoten.** Dass `ctl.*` und `gear.*` auf allen vier Stufen dieselben Ursprünge und Achsen haben, wird nicht gemessen | ein Knoten, der auf L2 woanders sitzt, springt beim Umschalten |
| 29 | **Die Umschaltweiten sind sehr kurz** (2,0 / 4,4 / 9,3 m). Treiber ist die Facettierung des größten Rumpfradius; bei einem schlanken Jet ist der klein, also wird spät umgeschaltet | konservativ, aber teuer: L0 läuft bis 2 m, danach L1 bis 4,4 m — praktisch immer L3. Ein zweiter Treiber (kleinstes sichtbares Bauteil) fehlt |
| 30 | **Kein Test gegen den Renderer.** Ob `sim/src/render/` die Knotenkonvention (lokale +X = Achse) liest, ist ungeprüft | die Konvention ist im Sidecar erklärt und sonst nirgends verankert |
