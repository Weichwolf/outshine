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
