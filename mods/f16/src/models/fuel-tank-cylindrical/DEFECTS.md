# fuel-tank-cylindrical — offene Defekte

> Der Startpunkt der naechsten Runde. `doc/assets.md` §1: *„The critic's open defects survive"*.
>
> **Runde 3.** Eignerentscheid: **alles metrisch, Dezimalsystem, projektweit.** Damit ist die
> schwerste Frage der Runde entschieden, und ihre Folge ist groesser als eine Tabellenwahl — ein
> ganzes Bauteil faellt weg. Zweite Lehre: **beide Tore meldeten weiter gruen, waehrend die Sache
> rot war.** Ein Schaetzer ohne Konvergenzaussage ist kein Tor.

## Geschlossen in Runde 3

### Ein Einheitensystem — und der Windring verschwindet

API 650 fuehrt **zwei Regelsaetze**, und die Norm sagt es selbst (Kasten vor 5.4): *„when 5 mm
(3/16 in.) thick material is specified, 4.8 mm thick material may be used in the SI rule set"*. Die
Tabelle zu 5.6.1.1 nennt fuer D < 15 m **5 mm** und **3/16 in** = 4,7625 mm — zwei Zahlen, keine
Umrechnung. Runde 2 baute die Schale in Zoll und alles andere in SI.

| | Runde 2 (gemischt) | Runde 3 (SI) |
|---|---|---|
| Schale | 4,7625 / 6,35 mm | **5 / 6 mm** |
| H1 [5.9.7.1] | 8,376 m | **9,460 m** |
| transformierte Schale [5.9.7.2] | 8,503 m | **8,861 m** |
| Zwischen-Windring [5.9.7.3] | Pflicht, 100×75×7, Lage 5,3923 m | **KEINER — 8,861 < 9,460** |

Der Ring existierte nur als Folge der Einheitenmischung. Mit ihm fallen ersatzlos: die Profilwahl
aus Tabelle 5-20, die Lagerechnung nach 5.9.7.3.1/5.9.7.3.2, das Ausweichen vor der Treppe nach
5.9.7.3.2, die 90-mm-Zoll/Millimeter-Frage und die Durchdringung Stufe × Ring. `_FT` kommt in
`fuel_tank_geometry.py` genau einmal vor — in der Katalogzeile, aus der der Tank bestellt ist.

### Beide Tore konvergieren jetzt und melden die Konvergenz

| Tor | Runde 2 | Runde 3 |
|---|---|---|
| **Silhouette** | 13 Azimute → 1,964 %. Bei 360 sind es 2,378 % — dieselbe Geometrie, nur genug Proben | Azimutzahl verdoppeln bis der schlechteste Wert um < 0,02 pp steigt, **mindestens 360**. Die ganze Reihe steht in `asset.json`. Rasterer voll vektorisiert (Δ 300 ms → ~10 ms je Ansicht), sonst waeren 360 Azimute nicht bezahlbar |
| **Durchdringung** | `samples=10` las fuer (Ringmauer, Treppenband) **0,00 cm³**; wahr sind elf Liter. Und erfand: 1,59 cm³ bei n=40, null bei n=80 | **Zwei Stufen.** Detektor: kreuzt eine Kante ein Dreieck (Möller-Trumbore, exakt, kein Abtastfehler)? Schaetzer: nur wo der Detektor Ja sagt, Gitter verdoppeln bis < 5 % relative Aenderung. Gemeldet wird der Wert **mit** seiner Konvergenz |

**Gemessen, alle vier Stufen: 0 ungewollte Durchdringungen.** Der neue Detektor fand dabei selbst,
was keine Liste hatte: Naht durch Mannlochblech, Naht im Kopfwinkel, Stufe durch Nahtwulst.

### Der Cauchy-Schaetzer hat seine Stimme verloren

Die Umschaltweite kam bisher aus dem Maximum von (a) Rundungsabweichung und (b) Cauchy-Groesse
`sqrt(A/4)` des groessten weggelassenen Koerpers. (b) ist bei duennen, langen Koerpern grob falsch:
fuer die Handlaeufe meldete sie **918 m**, waehrend die gemessene Silhouettenaenderung ein Bruchteil
der Grenze ist. **Wo eine Messung steht, hat die Naeherung keine Stimme** — (b) ist jetzt nur noch
Diagnose (`lod_switch.lost_cauchy_m`), die Weite kommt aus (a), das Urteil aus der Messung.

### Die Ringkorrektur gleicht jetzt Flaeche UND Umfang aus

Bis Runde 2 bekam jede n-Ecke den umfangsgleichen Radius, mit Cauchy begruendet (mittlere
Silhouettenbreite exakt). Richtig, und zu wenig: **das Tor misst XOR-FLAECHE**, und in der
Draufsicht ist Flaeche kein Umfang. Eine umfangsgleiche 16-Ecke hat 1,29 % zu wenig Flaeche — und
genau **2,58 %** XOR wurden dort gemessen, der geschlossen vorhergesagte Wert 2 × 1,29 %. Der
Faktor loest jetzt `a₀k² + p₀k − 2 = 0`, also ±0,28 % statt 0 % Breite und −0,82 % Flaeche.

### Der Rest

| Was | Runde 2 | Runde 3 |
|---|---|---|
| Senkrechte Stossnaehte | fehlten ganz | 6 Bleche je Schuss, um ein halbes Blech versetzt [5.1.5.2 b] |
| Verstaerkungsbleche | schwebten 2,7–13,1 mm (flache Scheibe nach aussen geschoben) | **gewalzt**, Polarnetz auf die gebaute Polygonwand |
| Podest licht | 586 mm | **610 mm** [T.5-17 Pkt.2 „after making adjustments at all projections"], per `assert` geprueft |
| Gelaenderhoehe | Achse auf 1070 → 1091 mm Oberkante | Achse einen Rohrradius tiefer; Konvention **schriftlich** ([OSHA 1910.29(b)(1)] misst die Oberkante) |
| `kJointCapCm3 = 250` ohne Quelle | absolut, Summe ungedeckelt | **relativer** Anteil des kleineren Bauteils, schlechteste Quote wird gemeldet (5,7 % von 10 %) |
| Bodenblech | 6 mm `[SET]` | **[API 5.4.1]** „not less than 6 mm" |
| Gelaender auf L3 | vorhanden | **gefallen** — der Kritiker mass 0,389 % dafuer |
| Stufen auf L3 | zum Band verschmolzen | **behalten**: gemessen kostete die Verschmelzung 0,9 pp, weil der Saegezahn auf 62 m noch 3,5 px je Stufe misst |

**Ergebnis:** L2→L3 XOR **1,560 %** bei 324 px über 360 Azimute (Δ 0,000 pp), alle Paare unter der
Grenze, alle Selbstpruefungen gruen, Bau bytegleich.

| LOD | Segmente | Dreiecke | Koerper | Umschaltweite |
|---|---|---|---|---|
| L0 | 96 | 13 348 | 149 | 11 m |
| L1 | 48 | 8 356 | 149 | 43 m |
| L2 | 24 | 4 880 | 90 | 62 m |
| L3 | 20 | 2 928 | 74 | — |

---

## Offen, gereiht

### 1 — Der Tank steht nackt in der Landschaft

Kein Auffangwall, keine Rohrleitung am Bodenstutzen, kein Schaumrohr, keine Beschriftung, kein
Gefahrgutschild. Groesster Abstand zwischen „richtiges Objekt" und „glaubwuerdige Szene".
*Schliessen:* Frage an den Eigner — `doc/asset-inventory.md` kennt diese Teile nicht.

### 2 — Keine Textur, und der Shader, der sie ersetzen soll, existiert nicht

Das Nahtraster ist jetzt Geometrie, aber 3 mm Relief sind bei 65 px/m unter einem Pixel: was Naehte
auf Fotos sichtbar macht, ist Farbe und Rost, nicht Relief. Der prozedurale Shader ist nicht gebaut.
Bis dahin ist der Tank gleichmaessig sauber und die Naehte nur im Streiflicht zu sehen.

### 3 — Kein Zerstoerungszustand

Comanche macht den Tank in 4 von 10 Missionen zum Ziel. Keine zerstoerte Fassung.

### 4 — Umlaufendes Dachrandgelaender fehlt

Gebaut ist der 2,4-m-Bogen des Podests. [5.8.10 c] laesst beides, aber alle drei Fotoreferenzen
zeigen ein Gelaender rings um die Dachkante. **Die auffaelligste Abweichung gegen die Referenz.**
Das Budget dafuer liegt bereit: die auf L3 gefallenen Gelaender.

### 5 — Sieben gesetzte Masse

| Konstante | Wert | Warum ungedeckt |
|---|---|---|
| `kVentDia` / Zahl der Ventile | 200 mm / 1 | API Std 2000 (5.8.5.2) nicht gerechnet; kein Peilstutzen, keine Notentlueftung |
| `kShellManholeReinf` | 930 mm | T.5-5a gibt den Blechdurchmesser fuer Mannloecher nicht an |
| `kShellManholeBolts` | 20 | dieselbe Tabelle nennt die Schraubenzahl nicht |
| `kNozzleFlangeDia/Thk` | 280 / 25 mm | ASME B16.5 nicht beschafft |
| Treppenquerschnitte | 30 / 25 / 200 / 10 / 42 / 48 mm | 5.8.10 b nennt **PIP STF05501/05520/05521** — nicht beschafft |
| Schweissraupe | 3 × 14 mm | keine Quelle |
| `kPlatesPerCourse` | 6 | 5.1.5.2 b fordert den Versatz, nicht die Blechlaenge |

### 6 — Materialwerte ohne Messung

Alle vier PBR-Saetze `[SET]`, kein Foto, keine Referenztafel. Der Anstrich wirkt ueberstrahlt.

### 7 — Umfangslagen willkuerlich

Mannloch 300°, Stutzen 345°, Dachmannloch 30°, Entlueftung 70°, Treppenfuss 130°. Letzterer wurde
gedreht, **damit die Seitenansicht des Beweisrenders die Treppe zeigt** — sachfremd.
*Schliessen:* als Parameter des Platzierers exportieren, mit der realen Regel.

### 8 — Was in den Pruefungen bleibt

- **T-Stoesse nur JE KOERPER.** Wo zwei Netze aneinanderstossen, prueft nichts.
- **Der Durchdringungs-DETEKTOR ist exakt, der SCHAETZER nicht.** Er konvergiert bis 5 % relativ und
  meldet die Konvergenz; ein exakter Dreieck-Dreieck-Volumenschnitt waere die saubere Fassung.
- **`kJointFrac = 0,10` ist gesetzt.** Die Kappe ist jetzt relativ und skaliert mit dem Modell, aber
  der Anteil selbst hat keine Quelle. Die 12 Praefixregeln in `kJoint` sind eine
  Konstruktionsaussage — sie darf niemand erweitern, statt Geometrie zu bessern.
- **Der Rasterer ist ein Flaechen-, kein Kantentest.** Zwei Netze mit gleicher Flaeche und
  verschobener Kante geben dieselbe Maske. Fuer das Poppen ist das die richtige Groesse; fuer
  Kantenflimmern waere es die falsche.

### 9 — Kein Dachtragwerk, kein Innenraum

Getragenes Kegeldach ohne Sparren und Mittelstuetze (5.10.4). Das Dachmannloch oeffnet auf nichts.
Das Verstaerkungsblech des Dachmannlochs ist ein Kreisringsektor, kein Kreis — es LIEGT an, hat aber
die falsche Umrisslinie.

### 10 — Kein Koerper fuer die Physik, keine UV-Koordinaten

`doc/body-format.md` §1 will SEGMENT und CONTACT; die `.glb` traegt nur Sichtgeometrie. UVs fehlen
bewusst — wer je eine Textur will, muss zuerst abwickeln.

### 11 — Der Bau dauert 2 min 15

Fast alles davon sind die beiden Tore (Durchdringung paarweise ueber 149 Koerper, Silhouette ueber
360+ Azimute in vier Aufloesungen). Das ist der Preis dafuer, dass sie wirklich messen, und er ist
richtig bezahlt — aber bei 93 Assets ist er nicht mehr tragbar. Ein `--fast`-Pfad fuer die
Zwischenrunden mit dem vollen Tor nur vor dem Commit waere die naechste Verbesserung.
