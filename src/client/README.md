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

# Asset-Aufnahmen

`make render ASSET=/path/model.glb OUTPUT=/path/shot.png RESOLUTION=1280x720`

`outshine-client render <asset.gltf|asset.glb> <width>x<height> <output.png> [options]`

- Auflösung: ganzzahlige Achsen von 1 bis 4096 Pixeln. Ausgabe als PNG; Verzeichnis muss existieren.
- `--camera auto|index`: Kamera null wird standardmäßig verwendet, falls eindeutig platziert;
  sonst Auto-Framing der aktuellen Bounds. Auto berücksichtigt beide Viewport-Achsen.
- `--time seconds`: absoluter, endlicher Zeitpunkt ab null; Standard null. Die Pose bleibt
  während des Render-Settling stehen. Schlüsselenden werden gehalten, nicht geloopt.
- `--animation index`: Clipauswahl, standardmäßig erster Clip, falls vorhanden.
- `--variant name`: Materialvariante vor dem Sampling auswählen.
- `--position x,y,z --look-at x,y,z`: gemeinsamer Override in lokalen Metern, Y oben.
- `--fov degrees`: vertikaler perspektivischer Öffnungswinkel zwischen 0 und 180 Grad.
- `--lighting auto|authored|studio`: auto erhält vorhandene Lichter; ohne importierte Lichter
  benutzt es die deterministische Vorschau. Studio ergänzt einen Key mit π Lux bei 45°
  Elevation/Bearing und einen linearen Fill von 0,05. Authored ergänzt kein Licht.
- `--exposure multiplier`: positiver linearer Belichtungsfaktor, Standard eins.

Optionen folgen den drei Positionsargumenten. Jede Option höchstens einmal.
Ungültige Argumente liefern Exitcode 2; Lade-/Render-/Ausgabefehler Exitcode 1.
Erfolg schreibt eine RENDER-Zeile mit Eingabe, Auflösung, Zeitpunkt und Ausgabepfad.
Der Szenariopfad `run` bleibt separat; beide Wege benutzen die öffentliche Engine-API.

Der Loader unterstützt noch nicht alle Animation-Pointer-Ziele und Importerweiterungen.
Der direkte Pfad behauptet keine vollständige Khronos-Konformität. Den Corpus schrittweise
mit identischen Kamera-, Licht-, Transfer- und Materialeinstellungen migrieren (WI 2195).

`make test-client-render` prüft den tatsächlichen Client mit unabhängigen temporären glTF-/GLB-
Fixtures; Blender und vorbereitete Khronos-Dateien sind dafür nicht erforderlich.
