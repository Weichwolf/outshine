# fuel-tank-cylindrical — offene Defekte

> Der Startpunkt der naechsten Runde. `doc/assets.md` §1: *„The critic's open defects survive"*.
>
> **Runde 2.** Der Kritiker hat zwoelf Klauseln selbst nachgezogen und zwoelf Defekte mit Zahl und
> Bild belegt. Die wichtigste Lehre steht nicht bei den Massen, sondern bei den Instrumenten: **drei
> der vier Pruefungen konnten nicht rot werden.** Sie sind zuerst repariert worden, danach erst die
> Geometrie — sonst haette Runde 2 gegen ein Messgeraet gebaut, das nicht misst.

## Geschlossen in Runde 2

### Die Tore, die nicht rot werden konnten

| Was | Befund | Jetzt |
|---|---|---|
| **Silhouette schloss das kumulierte Paar aus dem Urteil aus** | druckte `UEBER GRENZE` und meldete zugleich `passed: true` | jede gemessene Zeile zaehlt; das kumulierte Paar wird bei der Entfernung gemessen, ab der die groebste Stufe benutzt wird |
| **Silhouette tastete nur achsparallel ab** | jedes n der Leiter ist durch 4 teilbar, also stand bei 0/90/180 Grad IMMER eine Ecke vor der Kamera. Kritiker mass bei 15 Grad 2,46 %, das Skript meldete 1,81 % | 13 Azimute mit 13 verschiedenen Phasen, groesste Luecke 2,3 Grad. Schlechtestwert jetzt 1,96 % bei az111 |
| **Silhouette mass bei fester Aufloesung 1024 px** | viel feiner, als die Stufe je benutzt wird | Aufloesung = Koerpergroesse / (Umschaltweite x Pixelwinkel), gedeckelt auf 1280 px. L2→L3 wird bei 175 px gemessen |
| **Merge-Test verschmolz nie und verglich nie** | meldete nur `volume_sum` und die Huellenvereinigung nebeneinander | verschmilzt wirklich, vergleicht Volumen und Huellquader gegen die Teile (dV < 1e-9) |
| **Durchdringung wurde von nichts geprueft** | Kritiker fand 47 Koerperpaare mit geteiltem Innenvolumen | eigener paarweiser Test (Huellquader sieben, Schnittquader abtasten, beidseitiger Strahl-Einschluss). **0 ungewollte Paare** auf allen vier Stufen |

Der neue Durchdringungstest fand danach selbst, was in keiner Liste stand: Dachblech 4,8 mm im
Kopfwinkel · Bodenblech 53 Liter in der Ringmauer · Entlueftungsrohr im Dach · Stufe 0 im Beton ·
oberster Treppenpfosten und erster Podestpfosten am selben Ort · flaches Verstaerkungsblech auf einem
Kegel (34 mm Stich). Dazu die Sehnenregel: eine gerade Stufenkante ueber einem Polygon sticht 0,01 mm
durch — auch das gemessen und behoben.

### Die Wurzel

| Was | Runde 1 | Runde 2 |
|---|---|---|
| **[API 5.6.1.1 Fussnote 4]** — 3,2 m < D < 15 m: unterster Schuss ≥ 6 mm (1/4 in) | durchgehend 3/16 in | Schuss 1 = 1/4 in, Schuesse 2–4 = 3/16 in |
| **Windring-Lage** | 4,7268 m aus „Schale gleichdick" + 5.9.7.5 | transformierte Schale [5.9.7.2] → Soll 5,5021 m, Endlage **5,3923 m** [5.9.7.3.2], damit die Treppe nicht hindurchgeht |
| **Windring-Profil** | 100×100×8 **gesetzt** — steht nicht in Tabelle 5-20a | **100×75×7** (60,59 cm³ gegen Z_req 54,91), kleinstes Profil des Blocks „One Angle: Detail c" |
| **Windring-Form** | Ferse an der Schale, freier Schenkel nach oben — spiegelverkehrt | langer Schenkel waagerecht mit dem Ende an der Schale, freier Schenkel aussen nach unten [Bild 5-24 Detail c, **gerendert und abgelesen**] |
| **Ringmauer-Tiefe** | −0,15 m | **−0,60 m** [B.4.2.2: „0.6 m below the lowest adjacent finish grade"] |
| **Treppe** | 2R+r auf 610 geklemmt → 182,79/244,41 mm, keine Tabellenzeile | steilste OSHA-taugliche Zeile der T.5-19a (180/250/35°45′), daraus N=56 → **179,64/250,73 mm, 35,62°** |
| **Handlauf** | 210 mm Absatz am Podest | rampt ueber 3 Stufen auf Podesthoehe [T.5-18 Pkt.6 „without offset"] |
| **Podest** | nur Aussengelaender | zusaetzlich Stirngelaender am fernen Ende [T.5-17 Pkt.10]; die Innenseite ist die Tankwand, die Pkt.11 als schliessende Flaeche behandelt |
| **Anbauten** | sassen beim wahren Kreisradius, bei L3 181 mm frei vor der Wand | folgen dem gebauten Polygon (`poly_radius`), inklusive Sehnenkorrektur ueber ihre Winkelbreite |
| **L3** | 12 Segmente | **16** — die reparierte Silhouettenpruefung wies 12 als zu grob nach (2,42 % > 2 %) |

### Zwei `[SET]`, die in Wahrheit `[DOC]` waren

Beide standen bemasst in API 650; Runde 1 hatte sie als „nicht extrahierbar" abgetan.

- **Ringmauerhoehe 0,30 m** — Bild B-1, Aufriss. Ein Bild wird **gerendert** (`pdftoppm -r 170`) und
  abgelesen, nicht aus dem Textstrom gefischt. Das ist die eigentliche Lehre der Zeile.
- **Bodenblechueberstand** — [5.4.2] fordert **50 mm**, nicht 25 mm. Reiner Text; die Klausel war nur
  nie aufgeschlagen.

---

## Offen, gereiht

### 1 — Der Tank steht nackt in der Landschaft

Kein Auffangwall, keine Rohrleitung am Bodenstutzen, kein Schaumrohr, keine Beschriftung, kein
Gefahrgutschild. Groesster Abstand zwischen „richtiges Objekt" und „glaubwuerdige Szene".
*Schliessen:* Frage an den Eigner, ob Wanne und Verteiler zu diesem Asset gehoeren oder eigene
Eintraege sind — `doc/asset-inventory.md` kennt sie nicht.

### 2 — Keine Textur, und der Shader, der sie ersetzen soll, existiert nicht

Bewusste Entscheidung (Bandbreite ist der Mangel), aber der prozedurale Shader fuer Rost,
Nahtverfaerbung und Aufschrift ist nicht gebaut. Bis dahin ist der Tank gleichmaessig sauber.

### 3 — Kein Zerstoerungszustand

Comanche macht den Tank in 4 von 10 Missionen zum Ziel. Es gibt keine zerstoerte Fassung.

### 4 — Zoll oder Millimeter: 90 mm Unsicherheit in der Ringlage

Fussnote 4 nennt „6 mm (1/4 in.)". Note 3 derselben Tabelle fuehrt 6 mm als **Substitution** fuer
1/4 in, also ist 1/4 in die Grundgroesse — so ist gebaut. Mit 6 mm laege der Sollpunkt bei 5,4116
statt 5,5021 m; der Kritiker hat so gerechnet. **Das Profil aendert sich dadurch nicht** (beide Z_req
landen auf 100×75×7), die LAGE schon.
*Schliessen:* entscheiden, ob dieser Tank metrisch oder in Zoll beschafft ist — eine Bestellangabe,
keine Norm.

### 5 — Sechs gesetzte Masse

| Konstante | Wert | Warum ungedeckt |
|---|---|---|
| `kVentDia` / Zahl der Ventile | 200 mm / 1 | API Std 2000 (5.8.5.2) nicht gerechnet; kein Peilstutzen, keine Notentlueftung |
| `kShellManholeBolts` | 20 | T.5-5a liefert Lochkreis und Deckel, nicht die Schraubenzahl; 20 aus der Dachmannloch-Tabelle per Analogie |
| `kNozzleFlangeDia` | 279,4 mm | ASME B16.5 Klasse 150 NPS 6 — B16.5 nicht geholt |
| Treppenquerschnitte | 30 / 25 / 200 / 10 / 42 / 48 mm | API 650 bindet die Geometrie, nicht die Bauteilquerschnitte. 5.8.10 b nennt **PIP STF05501/05520/05521** — nicht beschafft |
| Schweissraupe | 3 × 14 mm | keine Quelle. Runde 1 hatte 5 × 60 mm, das war zu breit und zu flach |
| `kStairRailRampSteps` | 3 | die Rampe ist die richtige Antwort auf „without offset", ihre Laenge ist gesetzt |

### 6 — Materialwerte ohne Messung

Alle vier PBR-Saetze `[SET]`, kein Foto, keine Referenztafel. Der Anstrich wirkt im Beweisbild
ueberstrahlt.

### 7 — Umfangslagen willkuerlich

Mannloch 300°, Stutzen 345°, Dachmannloch 30°, Entlueftung 70°, Treppenfuss 130°. Der Treppenfuss
wurde gedreht, **damit die Seitenansicht des Beweisrenders die Treppe zeigt** — sachfremd.
*Schliessen:* als Parameter des Platzierers exportieren, mit der realen Regel (Mannloch bewusst nicht
unter der Treppe, Treppenfuss zum Zugangsweg).

### 8 — L3 behaelt Bauteile, die es nicht mehr aufloest

Der Kritiker: rund 50 % der L3-Dreiecke gehen an Teile, die auf ihrer Umschaltweite unter einem
Viertel Pixel liegen (Gelaender 888 Tri). Die Regel fragt, ob das VERLORENE unter ein Pixel faellt —
nie, ob das BEHALTENE noch aufloest. **Das Instrument existiert jetzt** (XOR bei der
Umschaltaufloesung), die Entscheidung ist nicht getroffen: wer Gelaender an L3 streicht, muss den
XOR-Wert danebenlegen. Nicht gemacht, weil Runde 2 die Tore hoeher gewichtet hat.

### 9 — Drei Luecken, die in den Pruefungen bleiben

- **T-Stoesse nur JE KOERPER.** Wo zwei Netze aneinanderstossen, prueft nichts.
- **Der Durchdringungstest ist eine Rasterschaetzung**, kein exakter Test: 10³ Proben im
  Schnittquader. Er FINDET zuverlaessig, aber die gemeldete cm³-Zahl traegt das Quantum des Rasters
  (in Runde 2 an einem Paar als 172 703 → 1 521 cm³ gesehen, als die Aufloesung stieg). Ein exakter
  Dreieck-Dreieck-Test waere die saubere Fassung.
- **Schweisspunkte sind per Liste erlaubt** (`kJoint`, Kappe 250 cm³ je Paar). Die Liste ist eine
  Behauptung ueber die Konstruktion; sie waechst still mit, wenn jemand sie erweitert statt die
  Geometrie zu bessern.

### 10 — Kein Dachtragwerk, kein Innenraum

Getragenes Kegeldach ohne Sparren und Mittelstuetze (5.10.4). Von aussen unsichtbar, aber das
Dachmannloch oeffnet auf nichts.

### 11 — Kein Koerper fuer die Physik, keine UV-Koordinaten

`doc/body-format.md` §1 will SEGMENT und CONTACT; die `.glb` traegt nur Sichtgeometrie. UVs fehlen
bewusst — wer je eine Textur will, muss zuerst abwickeln.
