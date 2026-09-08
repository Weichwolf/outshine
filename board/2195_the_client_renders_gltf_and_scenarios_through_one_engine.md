Type: feature
State: active
Area: client, include, import, test
Parent: 2188
Depends:

# The client renders glTF and scenarios through one Engine

## Auftrag und Befund

Nutzerauftrag: direkter glTF/GLB-Pfad parallel zum Szenario-Pfad; Datei, Auflösung
und PNG-Ausgabe genügen. Sinnvolle Auto-Kamera wenn keine glTF-Kamera vorhanden,
alle Kameravorgaben übersteuerbar. Diesen Client künftig für Render-Abnahmen nutzen.
Main.cpp bietet run <scenario>, aber keinen direkten Asset-Renderbefehl.
Loaded und Engine beherrschen Import, Kameras, Materialien und PNG bereits.
FramingFor berücksichtigt bisher nur vertikalen FOV; schmale Bilder können clippen.

## Umsetzung

- render <asset.gltf|asset.glb> <width>x<height> <output.png> neben run <scenario>.
  Kein temporäres Szenario-XML und kein zweiter Renderer. Öffentliche Engine-API;
  vorhandene Import-/Asset-Ownership nutzen, Geometrie nicht mehrfach importieren.
- Kamera: explizite Pose/Projektion bzw. Kameraauswahl > glTF-Kamera > Auto-Framing.
  Auch vorhandene Kamera mit auto übersteuerbar. Index außerhalb der Datei ist Fehler.
- Auto-Framing über transformierte sichtbare Bounds, Seitenverhältnis, Sicherheitsrand
  und abgeleitete Near/Far-Ebenen. Gemeinsame Engine-Implementierung, keine CLI-Mathematik.
  Perspektivische und orthographische Kamerakonventionen nach Khronos:
  https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#cameras
- Argumente nichtwerfend und vollständig parsen; endliche Werte, positive begrenzte
  Auflösung, Fehler auf stderr und verlässliche Exitcodes. Kein Erfolg ohne PNG.
- Authored Materialien und Licht erhalten. Fehlende Lichtumgebung mit ausdrücklich
  definiertem, deterministischem Standard behandeln; für Oracles abschalt-/übersteuerbar.
- Bestehenden Szenario-Pfad und Corpus-Semantik erhalten. Render-Tests nutzen denselben
  Client mit passenden glTF-/Szenario-Optionen; interne Fehler-Injektion bleibt API-Test.

## Abnahme

- [ ] Make baut den Client samt neuem Pfad; Hilfe und Beispiele dokumentiert.
- [ ] glTF und GLB, externe Ressourcen, Kamera vorhanden/fehlend, Auto-Override,
      Hoch-/Querformat, Pose/FOV-Override, ungültige CLI-/Asset-/Ausgabeparameter geprüft.
- [ ] PNG-Dimensionen, sichtbare Bounds und Kamerakonventionen unabhängig geprüft;
      PNGs selbst visuell angesehen, Negative Kontrolle des Framings wird rot.
- [ ] Materialien/Licht/Animation und bestehende Szenario-Rendervergleiche bleiben korrekt.
- [ ] Render-Harness auf gemeinsamen Client ausgerichtet; kein neuer privater Renderpfad.
- [ ] make lint und betroffene Make-Suiten; Logs im System-Tempverzeichnis.
