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

Harness konsolidieren: Vorbereitung, öffentliche Ausführung und unabhängige Auswertung
trennen. Render-Abnahmen ohne interne Engine-Typen; API-Tests nur für API-Verträge.
Jeder Fall prüft eigene Voraussetzungen und verwendet frische Ausgabepfade. Keine
Laufreihenfolge, fremden Restdateien oder ungeprüften historischen Referenzen voraussetzen.
Khronos-JSON hält glTF-Input, Setup und Referenz-Hashes pro explizitem Zeitpunkt;
mehrere feste Kameras pro Fall, besonders Gesamt-/Detailansichten komplexer Szenen.
Auflösung, Zeit, Seed, Licht, Farbraum und Vergleichsvertrag ausdrücklich deklarieren;
PNG-Daten liegen im geprüften Hash-Cache. Vorbereitete Eingaben ebenfalls gegen ihre
Quelle/Transformation und Provenienz prüfen; Referenz-Hash allein validiert keinen Input. Normale Läufe erzeugen weder Referenzen
noch Pins. Exakte Zeitauswahl fehlt dem Client: die Million-FPS-Näherung entfernen;
Sequenzen bis dahin ausdrücklich ungewertet melden, niemals nur Frame 0 akzeptieren.
Gemeinsame Fixture-/Provenienzauflösung statt separater Pfadkonventionen; vorbereitete
Assets müssen einzeln reproduzierbar sein. Bestehende Prüfumfänge beim Umbau erhalten.

Loader-Voraussetzung: load publiziert erst nach vollständiger Konvertierung und erhält
bei Fehlern das vorherige Asset. Neuer Import setzt Variante/Clips zurück; unabhängige
Fixtures prüfen Wiederverwendung und Fehlversuche. Loaded::poses kann native Geometrie
zu expliziter Zeit liefern; Render-Settling darf diesen Snapshot nicht weiterbewegen.
Animierte Kameras gesondert prüfen: camera(index) liest bisher die ursprüngliche Pose.

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
