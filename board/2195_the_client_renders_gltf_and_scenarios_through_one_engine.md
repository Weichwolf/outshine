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
Direkter render-Befehl im Ausbau: öffentlicher Loader → nativer Snapshot → Engine.
Explizite Zeit bleibt beim temporalen Render-Settling eingefroren; run bleibt separat.
Loaded und Engine beherrschen Import, Kameras, Materialien und PNG bereits.
Auto-Framing berücksichtigt beide Viewport-Achsen und prüft endliche Bounds.
Die unbenutzten quadratischen Legacy-Überladungen entfallen; nur der Viewport-Vertrag
bleibt öffentlich. Loader-Verträge einschließlich Move/Fehler/Lebensdauer dokumentieren.

Harness konsolidieren: Vorbereitung, öffentliche Ausführung und unabhängige Auswertung
trennen. Render-Abnahmen ohne interne Engine-Typen; API-Tests nur für API-Verträge.
Jeder Fall prüft eigene Voraussetzungen und verwendet frische Ausgabepfade. Keine
Laufreihenfolge, fremden Restdateien oder ungeprüften historischen Referenzen voraussetzen.
Khronos-JSON hält glTF-Input, Setup und Referenz-Hashes pro explizitem Zeitpunkt;
mehrere feste Kameras pro Fall, besonders Gesamt-/Detailansichten komplexer Szenen.
Auflösung, Zeit, Seed, Licht, Farbraum und Vergleichsvertrag ausdrücklich deklarieren;
PNG-Daten liegen im geprüften Hash-Cache. Vorbereitete Eingaben ebenfalls gegen ihre
Quelle/Transformation und Provenienz prüfen; Referenz-Hash allein validiert keinen Input. Normale Läufe erzeugen weder Referenzen
noch Pins. Der Client sampelt exakte Zeiten; die Million-FPS-Näherung im Harness entfernen;
Sequenzen bis dahin ausdrücklich ungewertet melden, niemals nur Frame 0 akzeptieren.
Gemeinsame Fixture-/Provenienzauflösung statt separater Pfadkonventionen; vorbereitete
Assets müssen einzeln reproduzierbar sein. Bestehende Prüfumfänge beim Umbau erhalten.

Loader-Voraussetzung: load publiziert erst nach vollständiger Konvertierung und erhält
bei Fehlern das vorherige Asset. Neuer Import setzt Variante/Clips zurück; unabhängige
Fixtures prüfen Wiederverwendung und Fehlversuche. Loaded::poses kann native Geometrie
zu expliziter Zeit liefern; Render-Settling darf diesen Snapshot nicht weiterbewegen.
Kameraauswertung muss dieselben gesampelten lokalen Transformationen wie Geometrie
verwenden, einschließlich Elternketten, Rückwärtssampling und Clip-Deaktivierung.
Ungültige Zeitwerte vor Mutation ablehnen. Unabhängige analytische Kamera-Fixture
prüft Positionen. Die vier erkannten Materialkanäle bis zur nativen Geometrie führen:
Basisfarbe, Metallic, Roughness, Emission einschließlich separater EmissiveStrength.
Kanalbreite, Zielmaterial und endliche Ergebniswerte prüfen; Reset/Rückwärtssampling
gegen unabhängige Fixtures. Weitere Animation-Pointer-Ziele bleiben offen.
Clipauswahl zunächst separat validieren: Pose::Build leert seinen Output vor der
Indexprüfung. Ein abgelehnter Clip darf die aktive Animation nicht zerstören;
Regression sampelt nach ungültiger Auswahl die bisherige Kamera weiter.
Variantenwahl und Sampling liefern wie load/plays eigene expected-Diagnosen;
erfolgreiche Mutationen löschen alte Fehler. Fehlertexte bleiben im Ergebnis gültig.
Referenz: https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_animation_pointer

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

Client-Nachweis: unabhängige glTF/GLB-Fixtures, perspektivisch/orthographisch,
Varianten, Zeit, ungültige Argumente und Ausgabe. Auto-Framing ohne Seitenverhältnis
schneidet die Hochformat-Fixture ab und wird erkannt. ABeautifulGame über render als
Übersicht und Nahaufnahme visuell geprüft. Corpus-Migration mit unveränderten Licht-/
Transfer-/Materialverträgen bleibt offen; Vorschau ist keine photorealistische Abnahme.

## Abnahme

- [x] Make baut den Client samt neuem Pfad; Hilfe und Beispiele dokumentiert.
- [x] glTF und GLB, externe Ressourcen, Kamera vorhanden/fehlend, Auto-Override,
      Hoch-/Querformat, Pose/FOV-Override, ungültige CLI-/Asset-/Ausgabeparameter geprüft.
- [x] PNG-Dimensionen, sichtbare Bounds und Kamerakonventionen unabhängig geprüft;
      PNGs selbst visuell angesehen, Negative Kontrolle des Framings wird rot.
- [ ] Materialien/Licht/Animation und bestehende Szenario-Rendervergleiche bleiben korrekt.
- [ ] Render-Harness auf gemeinsamen Client ausgerichtet; kein neuer privater Renderpfad.
- [ ] make lint und betroffene Make-Suiten; Logs im System-Tempverzeichnis.

## Gemeinsamer Importvertrag
Outshine-Szenario und glTF sind Importadapter mit unterschiedlicher Ausdrucksstärke.
Formatsyntax/-referenzen und Konvertierung im Adapter; native Weltbeschreibung und
fachliche Validierung gemeinsam mit direkten Code-Aufrufern über die öffentliche API.
Kein Importer schreibt an dieser API vorbei in Engine-Interna. glTF-Konventionen
bleiben im Adapter; Szenarien ergänzen Weltgenerierung, Simulation und Umwelt.
Import zunächst vorbereiten/validieren, dann transaktional veröffentlichen; gemeldete
Fehler erhalten die aktive Welt. Dies ist SOLL, keine bestehende Fehlergarantie.
Abnahme: äquivalente Code-/Szenario-/glTF-Inhalte erzeugen gleiche native Semantik;
identische native Defekte werden über alle drei Pfade abgelehnt. Später Importfehler
erhält vorhandene Welt. Szenario-Reader/Writer erhalten alle unterstützten statischen
Deklarationen im Roundtrip; Laufzeithandles/Savegame-Zustand sind kein Importformat.

## Roundtrip-Client
Roundtrip-Prüfung aus main in eigene Client-Komponente verschoben.
Engine-Aufrufe bleiben öffentlich; vorhandenes WriteFileAtomically prüft Schreiben/Close
und erhält alte Dateien bei IO-Fehlern. Pro-Szenario-Prüfung liefert expected<Bytes,Fehler>,
CLI zählt Ergebnisse getrennt. Test: gültiger Export und ungültiges Ziel, plus vorhandene
Short-Write-Negativkontrolle des gemeinsamen IO-Helfers; alle Places bestehen über CLI.
Negativkontrolle mit verschlucktem Schreibfehler scheitert. Keine Aussage über verlorene
Sektionen aus Selbstvergleich ableiten. Gemeinsamer CLI-Scratchpfad bleibt noch zu isolieren.

Build-Audit-Negativkontrolle wählt den entfernten Provider aus der Engine-Profildeklaration.
Ein nur vom Test aufgerufenes Client-Blatt erzeugt beim Entfernen keinen ungelösten
Bibliotheksverweis; die bisherige alphabetische Auswahl war dafür falsch spezifiziert.
Erwarteter Audit-Fehler unverändert, keine zusätzliche Dateiliste im Test.
