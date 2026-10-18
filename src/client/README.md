# Szenarien

`outshine-client run [options] <scenario> [name]`

`scenario` ist eine XML-/JSON-Datei oder ein vollständiges JSON-Objekt als Argument.
JSON verwendet die bestehenden Szenariofelder: skalare Werte entsprechen Attributen,
Objekte Sektionen, Objektarrays wiederholten Kindern. Skalare Arrays ergeben Vektoren.
Boolesche Werte sind `true`/`false`; Feldnamen und Validierung bleiben identisch zu XML.

```sh
build/outshine-client run --offline '{"world":{"lat":47.232575,"lon":9.598371,"sightM":240000},"render":{"widthPx":1280,"heightPx":720},"clock":{"start":"2026-09-07T10:40:00Z","live":false},"views":{"view":{"id":"station","at":{"lat":47.232575,"lon":9.598371,"heightM":614,"samplesHeight":false,"bearingDeg":1,"pitchDeg":-11.5}}}}'

build/outshine-client shots --offline \
  --scenario-overrides '{"world":{"vegetation":false}}' Feldkirch
```

`--scenario-overrides <JSON object>` gilt für `run`, `measures` und `shots`.
Objekte ergänzen vorhandene Felder, Arrays ersetzen Sammlungen, `null` entfernt Felder.
Overrides bestimmen zuerst die Layerwahl und gewinnen anschließend gegenüber geladenen
Layern. Relative Layerpfade beziehen sich bei Dateien auf deren Verzeichnis, bei Inline-JSON
auf das Shipped-Verzeichnis. Eingabe, Overrides und geladene Layer zusammen: maximal 16 MiB.
Unbekannte Felder, doppelte Schlüssel und ungültige Werte werden abgewiesen.
Szenarioinhalte bekommen keine einzelnen CLI-Schalter.

Places planen die Frame-Anfänge auf absoluten Terminen. Die Messung endet nach
der GPU-Fertigstellung des letzten Frames; sie wartet danach kein leeres Restintervall.
Framezahl und 360°-Kamerafahrt bleiben gleich. PNG-Kodierung liegt außerhalb der Messzeit.

Das Place-Pflichtgate überschreibt das Profil ausdrücklich mit 1280×720/30.
`make suite SUITE=outshine/integration/places60` prüft separat den Vergleich bei 1280×720/60.
Beide erhalten Inhalt/Sichtweite und das CPU-p99-Ziel unter 10 ms. Jede Messung benötigt
GPU-Zeitstempel für sämtliche Frames und ein GPU-Maximum innerhalb von `1000/fps` ms.
Bei 30 bzw. 60 Frames entspricht das Maximum auch dem Nearest-Rank-p99.

`shots --rows` ergänzt `PROFILE` und die unveränderte `ROW` um eine `GPUROW`:
`Name`, gerenderte Frames, GPU-Zeitstempel, GPU-Gesamtzeit ms, GPU-Maximum ms,
Frames ohne Zeitstempel, CPU-Abfragezeit ms; Felder durch Tabs getrennt.
Die Werte stammen ausschließlich aus dem abgeschlossenen Messintervall vor dem Screenshot.
Fehlende Messwerte sind `nan`; Preload-/Lifetime-Zeiten ersetzen sie nicht.
Vor jeder Place-Messung startet `renderer().resetMeasurements()` ein neues CPU-/GPU-Fenster.
Es wartet und rendert nicht. Später eintreffende Ergebnisse älterer Einreichungen zählen
weiterhin nicht zum neuen Fenster. Auch mehrere Aufnahmen desselben Engine-Prozesses
melden ausschließlich ihre eigenen Frames; Quelle-/Assetzähler bleiben davon unabhängig.

# Cachewartung

```sh
build/outshine-client cache trim --assets-mib 8192 --sources-mib 2048
build/outshine-client cache trim --assets-mib 8192 --sources-mib 2048 --apply --compact
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
