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
