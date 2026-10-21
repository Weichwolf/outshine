# Native Abhängigkeiten

Keine gebündelten Bibliotheksquellen. Systempakete bleiben hinter nativen Verträgen;
Installation ist noch keine Integration oder ein Performancebeweis.

| Bibliothek | Geprüfter lokaler Stand | Zuständiger WI | Verwendung |
|---|---|---|---|
| GEOS | 3.15.0 | [#8](https://github.com/Weichwolf/outshine/issues/8) | Polygontriangulation mit Höfen/Inseln über C-API; mindestens 3.10 |
| SFCGAL | 2.3.0 | [#48](https://github.com/Weichwolf/outshine/issues/48) | Native Walmdachpartition mit Quellkanten/Höfen über C-API; mindestens 2.3, Rendererübernahme folgt |
| OSQP | 1.0.0 | [#760](https://github.com/Weichwolf/outshine/issues/760) | Nativer physischer Wasserfit über C-API, Float64; Solver und vollständige Prüfungen nur beim Asset-Aufbau |
| SQLite | 3.54.0 | [#10](https://github.com/Weichwolf/outshine/issues/10) | Nativer Asset-Paketindex mit transaktionalen Metadaten und R*Tree |
| Zstandard | 1.5.7 | [#10](https://github.com/Weichwolf/outshine/issues/10) | Verlustfreie native Paketkompression; unveränderte Assetidentität und Ladeschranke |

Wartung geprüft am 2026-10-20: [GEOS](https://libgeos.org/usage/download/)
3.15.0 (2026-09), [SFCGAL](https://github.com/SFCGAL/SFCGAL) Codeänderungen 2026-10,
[OSQP](https://github.com/osqp/osqp/releases) 1.0.0 (2025-03), letzter geprüfter
Hauptzweig-Commit 2025-11, [SQLite](https://sqlite.org/changes.html) 3.54.1 (2026-10)
und [Zstd](https://github.com/facebook/zstd/releases) 1.5.7 (2025-02),
Hauptzweigänderungen 2026-10. Lokaler Paketstand und Upstream-Stand sind verschieden;
Commitaktivität allein beweist keine Fehlerfreiheit oder künftige Wartung.

## Weitere tatsächlich genutzte Bibliotheken

| Bibliothek | Nutzung und Entscheidung |
|---|---|
| SDL3 | Fenster, Eingabe, Audio und Plattformdienste; bleibt. Native Metal-/Vulkan-Ausführung ersetzt SDL3_GPU. |
| SDL3_image / SDL3_ttf | Bildcodecs / Schrift; bestehende Funktionen erhalten, Bildpfade erst nach Nutzerinventur zusammenführen. |
| libcurl | HTTP/TLS und Transfers; bleibt, kein eigener Netzwerk-/Zertifikatsstack. |
| zlib | PNG und bestehende Cachepfade; bleibt für diese Formate. |
| LZ4 | Bestehender ArtifactStore; Vereinheitlichung mit Zstd erst mit lesbarer Altbestandsmigration. |
| libtiff / libwebp | Copernicus-Raster / Terrarium-Höhen; Quellformatdecoder, kein eigener Codec-Eigenbau. |
| SPIRV-Cross | Native Shaderübersetzung/Reflexion; bleibt. glslang ist ein Buildwerkzeug. |
| nlohmann/json | Nur Benchmarkberichte im nativen Integrationszweig; eigenen JSON-Ausgabepfad als Ersatz prüfen. |

Die Installation oben betrifft die numerischen/Assetpakete, nicht das gesamte Toolchain-
Setup. Plattform-SDK, SDL/Codecs/Shaderwerkzeuge stehen in den Buildprofilen und
`src/client/README.md`. Der alte Master-Renderpfad benötigt bis zur nativen Aufnahme
zusätzlich SDL3_shadercross; der native Integrationszweig benötigt es nicht.

## Installation

```sh
brew install ccache geos sfcgal osqp sqlite zstd
```

Debian/Ubuntu, abhängig von der Distributionsversion:

```sh
sudo apt install ccache libgeos-dev libsfcgal-dev libsqlite3-dev libzstd-dev
```

SFCGAL benötigt mindestens 2.3; die Paketversion vor dem Build mit `pkg-config --modversion sfcgal` prüfen.
GEOS, SFCGAL, SQLite und Zstandard liefern pkg-config-Metadaten.
Der Wasser-Höhenfit benötigt OSQP 1.x mit Float64. Ohne OSQP-pkg-config-Metadaten
werden die üblichen Systempräfixe geprüft; einen eigenen Installationspräfix setzt
`OSQP_PREFIX`. Die Verfügbarkeit eines OSQP-Entwicklungspakets hängt von der Distribution ab.

Meshoptimizer ist keine Produktabhängigkeit. Einzelne optionale Forschungsvergleiche
nutzen eine separat installierte Bibliothek; Aufrufe und historische Versionen stehen
bei den Experimenten. Outshine pflegt dafür kein eigenes Paketrezept.

## Reduktion und gemeinsame Mathematik

Entscheidung und Umsetzung: [#954](https://github.com/Weichwolf/outshine/issues/954).
simdjson, pugixml und Bullet sind ungenutzt und keine Installationsanforderungen.
JSON/XML und die vorhandene Physik bleiben erhalten. Forschungspakete sind keine
nativen Produktabhängigkeiten; installierte Systempakete werden nicht pauschal entfernt.

Eigen ist durch den gemeinsamen CSC-Baustein `src/base/math/SparseMatrix.h` ersetzt:
sortierte Tripel/zusammengefasste Duplikate, obere Dreiecksmatrix und gewöhnliche/
transponierte Matrix-Vektor-Produkte. OSQP erhält die eigenen Arrays ohne Matrixkopie.
GeographicLib ist durch `src/base/geo/Geocentric.h` ersetzt. Wasser, Gelände und Kamera
nutzen dieselbe Vorwärts-/Rücktransformation und dieselben abgeleiteten WGS84-Konstanten.
Die Inverse löst die Ellipsoidnormalenbedingung mit begrenztem Newton-Verfahren;
Höhe folgt aus der Normalprojektion statt einer polnahen Division durch cos(Breite).
Nichtendliche, zentral mehrdeutige oder nicht konvergierte Eingaben werden abgelehnt.
Der Anwendungsbereich ist Erdoberfläche/Untergrund bis Orbit, keine inverse Vermessung
des Erdkerns. Alte geodätische Referenzkorpora bleiben Testdaten, keine Runtime-Library.

SFCGAL bleibt nur für noch nicht ersetzte komplexe Walmdachpartitionen. Rechteckpläne
werden einschließlich Quellkantenvorbereitung ohne Bibliotheksaufruf erzeugt. Die geprüfte
Python-Konturreferenz für konkave Grundrisse und Höfe liegt vor; deren native Übernahme
und anschließende vollständige Entfernung besitzt #955. Bis dahin bleibt der komplexe Pfad.
Ein kleiner gemeinsamer Geometriekern darf weitere konkrete Asset-Rezepte bedienen;
parallele Rezeptauswertung/Tessellierung und verzweigte CPU-Topologie bleiben getrennt.
GPU-Eignung ist zu prüfen, kein pauschaler GPU-Port aller Solver.
Das Paket zieht CGAL, Boost, GMP und MPFR mit; CGAL benötigt wiederum Eigen. Ein
entfernter direkter Eigen-Include bedeutet deshalb noch kein Eigen-freies System.
GEOS bleibt für robuste Polygonoperationen, OSQP für den beschränkten Wasser-QP.
Weitere Kandidaten: nlohmann/json für native Benchmarkberichte, LZ4/Zstd und Bildcodecs.
Bestehende Formate und Inhalte dürfen bei einer Reduktion nicht unlesbar werden.

Python-Experimente prüfen native Umsetzungen. Shapely/GEOS und Python-OSQP/native OSQP
teilen jedoch Kerne; analytische Fälle und unabhängige Invarianten ergänzen den Vergleich.

## SQLite oder DuckDB

SQLite bleibt der transaktionale Assetindex mit gezielten Schlüssel-/Raumabfragen und
versionierten, bereits Zstd-komprimierten Nutzdatenpaketen. DuckDBs Spaltenkompression
bietet für dieses Layout keinen belegten Zusatzgewinn; größere Einsparungen würden
einen anderen Datenzuschnitt voraussetzen. Dateigröße, RAM und Decode-Arbeit getrennt
messen. DuckDB ist ein möglicher optionaler Analysehelfer, keine zweite Runtime-Datenbank.
Grundlagen: [DuckDB-Speicherformat](https://duckdb.org/docs/current/internals/storage),
[DuckDB-Ausführung](https://duckdb.org/docs/current/guides/performance/how_to_tune_workloads),
[SQLite R*Tree](https://sqlite.org/rtree.html).

Der integrierte native Assetcache benötigt SQLite und Zstandard auch im Runtime-Link. Statische
Nutzer von `liboutshine.a` binden zusätzlich `pkg-config --libs sqlite3 libzstd` ein.
Der Building-Generator benötigt beim statischen Link zusätzlich `pkg-config --libs sfcgal`.

Native Builds benötigen `ccache`. Der Compiler-Cache liegt standardmäßig unter
`${TMPDIR:-/tmp}/outshine-compiler-cache`, auf 1 GiB begrenzt; bestehende
`CCACHE_DIR`/`CCACHE_MAXSIZE`-Vorgaben gehen vor. Er enthält ausschließlich
wiederherstellbare Compilerprodukte, keine Weltquellen oder Game-Assets.
