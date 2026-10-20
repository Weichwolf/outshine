# Native Abhängigkeiten

Keine gebündelten Bibliotheksquellen. Systempakete bleiben hinter nativen Verträgen;
Installation ist noch keine Integration oder ein Performancebeweis.

| Bibliothek | Geprüfter lokaler Stand | Zuständiger WI | Verwendung |
|---|---|---|---|
| GEOS | 3.15.0 | [#8](https://github.com/Weichwolf/outshine/issues/8) | Polygontriangulation mit Höfen/Inseln über C-API; mindestens 3.10 |
| GeographicLib | 2.7 | [#760](https://github.com/Weichwolf/outshine/issues/760) | Robuste geozentrische Rücktransformation für physische Wasserhöhen und Normalenhüllen |
| SFCGAL | 2.3.0 | [#48](https://github.com/Weichwolf/outshine/issues/48) | Native Walmdachpartition mit Quellkanten/Höfen über C-API; mindestens 2.3, Rendererübernahme folgt |
| OSQP | 1.0.0 | [#760](https://github.com/Weichwolf/outshine/issues/760) | Nativer physischer Wasserfit über C-API, Float64; Solver und vollständige Prüfungen nur beim Asset-Aufbau |
| SQLite | 3.54.0 | [#10](https://github.com/Weichwolf/outshine/issues/10) | Nativer Asset-Paketindex mit transaktionalen Metadaten und R*Tree |
| Zstandard | 1.5.7 | [#10](https://github.com/Weichwolf/outshine/issues/10) | Verlustfreie native Paketkompression; unveränderte Assetidentität und Ladeschranke |

## Installation

```sh
brew install ccache geos geographiclib sfcgal osqp sqlite zstd
```

Debian/Ubuntu, abhängig von der Distributionsversion:

```sh
sudo apt install ccache libgeos-dev libgeographiclib-dev libsfcgal-dev libsqlite3-dev libzstd-dev
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
GeographicLib
liefert nur die geozentrische Rücktransformation; eigene Geodäsiepfade werden samt
Ellipsoid und Fehlergrenzen vereinheitlicht. Dieser Ersatz benötigt Referenz-
und Kostenvergleiche, bevor seine Buildabhängigkeit entfällt.

SFCGAL liefert ausschließlich die Walmdachpartition und soll entfallen. Research baut
zuerst einen geprüften Ersatz für konkave Grundrisse und Höfe; bis dahin bleibt der Pfad.
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
