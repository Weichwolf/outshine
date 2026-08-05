# fuel-tank-cylindrical — offene Defekte

> Der Startpunkt der naechsten Runde. `doc/assets.md` §1: *„The critic's open defects survive"*.
>
> **Runde 4.** Eine Lehre, und sie ist groesser als dieses Asset: **eine Pruefung, die ihre eigenen
> Eingaben liest, ist keine Pruefung.** Das Bauskript hatte genau EINEN `assert`, und der lautete
> sinngemaess `assert (a + b) - b >= a`. Er meldete die lichte Podestbreite als eingehalten; am Netz
> waren es 618,0 statt 610,0 mm, weil der Abzug eine Fussleiste mitzaehlte, die nach aussen sitzt.
> Norm gehalten — aus Versehen. Das war das **sechste Mal** in diesem Baum, dass eine Pruefung
> berichtet, ohne zu messen, und das gefaehrlichste, weil dieses Skript die Vorlage fuer 92 weitere
> Assets ist.
>
> Die Regel, die ab jetzt gilt: **ein `assert` in einem Bauskript misst am gebauten Netz, nie an den
> Eingaben.** Der Beweis, dass sie beisst: sie fand beim ERSTEN Lauf drei Normverletzungen, die drei
> Runden ueberlebt hatten.

## Geschlossen in Runde 4

### Die Pruefung misst jetzt das Ding, nicht die Rechnung

`check_norms` liest **29 Masse aus Punktkoordinaten** des fertigen Netzes und haelt sie gegen
API 650 / OSHA. Kein Wert stammt aus `fuel_tank_geometry`; wo eine Umrechnung noetig ist, wird sie
benannt (die Ringkorrektur k(n), wobei **n aus dem Traufenring GEZAEHLT** wird). Die drei
`assert`-Zeilen in `_stair_steps()` und der eine auf `kWindGirderNeeded` sind ersatzlos gefallen —
sie lasen Konstanten, die zwei Zeilen darueber aus denselben Konstanten entstanden waren.

**Was die Messung beim ersten Lauf fand:**

| Zeile | gemessen | gefordert | Ursache |
|---|---|---|---|
| `stair.width` | **553,0 mm** | 710 mm [T.5-18 Pkt.2] | Die unterste Stufe lag bei 179,6 mm, also UNTER der Ringmauerkrone (300 mm), und wich der 147 mm vorstehenden Ringmauer aus, indem sie innen beschnitten wurde |
| `stair.width` (danach) | **703,6 mm** | 710 mm | Der Aussenradius stand fest, die Innenkante sitzt aber auf der GEBAUTEN Wand (Polygonecke + Schweissraupe + Sehne) |
| `platform.post_spacing` | **2490,0 mm** | 2400 mm [T.5-17 Pkt.8] | Die Pfostenzahl kam aus dem Bogen der Laufsteg-MITTELLINIE; die Pfosten stehen am Aussenholm, dort ist der Bogen 2500 mm |

Behoben, jeweils aus der Norm selbst statt durch Nachgeben:

- **Die Treppe beginnt am Tankboden.** [T.5-18 Pkt.10] sagt es woertlich: *„Stairways shall extend
  from the bottom of the tank up to a roof edge landing or gauger's platform"*, und derselbe Punkt
  will die Wangenenden frei vom Boden. Steighoehe damit 9,7536 m statt 10,0596 m → **55 statt 56
  Steigungen**, R 177,34 mm, r 255,32 mm, 2R+r = 610,2 mm, Winkel 34,78°. Die unterste Stufe liegt
  153 mm ueber der Ringmauerkrone, jede Stufe hat volle Breite.
- **Der Aussenradius folgt der gebauten Innenkante von L0**, geschlossen gerechnet
  (`kStairInnerRBuilt`). Nicht der groebsten Stufe: sonst diktierte das 20-Eck der Fernsicht eine
  765-mm-Treppe. Der Aussenrand bleibt auf allen vier Stufen derselbe — er traegt die Silhouette.
- **Pfostenzahl aus dem Bogen am Holm.**

Zwei Fehler in der Pruefung selbst, beide durch Ablesen der Zahlen gefunden statt durch Bestehen:

- Die Schussdicke wurde ueber ein z-Band gesucht. Ein Band zwischen zwei Hoehen enthaelt **gar keine
  Punkte** — die Wand eines Schusses ist EINE gerade Strecke —, und bis an die Stossfuge gezogen
  greift es den dickeren Schuss darunter und die Schweissraupe mit ab. Ergebnis: 6 mm fuer jeden
  Schuss und **H1 = 14,92 statt 9,46 m**. Ein Messfehler, der die Windpruefung LEICHTER machte. Das
  Profil wird jetzt aus den **Flaechen** des Netzes zurueckgewonnen (die Kante eines Vierecks bei
  Azimut 0 IST die Profilkante) — eine Sortierung nach z scheitert, weil an jeder Stossfuge zwei
  Punkte auf derselben Hoehe liegen.
- Die Bandtoleranz war 1e-9 in der Einheit der Zeile, also ein Pikometer. Eine exakt eingehaltene
  5-mm-Wand fiel durch. Jetzt 1e-6 — Nanometer bei Millimeterzeilen, unter jeder Fertigungstoleranz.

### Das Dachrandgelaender steht

[API 5.8.10 a] erklaert OSHA 29 CFR 1910 Subpart D fuer mitverbindlich; [OSHA 1910.28(b)(1)(i)]
verlangt an einer ungeschuetzten Kante ab 1,2 m eine Absturzsicherung. Die Dachkante liegt
**10,07 m** ueber Gelaende. Bemasst nach [T.5-17] Pkt. 4/7/8: **20 Pfosten, 2310 mm Teilung,
1070 mm Oberkante, Mittelholm auf halber Hoehe**, unterbrochen nur am Podestbogen.

Der Ring folgt der **gebauten Polygontraufe**, nicht dem Kreis — auf dem Kreisradius haette er bei
n = 20 an den Facettenmitten 91 mm ueber der Dachkante gestanden.

**Er steht auf allen vier Stufen, auch auf L3**, und damit faellt eine Entscheidung der Runde 3:
die Gelaender bleiben dort. Gemessen kostet ihr Wegfall jetzt **0,30 pp** (2,099 gegen 1,800 % bei
L2→L3) aus einem Budget, das die Polygonteilung allein schon zu 1,36 pp verbraucht — gegen die
0,389 pp, mit denen Runde 3 sie fallen liess. Preis: 1100 Dreiecke auf der billigsten Stufe.

Der Kritiker hatte das Fehlen beziffert (XOR gegen dieselbe Geometrie MIT Ring: 1,293 % @ 11 m ·
1,441 % @ 43 m · 0,968 % @ 62 m) und das Argument dazu geliefert, das hier das eigentliche ist:
**das war ein Fehler gegen die Wirklichkeit, nicht zwischen zwei eigenen Naeherungen.** Die
2-%-Schranke ist ein Selbstkonsistenz-Budget und deckt so etwas nicht.

### Das Silhouettentor mass ein Alias und nannte es Konvergenz

Runde 3 verdoppelte die Azimutzahl, bis der schlechteste Wert um weniger als 0,02 pp stieg.
Gemessen:

| Azimute | ggT(k, 20) | L2→L3 | schlechtester Azimut |
|---|---|---|---|
| 180 | 20 | 1,5631 % | 54,000° |
| 360 | 20 | 1,5631 % | 54,000° |
| 720 | 20 | 1,5631 % | 54,000° |
| 1440 | 20 | 1,5938 % | 53,750° |
| 361 | 1 | 1,5681 % | 53,850° |

Dreimal derselbe Wert am selben Azimut — nicht Konvergenz, sondern **Alias**: das 20-Eck hat 18°
Periode, und 1°, 2° und 0,5° tasten alle dieselben 18 Phasen ab. Jetzt **feste 361 Azimute**
(19², teilerfremd zu 96/48/24/20).

Und weil ein endliches Raster ein **Supremum** nie ganz findet, wird der Fehlbetrag gemessen statt
behauptet: ueber ein 90°-Fenster bei 0,05° (1801 Proben) liegt das Supremum bei 1,5938 %, benachbarte
Proben unterscheiden sich im Mittel um 0,023 pp und im schlimmsten Fall um 0,315 pp. Ein 1°-Raster
findet 0,026 pp zu wenig. **Zuschlag 0,03 pp vor dem Urteil.**

### Die Bauzeit, und wo sie wirklich lag

**Vorher 88,4 s, jetzt 43 s** (kalt; auf thermisch gedrosselter Maschine 59 s — dieselbe Arbeit
kostet dort 1,4×, sichtbar an `bau` 0,6 → 1,2 s).

Der Kritiker vermutete die Kosten in der Aufloesung (`res^1,43`). **Gemessen stimmt das nicht:**
81 px kosteten 16,2 ms, 1280 px 57,8 ms — 28 % der Kosten bei 0,4 % der Pixel. Der Grund: die
Rasterung baute die Dreiecksliste **in jeder Ansicht neu auf**, in einer Python-Schleife ueber alle
Vielecke. Das ist O(Dreiecke) je Ansicht und von der Aufloesung unabhaengig.

| Hebel | Wirkung | Aussageverlust |
|---|---|---|
| Punktfeld + Dreiecksindizes **einmal je Stufe** (`flatten`) | Silhouette 63,5 → 19,5 s | keiner, **bitgleiche** Masken gegengeprueft |
| `np.add.at` → `np.bincount` im Rasterer | 19,5 → 13,3 s | keiner, ganzzahlig identisch |
| Verdopplungsleiter weg, 361 Azimute fest | −43 % Azimutarbeit | keiner, die Sprossen 90/180 stellten nichts fest |
| Grob zuerst (¼ Aufloesung, Eskalation ab halber Grenze) | L0→L1 bleibt grob | grob war an diesem Koerper stets die **obere** Schranke (Faktor 1,71 / 1,78 / 1,95 / 3,02) |
| Schnittquader-Schranke fuer erklaerte Schweisspunkte | Durchdringung ~halbiert | **strenger**, nicht schwaecher: bewiesen statt geschaetzt |
| `np.cross` → komponentenweise, `signed_volume` nicht doppelt | Rest | keiner |

Die **Schnittquader-Schranke** ist der einzige Hebel mit einer Nebenwirkung, und sie ist benannt:
fuer 69 der 97 Schweisspunkte liegt schon der Schnittquader unter der Kappe, das gemeinsame Volumen
kann ihn nicht ueberschreiten, also ist die Kappe **bewiesen**. Diese Zahlen sind obere Schranken
und werden im Bericht **getrennt** von den gemessenen gefuehrt — sonst laese sich die groesste Quote
als 9,3 % statt 5,6 %. Jedes Paar, das NICHT in `kJoint` steht, wird weiterhin immer gemessen.

### Der Durchdringungs-Melder schluckt nichts mehr

`min_cm3 = 0,05` warf jeden Detektortreffer darunter weg; auf L3 verschwanden so 57 Paare spurlos.
Der Detektor ist EXAKT — was er findet, verlaesst jetzt die Funktion und steht im Sidecar
(`overlaps_below_verdict`, heute 78 Paare, alle 0,0000 cm³: Bauteile, die sich flaechig BERUEHREN,
etwa ein Gelaenderpfosten auf dem Dachblech). Die Schwelle entscheidet weiterhin, ob eine Zeile den
Bau durchfallen laesst — sie entfernt nur nichts mehr aus dem Bericht.

### Der Windterm steht da, mit seiner Annahme

[API 5.9.7.1, SI] ist H₁ = 9,47 t √((t/D)³) · **(190/V)²**. Der Code liess `(190/V)²` weg — richtig
fuer den Normwert, aber unausgesprochen an genau der Zahl, die den Windring geloescht hat.
[API 5.2.1 k] setzt V = 190 km/h, wenn der Besteller nichts anderes nennt; das steht jetzt als
`kWindSpeedKmh` da. **Marge:** der Ring kehrt zurueck bei V > 190·√(H₁/W_tr) = **196,32 km/h**, also
3,32 %. Nachgerechnet und bestaetigt.

Nebenbei zwei Zitatfehler behoben: der Satz *„If the height of the transformed shell is greater than
the maximum height H1, an intermediate wind girder is required"* steht in **5.9.7.2 b**, nicht
5.9.7.3 (5.9.7.3 regelt erst die LAGE eines geforderten Ringes) — im PDF nachgeschlagen. Und die
Kommentarzahlen 9,4594 / 8,86077 / 1,8330 rad waren veraltet; richtig sind **9,45999 / 8,860995 /
1,82985 rad**.

### „2 × 1,29 %" war keine Herleitung

Runde 3 schrieb, die gemessenen 2,58 % XOR seien *„der geschlossen vorhergesagte Wert 2 × 1,29 %"*.
Das Flaechendefizit stimmt (umfangsgleiche 16-Ecke: **−1,2884 %**), der Faktor 2 nicht. Nachgerechnet
(Polarintegral ½∫|r₁²−r₂²|dφ, 4·10⁶ Stuetzstellen):

| | |
|---|---|
| XOR(Kreis, 16-Eck umfangsgleich) | **1,5151 %** |
| XOR(24-Eck, 16-Eck, beide umfangsgleich) | **1,3030 %** |
| 2 × Flaechendefizit | 2,5768 % — **keiner von beiden** |

Die Behauptung ist aus dem Kommentar entfernt. Die ausgeglichene Korrektur bleibt, aber ohne den
Anspruch, XOR-optimal zu sein — siehe offener Punkt 2.

---

## Offen, gereiht

### 1 — Der Tank steht nackt in der Landschaft

Kein Auffangwall, keine Rohrleitung am Bodenstutzen, kein Schaumrohr, keine Beschriftung, kein
Gefahrgutschild. Groesster Abstand zwischen „richtiges Objekt" und „glaubwuerdige Szene".
*Schliessen:* Frage an den Eigner — `doc/asset-inventory.md` kennt diese Teile nicht.

### 2 — Die Ringkorrektur ist nicht XOR-optimal, und das Tor misst XOR

Gemessen gegen den Kreis bei n = 20:

| Korrektur | k | XOR |
|---|---|---|
| ausgeglichen (heute) | 1,006894 | 0,7032 % |
| flaechengleich | 1,008286 | **0,6363 %** |
| Optimum (Suche) | 1,009310 | **0,6207 %** |

Die heutige Wahl kostet **0,08 pp** gegen das Optimum — bei einem Budget, dessen schlechteste Zeile
1,83 von 2,00 % belegt, ist das nicht nichts. Der Grund fuer den Ausgleich (mittlere
Silhouettenbreite nach Cauchy exakt) ist real, aber er ist eine BREITEN-Aussage, und geurteilt wird
ueber FLAECHE. *Schliessen:* k je Stufe aus einer Minimierung der gemessenen XOR statt aus der
geschlossenen Balance — und dann pruefen, ob die Silhouettenbreite spuerbar leidet.

### 3 — Keine Textur, und der Shader, der sie ersetzen soll, existiert nicht

Das Nahtraster ist Geometrie, aber 3 mm Relief sind bei 65 px/m unter einem Pixel: was Naehte auf
Fotos sichtbar macht, ist Farbe und Rost, nicht Relief. Der prozedurale Shader ist nicht gebaut.
Bis dahin ist der Tank gleichmaessig sauber und die Naehte nur im Streiflicht zu sehen.

### 4 — Kein Zerstoerungszustand

Comanche macht den Tank in 4 von 10 Missionen zum Ziel. Keine zerstoerte Fassung.

### 5 — Die Fussleiste haelt API, aber nicht OSHA

[API T.5-17 Pkt.5] fordert **75 mm**, und so ist sie gebaut. [OSHA 1910.29(k)(1)] fordert
**3,5 in = 88,9 mm**, und [API 5.8.10 a] erklaert OSHA Subpart D fuer mitverbindlich. Beide sind
Mindestmasse, also verletzt 75 mm die strengere der beiden Quellen. Beim Sourcing des
Dachrandgelaenders aufgefallen, in dieser Runde NICHT geaendert: ein Normkonflikt zwischen zwei
verbindlichen Quellen gehoert vor den Kritiker, nicht in einen stillen Konstantentausch.
*Schliessen:* Entscheid, dann `kPlatformToeH = 0,089`.

### 6 — Neun gesetzte Masse

| Konstante | Wert | Warum ungedeckt |
|---|---|---|
| `kVentDia` / Zahl der Ventile | 200 mm / 1 | API Std 2000 (5.8.5.2) nicht gerechnet; kein Peilstutzen, keine Notentlueftung |
| `kShellManholeReinf` | 930 mm | T.5-5a gibt den Blechdurchmesser fuer Mannloecher nicht an |
| `kShellManholeBolts` | 20 | dieselbe Tabelle nennt die Schraubenzahl nicht |
| `kNozzleFlangeDia/Thk` | 280 / 25 mm | ASME B16.5 nicht beschafft |
| Treppenquerschnitte | 30 / 25 / 200 / 10 / 42 / 48 mm | 5.8.10 b nennt **PIP STF05501/05520/05521** — nicht beschafft |
| Schweissraupe | 3 × 14 mm | keine Quelle |
| `kPlatesPerCourse` | 6 | 5.1.5.2 b fordert den Versatz, nicht die Blechlaenge |
| `kRoofRailInset` | Pfostenradius | Anschlussart des Dachrandgelaenders — dasselbe PIP-Blatt |
| `kRoofRailToeboard` | keine | s. Punkt 5; unter der Dachkante steht niemand, aber belegt ist das nicht |

### 7 — Materialwerte ohne Messung

Alle vier PBR-Saetze `[SET]`, kein Foto, keine Referenztafel. Der Anstrich wirkt ueberstrahlt.

### 8 — Umfangslagen willkuerlich

Mannloch 300°, Stutzen 345°, Dachmannloch 30°, Entlueftung 70°, Treppenfuss 130°. Letzterer wurde
gedreht, **damit die Seitenansicht des Beweisrenders die Treppe zeigt** — sachfremd.
*Schliessen:* als Parameter des Platzierers exportieren, mit der realen Regel.

### 9 — Was in den Pruefungen bleibt

- **Der Umschliessungs-Rueckfall ist einseitig und gekappt.** Sagt der Kantentest Nein, prueft er nur
  die ersten **64** Punkte von `v1` in `v2` — nie umgekehrt. Ein Koerper, der ganz in einem frueher
  einsortierten steckt, waere unsichtbar, und bei mehr als 64 Punkten ausserhalb des Schnittquaders
  auch der umgekehrte Fall. Heute null verpasst (alle 97 Treffer kommen ueber den Kantentest),
  latent doch. *Schliessen:* beide Richtungen, und statt `[:64]` ein Punkt je Zusammenhangskomponente.
- **`kOverlapMinCm3 = 0,05` ist gesetzt.** Sie entfernt nichts mehr aus dem Bericht, entscheidet aber
  weiterhin, was den Bau durchfallen laesst. Keine Quelle.
- **`kJointFrac = 0,10` ist gesetzt.** Die Kappe skaliert mit dem Modell, der Anteil selbst hat keine
  Quelle. Die 13 Praefixregeln in `kJoint` sind eine Konstruktionsaussage — sie darf niemand
  erweitern, statt Geometrie zu bessern.
- **T-Stoesse nur JE KOERPER.** Wo zwei Netze aneinanderstossen, prueft nichts.
- **Der Rasterer ist ein Flaechen-, kein Kantentest.** Zwei Netze mit gleicher Flaeche und
  verschobener Kante geben dieselbe Maske. Fuer das Poppen ist das die richtige Groesse; fuer
  Kantenflimmern waere es die falsche.
- **`stair.width` hat null Marge.** Gemessen exakt 710,0000 mm, weil der Aussenradius aus der
  ungeguenstigsten Innenkante geschlossen zurueckgerechnet ist. Deterministisch, aber jede Aenderung
  an Raupe, Teilung oder Stufenbogen kippt die Zeile sofort — das ist Absicht, kostet aber Nerven.

### 10 — Kein Dachtragwerk, kein Innenraum, kein Bodenblech in der Mitte

Getragenes Kegeldach ohne Sparren und Mittelstuetze (5.10.4). Das Dachmannloch oeffnet auf nichts.
Das Verstaerkungsblech des Dachmannlochs ist ein Kreisringsektor, kein Kreis — es LIEGT an, hat aber
die falsche Umrisslinie. Das Bodenblech ist ein **Kreisring** (r = 7,165 bis 7,368 m), keine Scheibe:
in der Mitte klafft ein Loch von 14,3 m Durchmesser. Unsichtbar, solange der Tank auf dem Boden
steht, aber jede Ansicht von unten oder ein Schnitt zeigt es.

### 11 — Kein Koerper fuer die Physik, keine UV-Koordinaten

`doc/body-format.md` §1 will SEGMENT und CONTACT; die `.glb` traegt nur Sichtgeometrie. UVs fehlen
bewusst — wer je eine Textur will, muss zuerst abwickeln.

### 12 — Die Normpruefung urteilt nur auf L0

L1..L3 werden vollstaendig gemessen und im Sidecar abgelegt, aber nicht geurteilt: L0 IST der
Koerper, die groberen Stufen sind Naeherungen, deren Abweichung das Silhouettentor begrenzt. Die
Entscheidung ist begruendet, aber sie hat einen Preis — auf L3 misst `stair.width` 661,7 mm und
`platform.railing_height` 1067,2 mm (sechseckiges Rohr, dessen Scheitel nicht oben liegt), und
niemand haelt fest, ab wann eine solche Abweichung zu gross wird. *Schliessen:* eine zweite,
lockerere Schranke fuer die groberen Stufen — hergeleitet aus der Pixelgroesse ihrer Umschaltweite,
nicht gesetzt.

---

## Zahlen dieser Runde

| LOD | Segmente | Dreiecke | Koerper | Bytes | Umschaltweite |
|---|---|---|---|---|---|
| L0 | 96 | 18 660 | 171 | 1 037 920 | 11 m |
| L1 | 48 | 10 420 | 171 | 620 360 | 43 m |
| L2 | 24 | 5 848 | 112 | 361 624 | 62 m |
| L3 | 20 | 4 576 | 112 | 299 872 | — |

Von Runde 3 (13 348 / 8 356 / 4 880 / 2 928 Dreiecke, 149 Koerper) kommen +40 % Dreiecke und
22 Koerper dazu; das ist das Dachrandgelaender plus die auf L3 zurueckgeholten Gelaender. Der
Koerpersprung 171 → 112 zwischen L1 und L2 sind die senkrechten Stossnaehte und die Schrauben, die
dort ohnehin fallen (`detail` 2 → 1).

| Tor | Ergebnis |
|---|---|
| Silhouette | schlechteste Zeile L2→L3 **1,800 % + 0,03 pp Raster = 1,830 %** von 2,00 % · 361 Azimute · L0→L1 0,578 · L1→L2 1,294 · L0→L3 1,573 |
| Durchdringung | **0 ungewollt**, 28 Schweisspunkte gemessen (groesste Quote 5,6 % von 10 %), 69 durch den Schnittquader bewiesen, 78 Beruehrungen unter der Urteilsschwelle gemeldet |
| Selbstpruefungen | 171 Koerper dicht / Windung / Normalen / T-Stoesse / Verschweissung / Merge — gruen |
| Norm | **29 Masse am gebauten Netz**, L0 alle in Band |
| Determinismus | zwei Laeufe, **alle fuenf Dateien bytegleich** (vier `.glb` + `.asset.json`) |
| Bauzeit | **42,6 s** auf leerer Maschine (vorher 88,4 s). Die Zahl haengt stark an der Fremdlast: bei Lastmittel 3,5 von 6 Kernen 52,7 s, bei 5,3 dann 62 s. Nur der Vergleich UNTER DERSELBEN LAST traegt — dort steht `b275e78` bei **102,9 s** gegen **52,7 s**, Faktor **1,95** bei 40 % mehr Dreiecken |
