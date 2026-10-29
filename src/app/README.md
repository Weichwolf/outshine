# Szenarien

`outshine run [options] <scenario> [name]`

`scenario` ist eine XML-/JSON-Datei oder ein vollständiges JSON-Objekt als Argument.
JSON verwendet die bestehenden Szenariofelder: skalare Werte entsprechen Attributen,
Objekte Sektionen, Objektarrays wiederholten Kindern. Skalare Arrays ergeben Vektoren.
Boolesche Werte sind `true`/`false`; Feldnamen und Validierung bleiben identisch zu XML.

```sh
build/outshine run --offline --view station /path/to/world.scenario
build/outshine run --scenario-overrides '{"world":{"vegetation":false}}' /path/to/world.scenario
```

`--scenario-overrides <JSON object>` gilt für `run`, `measures` und `benchmark`.
Objekte ergänzen vorhandene Felder, Arrays ersetzen Sammlungen, `null` entfernt Felder.
Overrides bestimmen zuerst die Layerwahl und gewinnen anschließend gegenüber geladenen
Layern. Relative Layerpfade beziehen sich bei Dateien auf deren Verzeichnis, bei Inline-JSON
auf das Verzeichnis der mitgelieferten Daten. Eingabe, Overrides und geladene Layer zusammen: maximal 16 MiB.
Unbekannte Felder, doppelte Schlüssel und ungültige Werte werden abgewiesen.
Szenarioinhalte bekommen keine einzelnen CLI-Schalter.

Benannte Bildprüfungen und ihre Kamerakataloge gehören zum
[Testtreiber](../../test/outshine/data/places/README.md), nicht zur Anwendung.

# Hardwaremessung

```sh
build/outshine benchmark
build/outshine benchmark --output /tmp/outshine-calibration.json
MTL_DEBUG_LAYER=1 build/outshine benchmark --gpu-validation --output /tmp/outshine-calibration-validated.json
```

Der explizite Befehl misst die festen CPU-, GPU- und Speicherarbeitsmengen der Version 2.
Metal benötigt `MTL_DEBUG_LAYER=1` vor Prozessstart; Vulkan ignoriert diese Umgebungsvariable.
Er benötigt weder Szenario noch Netzwerk, Python oder Compiler zur Laufzeit. Ein Ausgabepfad
allein wählt dieselbe synthetische Messung. Ohne Pfad schreibt er JSON nach stdout; eine Datei
wird erst nach vollständiger Ergebnisprüfung atomar ersetzt. Fehler lassen alte Ergebnisse erhalten.

Jede Arbeitsmenge hat einen Vorbereitungslauf und fünf gemessene Stichproben. CPU-FMA nutzt
denselben Gesamtumfang mit 1/2/4/6 Rechenarbeitern; das Betriebssystem wählt die Kerne.
Die JSON-Daten nennen Gerät, tatsächliche Compilerflags, Buildstand, Hostlast und Validierung.
GPU-Zeitstempel, Host-Encoding, Einreichung und Warten bleiben getrennt. Fehlende GPU-Zeiten
stehen als null im Ergebnis. Logische Bytes und Operationen sind keine physische DRAM-Bandbreite
oder zugesicherte Hardwareleistung. Validierte Läufe getrennt von Vergleichsmessungen auswerten.

# Szenariomessung

```sh
build/outshine benchmark --scenario /path/to/world.json --view station --frames 60 \
  --angular-velocity 0,-360,0 --offline \
  --output /tmp/outshine-scene.json --image /tmp/outshine-scene.png
```

Szenario-Dateien und Inline-JSON verwenden dieselbe Aufbereitung wie `run`.
`--scenario-overrides` ergänzt generische Szenariofelder; Auflösung, Inhalte und Ziel-fps
stehen im Szenario. Ohne deklarierte Bildgröße gilt 1280×720. `--frames` fordert exakt
1..1.000.000 Frames; diese Speichergrenze beschränkt keine normale Szenario-Wiedergabe.
`--linear-velocity x,y,z` bezeichnet Weltmeter pro Sekunde, `--angular-velocity x,y,z`
einen rechtshändigen Weltachsenvektor in Grad pro Sekunde. Geografische Weltachsen sind
Ost/Oben/Süd. Die Kamera folgt der Startpose bei `(i+1)/fps`; ihre Linse bleibt erhalten.

Benchmarks laufen immer mit maximalem Durchsatz ohne künstliche FPS-Wartezeit.
Die Ziel-fps bestimmen Simulationsschritt und Budget; Framezahl, Kamera und Linse bleiben
explizit. Berichtversion2 kennzeichnet `profile.pacing` als `unpaced` und die Messphase
als `unpaced_window`. `wall_throughput_fps` zählt vollständig abgearbeitete Frames pro
Fensterzeit einschließlich abschließendem GPU-Warten. Das gilt auch für Places-Messungen;
deren Ziel-fps-Sekunde bezeichnet exakt diese Zahl Simulationsschritte.

Fertige gemeinsame Overrides unter `data/profiles/` bieten720p/1080p/1440p/2160p,
jeweils30oder60fps, z.B. `720p30.json` oder `2160p60.json`. Derselbe Inhalt gilt für
`run` und `benchmark`; im laufenden Szenario gilt die Zielrate, im Benchmark maximaler
Durchsatz. Verbindliches Auslegungsprofil bleibt720p60; die anderen Dateien sind
Vergleichsprofile. Ein Profil setzt nur Bildgröße/Zielrate und reduziert keine Inhalte:

```sh
build/outshine run --scenario-overrides "$(cat data/profiles/1080p60.json)" /path/to/world.json
build/outshine benchmark --scenario /path/to/world.json --frames 60 \
  --scenario-overrides "$(cat data/profiles/1080p60.json)" --offline
```

Vor dem Fenster wird die gewählte Ansicht vollständig in Refined-Qualität vorbereitet.
`--preload-seconds` setzt deren Deadline, standardmäßig zehn Sekunden. Es gibt keine
Warmup- oder Zusatzframes. Nur das letzte gemessene Bild wird auf Wunsch atomar als PNG
geschrieben. JSON und PNG brauchen unterschiedliche Pfade; Eingabe-/Lade-/Gerätefehler
erhalten bestehende Dateien. Ohne `--output` geht das JSON allein nach stdout.

Der Bericht trennt CPU-Wallzeit ohne absichtliches Pacing von nativen GPU-Zeitstempeln,
vollständige Framezahl, Weltvollständigkeit und vollständige GPU-Stichproben. CPU-Zeit
enthält synchrone Warteanteile. GPU-Erfassung fügt keine Abfragen oder Warteoperationen
hinzu; ihr begrenzter Speicher wird vor dem Fenster angelegt. Fehlende GPU-Zeit bleibt
unbewiesen: keine vollständigen GPU-Perzentile und kein grünes Budget-Gate.
Aufbau, Preload, ungepacetes Fenster, abschließendes GPU-Warten und PNG liegen getrennt vor.
Der SHA-256 der effektiven Szenariodeklaration bindet Overrides und Providerkonfiguration;
er ersetzt keinen Inhaltsnachweis extern geladener Assets. Quellen-/Cachekosten und
Hardware-/Build-/Lastangaben bleiben im Bericht erhalten.

Das Geräteziel nach AGENTS ist 1280×720/60, einschließlich p99≤16,67 ms. Andere Profile
sind deklarierte Vergleiche. Ein funktional erfolgreicher Bericht kann rote Welt-/Zeitbudgets
enthalten. `--gpu-validation` gehört zum getrennten synthetischen Messmodus.
Für den JSON-Bericht wird die Headerbibliothek `nlohmann_json` über pkg-config benötigt
(`nlohmann-json3-dev` unter apt, `nlohmann-json` unter Homebrew).

# Cachewartung

```sh
build/outshine cache trim --assets-mib 8192 --sources-mib 2048
build/outshine cache trim --assets-mib 8192 --sources-mib 2048 --apply --compact
```

Standard ist eine Vorschau. `--apply` entfernt die ausgewählten Inhalte. Null leert den
gewählten Cache. `--root <Verzeichnis>` wählt einen Cache-Elternordner mit `assets/` und
`sources/`; Standard ist das Anwendungsverzeichnis von SDL. Netzwerk und GPU starten nicht.

Das Assetbudget gilt gemeinsam für komprimierte Pakete in `assets.sqlite` und
`prototypes.sqlite`. Verwaiste Pakete werden zuerst entfernt, anschließend die am längsten
nicht verwendeten. Ein Paket wird mit sämtlichen Mitgliedern, Request-Bindungen und
R*Tree-Einträgen gelöscht. Fertige Assets bleiben eigenständige Produkte; ein späterer Miss
nutzt wieder den normalen Generatorpfad. Bereits geladene Leases behalten ihre Bytes.

Quellen werden als erkannte Hash-Dateien samt Abwesenheitsmarkern budgetiert. Unbekannte
Dateien, Symlinks und fremde Verzeichnisse bleiben erhalten. Auch verwaiste Nutzungsmetadaten
werden entfernt. Bekannte letzte Nutzungen bestimmen die Reihenfolge; Altbestände sind als
unbekannt gezählt. Deren Ersatzreihenfolge ist die Paket-Einfügereihenfolge beziehungsweise
die Schreibzeit einzelner Quelldateien. Daraus werden keine Nutzungszeiten gespeichert.

Budgets zählen Nutzdaten. Die kompakten `CACHE`-Zeilen melden zusätzlich Dateigrößen inklusive
SQLite-/WAL-/SHM-Kosten. Ohne `--compact` werden SQLite-Seiten intern wiederverwendbar;
mit dem Flag gibt VACUUM sie an das Dateisystem zurück. Das benötigt zusätzliche IO und
freien Arbeitsraum. Eine blockierte Kompaktion meldet ausdrücklich den bereits erfolgten
Trim und den Fehler. Vorschauen melden ausgewählte, noch nicht entfernte Einträge.

Quellen und Assetpakete speichern künftig Bytezahl, letzte erfolgreiche Nutzung und eine
bekannte Erstellungszeit. Frühere Einträge ohne diese Angaben bleiben ausdrücklich unbekannt.
Wiederholte Zugriffe werden im begrenzten Puffer zusammengefasst und zeitlich gedrosselt
persistiert; das Laden bleibt bei einem Metadatenfehler möglich. Einmalige weltweite Besuche
können so später gegenüber zuletzt verwendeten Inhalten aus dem Speicherbudget fallen.

Wartung erfolgt zwischen Läufen. Metadaten werden gebündelt geschrieben; ein laufender
Prozess kann noch ungespeicherte Beobachtungen oder neue Cacheeinträge besitzen. Datenbanken
bleiben jeweils transaktional konsistent; mehrere Datenbanken und Quelldateien bilden keine
gemeinsame Transaktion. Teilfehler werden mit dem betroffenen Cache gemeldet.
