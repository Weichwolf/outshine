# Lizenz

FlightBox steht unter der **GNU General Public License, Version 2 oder später**
(`GPL-2.0-or-later`). Der vollständige Lizenztext liegt in [`LICENSE`](LICENSE).

```
Copyright (C) 2026 Weichwolf

Dieses Programm ist freie Software: Sie können es unter den Bedingungen der
GNU General Public License, Version 2 oder (nach Ihrer Wahl) einer späteren
Version, weitergeben und/oder verändern.

Die Veröffentlichung erfolgt in der Hoffnung, dass es nützlich ist, jedoch
OHNE JEDE GEWÄHRLEISTUNG — sogar ohne die implizite Gewährleistung der
MARKTGÄNGIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
```

Erfasst ist der gesamte eigene Quellcode unter `sim/src/`, `tiles/`, `sim/tools/`, `sim/missions/` und
die eigene Dokumentation unter `doc/flightbox/` sowie `doc/mission-format.md`. Einzelne Dateien tragen
keinen eigenen Lizenzkopf; diese Datei gilt für den Baum, soweit unten nichts anderes steht.

## Warum GPL-2.0-or-later und nicht etwas anderes

Die Wahl folgt aus den Abhängigkeiten, nicht aus einer Haltung:

| Grund | Sachverhalt |
|---|---|
| **JSBSim ist LGPL 2.1 und wird STATISCH gelinkt** | `libJSBSim.a` geht in jedes Binary, in WASM wird es einkompiliert. Unter einer permissiven Lizenz verlangte die LGPL, mit jedem Binary relinkbare Objektdateien mitzuliefern. Unter Copyleft entfällt diese Pflicht. |
| **Das f16-Modell ist GPL mit UNBESTIMMTER Version** | `sim/vendor/jsbsim/aircraft/f16/f16.xml` deklariert `licenseName="GPL"` und verweist auf eine generische gnu.org-URL. Ist es GPL-2-only, wäre GPL-3 für uns unverträglich; ist es GPL-3, deckt „or later" es ab. **Nur `GPL-2.0-or-later` überlebt beide Lesarten.** |
| **Das Modell wird MITGELIEFERT, nicht nur referenziert** | Der WASM-Build backt es per `--embed-file` in `gpu.wasm` hinein. |
| **Ökosystem** | JSBSim (LGPL), FlightGear und dessen Flugzeugbestand (GPL-2+). Ein künftiger Import eines fremden JSBSim-Modells bleibt damit möglich. Apache-2.0 wäre mit GPL-2 unverträglich gewesen. |

Das Copyright verbleibt beim Autor; eine Doppellizenzierung bleibt jederzeit möglich.

## Fremdanteile

Diese Bestandteile stehen unter ihrer eigenen Lizenz und werden davon nicht berührt.

### Code

| Komponente | Ort | Lizenz |
|---|---|---|
| JSBSim | `sim/vendor/jsbsim` (Submodul) | LGPL 2.1 |
| Dawn / Tint | `sim/vendor/dawn` | BSD-3-Clause |
| emdawnwebgpu | `sim/vendor/emdawnwebgpu_pkg` | BSD-3-Clause |

### Flugzeug- und Waffenmodelle

Aircraft-XML trägt eine **eigene Lizenz je Datei**, deklariert im `<fileheader>` des Modells.

| Modell | Herkunft | Lizenz |
|---|---|---|
| F-16 (`aircraft/f16`) | JSBSim-Submodul, Erik Hofman u.a. | GPL (Version nicht spezifiziert) |
| Mk-82 (`aircraft/mk82`) | JSBSim-Submodul | s. `<fileheader>` |
| AIM-120 (`sim/assets/aircraft/aim120`) | **FlightBox-eigen** | GPL-2.0-or-later |

### Assets

| Asset | Ort | Lizenz |
|---|---|---|
| B612 Mono (Airbus-Cockpit-Typeface), gebacken nach `FBHudFontRom.h` | `sim/src/render/` | SIL OFL 1.1 — Reserved Font Name beachten |
| Mondtextur (NASA/GSFC SVS, CGI Moon Kit, LROC WAC Albedo) | `sim/web/moon.jpg` | public domain |
| Sternkatalog (HYG Database, Hipparcos-abgeleitet) | — | CC-BY-SA 4.0 |
| Eingebackenes Schweiz-DEM | `sim/assets/swiss-dem-90m.bin` | abgeleitet aus Copernicus DEM, s.u. |

### Zur Laufzeit geladene Daten

Diese Daten sind **nicht Teil dieses Repositories**; der Simulator lädt sie on-demand. Die
Attributionspflicht trifft den Betreiber einer Instanz.

| Quelle | Lizenz / Bedingungen |
|---|---|
| Kartendaten OpenStreetMap (Shortbread-Vektorkacheln) | © OpenStreetMap contributors, ODbL |
| Geländehöhe Copernicus DEM (Terrarium-kodiert) | Copernicus-Nutzungsbedingungen |
| Luftbilder Esri World Imagery | © Esri und Datenlieferanten — **Nutzungsbedingungen ungeprüft**, s. `doc/flightbox/TODO.md` |

## Nicht erfasst

`doc/f16/` enthält Destillate zweier **kommerzieller** Handbücher (DCS F-16C Viper Guide; DCS F-16C
Early Access Guide, Eagle Dynamics). Die PDFs selbst sind über `doc/.gitignore` ausgeschlossen und
werden nicht verbreitet. Die Destillate stehen **nicht** unter der GPL und sind auch nicht als eigenes
Werk lizenzierbar, soweit sie Zahlen, Tabellen und Ablaufbeschreibungen der Vorlagen wiedergeben. Ihre
rechtliche Einordnung ist offen und in `doc/flightbox/TODO.md` als solche vermerkt.

`temp/` ist Migrationsgut der Vor-Architektur und kein gepflegter Bestandteil.
