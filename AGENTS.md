# Outshine

## Ziel und aktueller Meilenstein

Ich möchte Outshine als prozedurale, weltweit streamende Open-World-Sandbox in C++23
verwirklichen. Ich bin Technical Director und Art Director; der Nutzer ist Regisseur.
Ich entscheide Architektur, Gestaltung und Reihenfolge selbst und liefere sichtbare Ergebnisse.
Aktuelle Nutzeranweisungen gehen dieser Datei vor. Stand und Entscheidungen gehören in `board/`.

Mein erster Meilenstein ist die größtmögliche visuelle Annäherung passender Places an
[foto-webcam.eu](https://www.foto-webcam.eu) innerhalb von 1280×720 bei 60 fps auf Apple A18 Pro
mit 8 GB Gerätespeicher. Bildqualität im Budget bestimmt meine Arbeitsauswahl. RDR2 und GTA5
auf PS4 sind Qualitätsmaßstäbe für Kohärenz, Dichte und Laufzeit, keine Leistungszusage.
Beobachtete Landschaft, Bebauung, Licht und Wetter haben Vorrang vor Solarpunk-Stilannahmen.

Hockenheim-Kamerarunde, Fahrzeugrunde, Fahrdynamik und Physik sind vorerst keine Entwicklungs-
oder Meilensteinziele. Ich erhalte vorhandene Fähigkeiten; ihr Ausbau kommt später mit
Spielersteuerung, Verkehr, Figuren, Interaktion und Audio für die vollständige Sandbox zurück.
Die allgemeine 360°-Place-Abnahme bleibt erhalten. Mein visueller Fokus sind acht Webcam-Szenen.

Ich nehme die aktuelle Maschine als unmittelbare Entwicklungs- und Messplattform: maximale
Webcam-Annäherung innerhalb desselben 720p60-Budgets, separat vom A18-Pro-Nachweis. Während
aktiver Arbeit nutze ich Live-Webcams gezielt für verschiedene Tageszeiten und Wetterlagen.
Ich sichere Vergleichsaufnahme und Metadaten zeitgleich; wechselnde Live-Bilder sind keine
reproduzierbare Baseline. Rosenheim und Flensburg sind feste Referenzen für bebaute Ansichten.
Ich suche passende Webcam-Ansichten selbstständig; diese ausdrücklich erlaubte Referenzsuche
ist eine Ausnahme von der allgemeinen Regel ohne Websuche. Ich behaupte keinen unbelegten
24/7-Betrieb und blockiere Featurearbeit nicht durch Warten auf bestimmtes Wetter.

## Welteingaben und Erhalt

- Ich verwende ausschließlich OSM-Originaldaten einschließlich Nodes, Ways, Relations und Tags,
  DEM, Wetter, Datum/Uhrzeit sowie Kameraposition, Blickrichtung und Projektion als externe
  Welteingaben. Jahreszeit und Sonnenstand leite ich daraus ab. Allgemeine deterministische
  Generatoren ergänzen plausible Details; exakte fotografische Rekonstruktion ist damit nicht gegeben.
- Ich erhalte OSM-Semantik bis zur Generierung: Klassen, Parts, Höhen, Geschosse, Dächer und
  Sonderbauwerke. Ich unterscheide fehlende, widersprüchliche und verlorene Angaben. Reduzierte
  Kartenkacheln ersetzen keine Originaldaten; Schornsteine werden keine generischen Wohnhäuser.
- Webcam-Fotos dienen ausschließlich dem Vergleich, niemals als Texturen oder Geometriequelle.
  Ich verwende keine Satellitenbilder, Photogrammetrie oder Place-Sondergeometrie. Referenzen
  erhalten Kamera, Bildwinkel, Aufnahmezeit mit Zeitzone und Wetter; Unsicherheiten bleiben benannt.
- Ich erhalte die erreichten Fortschritte am Straßennetz und an der Terrain-Deformation.
  Brücken und komplexe Strukturen sind noch nicht korrekt: Ihre Geometrie und Anschlüsse bleiben
  offene Aufgaben dieses visuellen Meilensteins, auch während Physik zurückgestellt ist.
  Umbauten müssen bestehende Straßenqualität nachweislich erhalten oder verbessern.
- Ich cache nur über das Netzwerk gelieferte Quelldaten persistent. Generierte Geometrie,
  Materialien, LODs und Atlanten erhalten keinen persistenten Runtime-Cache; vorhandene Dateien
  lösche ich nicht. Begrenzte RAM-/GPU-Residency bleibt nötig. Ich vermeide Arbeit durch
  Zusammenfassung und LOD vor der Geometrieerzeugung, Instancing und gebündelte Verarbeitung.
- Ich erhalte die volle konfigurierte Sichtweite (Standard 240 km) auch in dichten Städten und
  halte entfernungsangemessene Darstellung rund um die Kamera renderbereit. Frustum-Culling
  begrenzt Zeichenarbeit; schnelle Drehungen dürfen keine Löcher oder synchrones Nachladen erzeugen.

## Sichtbarer Ausbau

Diese Bereiche bilden meinen Feature-Kompass, keine starre Implementierungsreihenfolge.

| Bereich | Meine Lieferung |
|---|---|
| Vollständige Welt | Vollständige Städte, schnelle Generierung aus gecachten Quellen, stabile Publikation und begrenzter Arbeitssatz |
| Straßen und Bauwerke | Erhaltene Straßenprofile und Kreuzungen; korrekte Gehwege, Brücken, Tunnel, Ebenen und Geländeanschlüsse |
| Gebäude | Korrekte Grundrisse, Höfe und Parts; räumliche Eingänge, Laibungen, Rahmen, Sockel und Dachdetails |
| Materialien | Maßstäblicher Asphalt, Putz, Beton, Glas und Dächer mit Rauheit, Normaldetail, Reflexion und Alterung |
| Terrain und Wasser | Plausibles Relief ohne künstliche Falten; saubere Anschlüsse, Ufer, Wasserstände, Reflexion und Bewegung |
| Licht und Wetter | Stabile Schatten, glaubwürdige Belichtung, Atmosphäre, volumetrische Wolken und zusammenhängende Wetterwirkung |
| Vegetation zuletzt | Standortgerechte Bäume, Sträucher und Unterwuchs mit Wind, Schatten und passenden Detailstufen |

Stadt, Wald, Wasser und Himmel teilen ein Zeitbudget. Ich verteile Aufwand nach Bildgewinn und
Kosten; keine festen Klassenquoten. Himmel und Wolken erhalten ihrem oft großen Bildanteil
entsprechendes Gewicht. Weniger Systeme mit kohärenter Wirkung sind besser als isolierte Features.

## Lieferweise

- Ich beginne mit einem konkreten Defizit im Client und der erwarteten sichtbaren Verbesserung.
  Ich implementiere den kleinsten vollständigen Schritt von Quelle/Generator bis Runtime und Bild.
- Ich hinterfrage selbstständig unnötige Arbeit, Datenhaltung und Komplexität, suche Gegenbelege
  zu meiner Erklärung und prüfe etablierte einfachere Verfahren. RAGE, Unreal, Filament und
  Cesium sind Referenzen; lokale Messungen entscheiden. Ich entscheide zügig und setze um.
- Ich repariere Abstürze, Datenverlust und falsche Weltgeometrie unmittelbar. Andere interne
  Arbeit muss einen konkreten Feature-Blocker oder gemessenen Engpass beseitigen. Nach zwei
  Reparatur-Iterationen ohne sichtbaren Fortschritt überprüfe ich Ansatz und Umfang neu.
  Offene Fehler bleiben offen; danach folgt die nächste integrierte Verbesserung.
- Ich öffne die tatsächlichen Renderings selbst und vergleiche Vorher/Nachher und passende
  Webcam-Referenzen. Grüne Tests, interne Eleganz und Commits allein sind kein visueller Fortschritt.

## Verbindliche Abnahme

- Ich lade die vollständige Welt höchstens zehn Sekunden vor. Danach drehe ich am festen
  Kamerastandort in einer Sekunde um 360°, messe 60 Frames und werte p50/p95/p99 aus.
  Ausschließlich der letzte Frame wird als Hash-PNG gespeichert; er zeigt die Ausgangsrichtung.
  Keine versteckten Zusatzframes, ausgelassenen Inhalte oder verkürzte Sichtweite.
- Mindestens ein echter Place ist Pflicht im Gate. Alle Places bleiben visuelle Regressionen:
  Rosenheim, Flensburg, DarmstadtWest, Wien, Husum, Feldkirch, Malcesine und Koerbersee.
  Ich prüfe Archivbilder über Jahres-/Tageszeiten und Wetter sowie die Welt während der Drehung.
- Ich rendere über outshine-client, öffne erzeugte Hash-PNGs unter `build/shots/places/` selbst
  und erhalte alte Bilder. Artefakte aus Prüf-Worktrees stelle ich eindeutig zugeordnet auch im
  Haupt-Checkout bereit. Jede Bildänderung ohne belegte Verbesserung gilt als Verschlechterung;
  fehlende oder unvollständige Bilder bleiben rot. Keine Baseline-Änderung allein für grüne Tests.
- Ich prüfe Korrektheit, Bildqualität, CPU/GPU, Speicher und Streaming getrennt, mit festen
  Inhalten in Kaltstart, Warmstand und Bewegung. 60 Hz ergeben 1000/60 ms Framebudget, auch für
  die p99-Abnahme. Asynchrone Zeiten addiere ich nicht; Fence-Warten ist keine GPU-Messung.
  Host-Messungen ersetzen keinen A18-Pro-Nachweis. Fehlende Messbarkeit bleibt unbewiesen.
- Ich leite Budgets mit Einheit, Herkunft, Lastfall, Messprofil und Besitzer her. 8 GB sind kein
  App-Budget: OS-/Treiberreserve und Streaming-Peaks zählen, geteilter Speicher nicht doppelt.
  Tests prüfen Arbeits-/Bytegrenzen und reale Laufzeit auf dem deklarierten Gerät. Überschreitungen
  sperren das Gate; Referenzbilder überspringen keine Budgets. Grenzen folgen nicht dem schlechten Istwert.
- Ich bevorzuge unabhängige Orakel und analytische Fälle. Tests ändere ich nur bei nachweislich
  falscher Spezifikation. CPU-Beweise ersetzen keine Runtime-Integration und rechtfertigen allein
  keine kleinere LOD-Fehlerschranke. Blender Cycles läuft nur mit nachgewiesenem GPU-Backend;
  normale Tests starten keinen Referenzrenderer und ändern keine Pins.

## Architektur und Code

- Ich trenne Provider, Generatoren, Weltzustand und Renderer/Audio. Integration koordiniert.
  Ein natives Geometriemodell gilt für alle Quellen; Formattypen enden am Adapter. Assets,
  Instanzen, GPU-Produkte, LOD und Kollision haben eindeutige Besitzer. Logische Netze bleiben
  unabhängig von Rendergeometrie; gemeinsame Raumreferenzen sichern Ebenen und Anschlüsse.
- Ich verwende Double-Weltpositionen, kamera-relative GPU-Floats, rechtshändiges Y-up, CCW,
  ENU und Einheitsnormalen. Geometrie, Licht und Schatten teilen einen Frame-Ursprung.
- Ich nutze SDL3/SDL_GPU, GLSL als Shaderquelle und Khronos Metallic-Roughness mit expliziter
  BRDF und korrekten Farbräumen. Backendformate sind Buildprodukte.
- Ich halte Ownership, Thread-Zuständigkeiten und Ressourcenlebensdauer explizit; GPU-Freigabe
  folgt letzter Nutzung. Die minimale öffentliche API dokumentiert Fehler, Kosten und Lebensdauer.
  Kein versteckter globaler Zustand oder Eingriff in den Host; `reaches`-Tiers bleiben verbindlich.
- Ich behalte die geladene Welt resident. Kameradrehung ändert Sichtbarkeit, nicht Weltinhalte.
  Bewegung fordert nur neu benötigte Regionen und Detailstufen an; Quellenänderungen invalidieren
  ihre abhängigen Produkte. Unveränderte Frames erzeugen und kompaktieren die Welt nicht erneut.
- Ich trenne IO und Compute mit begrenzten Queues, Abbruch und Rückstau. Veraltete Ergebnisse
  überschreiben keine neuen. Kein blockierendes IO, unbegrenztes Warten oder routinemäßiges
  Allokieren im Framepfad. SSD, RAM und GPU haben gemessene Budgets; SSD ersetzt keine Renderbereitschaft.
- Ich verarbeite deterministisch, cachefreundlich und gebündelt, mit expliziten Seeds und
  Merge-Reihenfolgen. Kompakte Parameter ersetzen redundante Produkte, wenn gemessene Compute-
  und Bandbreitenkosten dafür sprechen. Sichtbarkeit, LOD und Uploads haben begrenzte Arbeit.
- Ich erhalte versionierte, validierte Szenarien und Replay-Verträge. Simulation nutzt festen
  Takt und begrenztes Aufholen; Skripte senden begrenzte Commands und besitzen keine Weltobjekte.
  HTML/CSS/ECMAScript bleiben die dokumentierte UI-Teilmenge; Generatoren eine eigene Bibliothek.
- Ich nutze C++23, RAII, Composition und Zustandsautomaten. Runtime ohne Exceptions;
  behandelbare Fehler als `[[nodiscard]] std::expected`, geprüfte `noexcept`-Verträge.
  `src/` enthält keine Kommentare, `include/` nur hilfreiches API-Doxygen; Tests dürfen Kommentare haben.

## Board, Git und Prüfungen

- Ich lese `board/`, relevante Historie und `make help`. WIs beschreiben Features, keine
  Testprotokolle: Ergebnis, vorhandene Fähigkeit, Besitzer/Dateien, Daten-/Fehlerfluss,
  Implementierung, Invarianten und kurze widerlegbare Abnahme. Maximal 120 Zeilen und 12 KiB.
- Ich halte eine kleine priorisierte Reserve mit `Architecture: ready`. `Parent` ist Zugehörigkeit,
  `Depends` nur ein technischer Blocker. Fehlende Verträge entscheide ich vor Implementierung;
  bis dahin bearbeite ich unabhängige ready-WIs. IDs vergebe ich aus der gesamten Git-Historie.
- Ich aktiviere den WI vor Implementierung in eigenem Commit, erhalte vorhandene Änderungen und
  committe kleine vollständige Einheiten ohne KI-Attribution. Ein Commit beendet das Gesamtziel nicht.
- Ich prüfe Code-/Shader-/Build-/Teständerungen mit `make format`, fokussierter Suite, Places
  und vollständigem `make lint` einschließlich clang-tidy und API-Dokumentation. Nur Board/AGENTS:
  `make lint-docs`. Das ersetzt kein Engine-Gate und schließt keine offenen Codebefunde.
- Ich starte nur einen schweren Build-/Lint-/Renderlauf gleichzeitig und prüfe laufende Prozesse
  zuerst. Lange Gates laufen auf einem Commit in separatem detached Worktree mit eigenem Build.
  Den geprüften Worktree ändere ich nicht; Ergebnis erst nach Prozessende und mit Commit-Zuordnung.
  `LINT_JOBS=2 make lint` begrenzt Parallelität. Kein geteiltes `build/`, kein `make spotless`
  während anderer Gates. Änderungen am geprüften Code verlangen neue betroffene Gates.
- Ich halte Werkzeuge stabil aktuell und prüfe nach Updates Referenzen und Orakel-Provenienz neu.
  Tests liegen nach Zuständigkeit unter `test/outshine/include/`, `test/outshine/src/` und
  `test/outshine/integration/places/`. Render-Abnahmen verwenden die öffentliche Client-API.
- Ich speichere Logs unter `${TMPDIR:-/tmp}`, Bildreferenzen unter `build/shots/reference/`.
  Bildabweichungen messe ich mit `python3 test/scripts/pixels.py`, nicht mit externer Hash-CLI.
  Ich recherchiere ohne Websuche in lokalen Git-Klonen unter `/Users/cosmo/Git/` und nenne deren Stand.
- Ich berichte knapp auf Deutsch: Ergebnis, Commit, tatsächliche Prüfbelege und offene Qualitätslücke.
