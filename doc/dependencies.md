# Bewährte Bibliotheken

Keine gebündelten Bibliotheksquellen. Systempakete bleiben hinter nativen Verträgen;
Installation ist noch keine Integration oder ein Performancebeweis.

| Bibliothek | Geprüfter lokaler Stand | Zuständiger WI | Verwendung |
|---|---|---|---|
| SDL3 | 3.2.28 (isolierter Linux-Build) | [#237](https://github.com/Weichwolf/outshine/issues/237) | Acht Farbziele für native Render-/Impostorpässe; mindestens 3.2.24, ältere Runtime wird vor Geräteanlage verweigert |
| GEOS | 3.15.0 | [#8](https://github.com/Weichwolf/outshine/issues/8) | Polygontriangulation mit Höfen/Inseln über C-API; mindestens 3.10 |
| SFCGAL | 2.3.0 | [#48](https://github.com/Weichwolf/outshine/issues/48) | Native Walmdachpartition mit Quellkanten/Höfen über C-API; mindestens 2.3, Rendererübernahme folgt |
| meshoptimizer | 1.3 | [#12](https://github.com/Weichwolf/outshine/issues/12) | Geplant: native Index-/Vertexbuffer, begrenzte Simplifizierung |
| simdjson | 4.6.11 | [#9](https://github.com/Weichwolf/outshine/issues/9) | Geplant: gemeinsamer JSON-Decoder |
| pugixml | 1.16 | [#9](https://github.com/Weichwolf/outshine/issues/9) | Geplant: gemeinsamer XML-Decoder |
| Eigen | 5.0.1 | [#2](https://github.com/Weichwolf/outshine/issues/2) | Geplant: lineare Berechnungen, keine Änderung der GPU-ABI |
| Bullet | 3.25 | [#2](https://github.com/Weichwolf/outshine/issues/2) | Geplant: Kontakte, Starrkörper und Gelenke |
| SQLite | 3.54.0 | [#10](https://github.com/Weichwolf/outshine/issues/10) | Nativer Asset-Paketindex mit transaktionalen Metadaten und R*Tree |
| Zstandard | 1.5.7 | [#10](https://github.com/Weichwolf/outshine/issues/10) | Verlustfreie native Paketkompression; unveränderte Assetidentität und Ladeschranke |

## Installation

SDL3 benötigt mindestens 3.2.24 für acht Farbziele. Header und geladene Runtime
prüfen: ältere 3.2-Versionen können zwar linken, unterstützen aber nur vier Ziele.
`pkg-config --modversion sdl3` allein belegt nicht die zur Laufzeit geladene Bibliothek.

```sh
brew install geos sfcgal simdjson pugixml eigen bullet sqlite zstd
```

meshoptimizer fehlt in Homebrew-Core. Das eigene [Paketrezept](../lib/Formula/meshoptimizer.rb)
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
sudo apt install libgeos-dev libsfcgal-dev libmeshoptimizer-dev libsimdjson-dev libpugixml-dev libeigen3-dev libbullet-dev libsqlite3-dev libzstd-dev
```

SFCGAL benötigt mindestens 2.3; die Paketversion vor dem Build mit `pkg-config --modversion sfcgal` prüfen.
GEOS, SFCGAL, SQLite, Zstandard, simdjson, pugixml, Eigen und Bullet liefern pkg-config-Metadaten. meshoptimizer liefert
einen CMake-Vertrag (`meshoptimizer::meshoptimizer`); keine erfundene pkg-config-Abhängigkeit.
Optionale Pakete werden erst beim jeweiligen Feature an den Build angeschlossen.

Der integrierte native Assetcache benötigt SQLite und Zstandard auch im Runtime-Link. Statische
Nutzer von `liboutshine.a` binden zusätzlich `pkg-config --libs sqlite3 libzstd` ein.
Der Building-Generator benötigt beim statischen Link zusätzlich `pkg-config --libs sfcgal`.
