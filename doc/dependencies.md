# Native Abhängigkeiten

Keine gebündelten Bibliotheksquellen. Systempakete bleiben hinter nativen Verträgen;
Installation ist noch keine Integration oder ein Performancebeweis.

| Bibliothek | Geprüfter lokaler Stand | Zuständiger WI | Verwendung |
|---|---|---|---|
| GEOS | 3.15.0 | [#8](https://github.com/Weichwolf/outshine/issues/8) | Polygonoperationen, Triangulation und Dachkonturen über C-API; mindestens 3.12 |
| OSQP | 1.0.0 | [#760](https://github.com/Weichwolf/outshine/issues/760) | Nativer physischer Wasserfit über C-API, Float64; Solver und vollständige Prüfungen nur beim Asset-Aufbau |
| HiGHS | 1.15.1 | [#1003](https://github.com/Weichwolf/outshine/issues/1003) | Begrenzter nativer Treppen-/Wegprofilfit über C-API; ursprüngliche Bedingungen werden nachgerechnet, Weltanbindung noch offen |
| SQLite | 3.54.0 | [#10](https://github.com/Weichwolf/outshine/issues/10) | Nativer Asset-Paketindex mit transaktionalen Metadaten und R*Tree |
| Zstandard | 1.5.7 | [#10](https://github.com/Weichwolf/outshine/issues/10) | Verlustfreie native Paketkompression; unveränderte Assetidentität und Ladeschranke |
| OpenSSL libcrypto | 3.5.7 | [#1098](https://github.com/Weichwolf/outshine/issues/1098) | Linux-Einmal-Digests für SHA-256; nur tatsächliche SHA-Verbraucher, Apple verwendet CommonCrypto |

Wartung geprüft am 2026-10-20: [GEOS](https://libgeos.org/usage/download/)
3.15.0 (2026-09),
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
| libtiff / libwebp | Copernicus-Raster / Terrarium-Höhen; Quellformatdecoder, kein eigener Codec-Eigenbau. |
| SPIRV-Cross | Native Shaderübersetzung/Reflexion; bleibt. glslang ist ein Buildwerkzeug. |
| Vulkan-Header / Loader | Linux-Backend; Header müssen `VK_KHR_internally_synchronized_queues` enthalten (geprüft mit 1.4.363). Runtimefähigkeiten werden unabhängig am Gerät geprüft. |
| QuickJS-NG | JavaScript-Semantik und VM statt eigenem Parser/Interpreter; MIT, keine transitiven Laufzeitpakete. Speicher, Stack und kooperative Ausführung begrenzt; kein JS-Datei-/Netzwerkmodul eingebunden. |
| nlohmann/json | Native Benchmarkberichte; vorhandene Systembibliothek weiterverwenden. JSON-Eingaben verwenden derzeit den eigenen Parser unter `src/base/format/`; dessen Ablösung braucht die Vertragsprüfung aus #1320. |

Die Installation oben betrifft die numerischen/Assetpakete, nicht das gesamte Toolchain-
Setup. Plattform-SDK, SDL/Codecs/Shaderwerkzeuge stehen in den Buildprofilen und
`src/app/README.md`. Metal und Vulkan verwenden die nativen Backendverträge;
SDL3_shadercross ist keine Produkt- oder Buildabhängigkeit.

## Installation

```sh
brew install ccache geos osqp highs sqlite zstd quickjs-ng nlohmann-json
```

Debian/Ubuntu, abhängig von der Distributionsversion:

```sh
sudo apt install ccache libgeos-dev libsqlite3-dev libzstd-dev libssl-dev nlohmann-json3-dev
```

Linux verwendet für Einmal-SHA-256 die Systembibliothek libcrypto; `libssl-dev`
liefert Header und `libcrypto.pc`. `pkg-config --modversion libcrypto` prüft den Stand.
Prefix-/Append-Zustände und Cacheformate bleiben erhalten; isolierte native Render-/
Stage-Profile benötigen libcrypto nicht. Apple bleibt bei CommonCrypto ohne OpenSSL-Paket.

Linux prüft beim CMake-Konfigurieren die Vulkan-Featurestruktur und Queueflags für
die Ein-Queue-Präsentation der Iris Xe. Header 1.4.309 enthalten diesen Vertrag
nicht; ein erfolgreicher Offscreen-Build ersetzt die benötigte Fensterfähigkeit
nicht. CMake weist ungeeignete Header vor dem Produktbau mit einer Fehlermeldung
zurück. Die Prüfung wird auch nach einem SDK-Wechsel erneut ausgeführt;
Änderungen der gewählten Headerdatei lösen beim Ninja-Build die Konfiguration neu aus.
Ein separates Header-SDK lässt sich mit `-DVulkan_INCLUDE_DIR=<SDK>/include`
wählen; bei diesem Pfad liegt `vulkan/vulkan_core.h`. Loader und Treiber werden
weiter separat gefunden, keine Fremdheader werden im Repository gebündelt.
Die geprüfte Kombination ist Header 1.4.363, Loader 1.4.309 und Mesa 26.1.6.
Der Treiber muss die benötigte Erweiterung und ihr Feature tatsächlich anbieten;
fehlende Runtimefähigkeit bleibt ein erklärter Unsupported-Fehler. Apple/Metal
benötigt diesen Vulkan-Vertrag nicht. Entscheidung und Gegenproben: [#1346](https://github.com/Weichwolf/outshine/issues/1346).

Scripting benötigt QuickJS-NG (geprüft mit 0.17.0) mit `qjsConfig.cmake` und dem
importierten CMake-Target `qjs`. Paketverfügbarkeit unter Linux distributionsabhängig;
Systempräfix bei Bedarf mit `CMAKE_PREFIX_PATH` angeben. Bellards QuickJS ist kein
stillschweigender ABI-Ersatz. Nur Scripting bindet die Bibliothek direkt ein.

GEOS benötigt mindestens 3.12 (`GEOSOrientPolygons`); die Paketversion mit
`pkg-config --modversion geos` prüfen. GEOS, SQLite und Zstandard liefern pkg-config-Metadaten.
Der Wasser-Höhenfit benötigt OSQP 1.x mit Float64. Ohne OSQP-pkg-config-Metadaten
werden die üblichen Systempräfixe geprüft; einen eigenen Installationspräfix setzt
`OSQP_PREFIX`. Die Verfügbarkeit eines OSQP-Entwicklungspakets hängt von der Distribution ab.
HiGHS benötigt Systembibliothek und `highs.pc`; `pkg-config --modversion highs` prüft den Stand.
Die Verfügbarkeit eines HiGHS-Entwicklungspakets hängt ebenfalls von der Distribution ab.
Der Profilfit verwendet einen Thread und begrenzte Dualsimplex-Iterationen/Laufzeit;
Abbruch und Originalmatrixprüfung bleiben im nativen Besitzer. Die vorhandene
OSQP-Variante wurde am identischen lokalen LP geprüft und erreichte das Laufzeitlimit
mit verletzten Bedingungen; andere Skalierungen wurden damit nicht ausgeschlossen.

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

SFCGAL ist durch den begrenzten nativen Konturpfad ersetzt (#955). Rechtecke bleiben
analytisch; konkave Grundrisse und Höfe verwenden gerichtete Konturereignisse,
quelleneigene überstrichene Facetten und gemeinsame Firstknoten. GEOS übernimmt
Polygonoperationen. Originaltraufen, Ebenen, Deckung, Kapazität und Deadline werden
weiter geprüft. Die Dachimplementierung benötigt weder SFCGAL noch dessen
transitive CGAL-/Boost-/GMP-/MPFR-/Eigen-Abhängigkeiten. Historische Python-
Vergleiche behalten ihre Herkunft, sind keine Produktabhängigkeiten.
Ein gemeinsamer Geometriekern wächst nur mit konkreten weiteren Verbrauchern;
GPU-Eignung fester Rezepte und verzweigte CPU-Topologie bleiben getrennte Fragen.
GEOS bleibt für robuste Polygonoperationen, OSQP für den beschränkten Wasser-QP.
Der frühere ArtifactStore samt LZ4-Container und optionalem Taskpfad ist ohne
Produktkonsumenten entfernt (#1032). Aktive Gebäudeprodukte verwenden AssetCache
mit SQLite/Zstd; ihre Formate und gespeicherten Pakete bleiben unverändert.
JSON vereinheitlichen, soweit die tatsächlichen Eingabeverträge erhalten bleiben:
doppelte Schlüssel erkennen, Quelltextausschnitte/Schlüsselreihenfolge erhalten,
Zahlengrenzen und maximale Verschachtelung prüfen, Fehler ohne Exceptions liefern.
Ein DOM-Austausch allein belegt das nicht; bestehende atomare Fälle und echte
Szenario-/Rezeptverbraucher gegenprüfen (#1320). Weitere Reduktionskandidaten: Bildcodecs.
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
Die Polygon- und Dachverfahren benötigen beim statischen Link `pkg-config --libs geos`.

Native Builds benötigen `ccache`. Der Compiler-Cache liegt standardmäßig unter
`${TMPDIR:-/tmp}/outshine-compiler-cache`, auf 1 GiB begrenzt; bestehende
`CCACHE_DIR`/`CCACHE_MAXSIZE`-Vorgaben gehen vor. Er enthält ausschließlich
wiederherstellbare Compilerprodukte, keine Weltquellen oder Game-Assets.
