# Bewährte Bibliotheken

Keine gebündelten Bibliotheksquellen. Systempakete bleiben hinter nativen Verträgen;
Installation ist noch keine Integration oder ein Performancebeweis.

| Bibliothek | Geprüfter lokaler Stand | Zuständiger WI | Verwendung |
|---|---|---|---|
| GEOS | 3.15.0 | [2173](../board/2173_osm_semantics_reach_generators_and_unknowns_remain_explicit.md) | Polygontriangulation mit Höfen/Inseln über C-API; mindestens 3.10 |
| meshoptimizer | 1.3 | [2336](../board/2336_one_world_refines_seamlessly_from_orbit_to_ground_detail.md) | Geplant: native Index-/Vertexbuffer, begrenzte Simplifizierung |
| simdjson | 4.6.11 | [2188](../board/2188_engine_contracts_will_match_the_sandbox_architecture.md) | Geplant: gemeinsamer JSON-Decoder |
| pugixml | 1.16 | [2188](../board/2188_engine_contracts_will_match_the_sandbox_architecture.md) | Geplant: gemeinsamer XML-Decoder |
| Eigen | 5.0.1 | [2136](../board/2136_a_thousand_minds_walk_the_world_inside_the_frame.md) | Geplant: lineare Berechnungen, keine Änderung der GPU-ABI |
| Bullet | 3.25 | [2136](../board/2136_a_thousand_minds_walk_the_world_inside_the_frame.md) | Geplant: Kontakte, Starrkörper und Gelenke |
| SQLite | 3.54.0 | [2280](../board/2280_ready_assets_load_from_a_spatial_cache.md) | Nativer Asset-Paketindex mit transaktionalen Metadaten und R*Tree |
| Zstandard | 1.5.7 | [2280](../board/2280_ready_assets_load_from_a_spatial_cache.md) | Verlustfreie native Paketkompression; unveränderte Assetidentität und Ladeschranke |

## Installation

```sh
brew install geos simdjson pugixml eigen bullet sqlite zstd
```

meshoptimizer fehlt in Homebrew-Core. Das eigene [Paketrezept](../Formula/meshoptimizer.rb)
baut den offiziellen Release mit SHA-256-Prüfung, ohne Bibliotheksquellen im Repository.

```sh
brew tap cosmo/outshine git@github.com:Weichwolf/outshine.git
brew install cosmo/outshine/meshoptimizer
brew test cosmo/outshine/meshoptimizer
```

Der Tap verwendet das bestehende Outshine-Remote; `brew update` übernimmt neue Paketrezepte.
Bei einem Bibliotheksupdate Release/Hash und ABI prüfen, Paket bauen und Engine-Gates ausführen.

Debian/Ubuntu, abhängig von der Distributionsversion:

```sh
sudo apt install libgeos-dev libmeshoptimizer-dev libsimdjson-dev libpugixml-dev libeigen3-dev libbullet-dev libsqlite3-dev libzstd-dev
```

GEOS, SQLite, Zstandard, simdjson, pugixml, Eigen und Bullet liefern pkg-config-Metadaten. meshoptimizer liefert
einen CMake-Vertrag (`meshoptimizer::meshoptimizer`); keine erfundene pkg-config-Abhängigkeit.
Optionale Pakete werden erst beim jeweiligen Feature an den Build angeschlossen.

Der integrierte native Assetcache benötigt SQLite und Zstandard auch im Runtime-Link. Statische
Nutzer von `liboutshine.a` binden zusätzlich `pkg-config --libs sqlite3 libzstd` ein.
