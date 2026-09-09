Type: bug
State: active
Area: engine, include, scenario
Tags: architecture, state, errors
Parent: 2188
Depends: 2210

# Engine transitions will publish only valid state

## Befund und Entscheidung

handleEvent trennt inzwischen behandelt, ignoriert und Fehler; öffentliche Input-/
UI-Tests prüfen dies. Der Legacy-error-Text bleibt separat und kann veraltet sein.
EngineHeld.h verteilt Phasen über Taken, Targeted, FrameOpen, Carrying usw.; Audio-Vorbereitung ist bereits optional.
Unabhängige Eigenschaften bleiben erlaubt; Phasen mit verbotenen Kombinationen
benötigen dagegen explizite Zustandsautomaten. Keine pauschale Boolean-Ersetzung.

**Benchmark**: Filament trennt Engine-Ressourcen und Frame-Aufrufverträge.
https://github.com/google/filament/blob/main/filament/include/filament/Renderer.h
Unreal/RAGE sind kein Beleg für atomare outshine-Declare-Semantik; diese folgt aus
unserem Szenario-/Sandbox-Vertrag und wird hier ausdrücklich festgelegt.

Konfiguration validieren, Kandidaten aufbauen, erst dann veröffentlichen. Fehlgeschlagene
änderbare Konfiguration erhält den letzten gültigen Zustand; irreversibler Devicefehler
wechselt ausdrücklich in Failed. Keine Erfolgsvortäuschung durch alte Framebilder.
Fehler als strukturierter Code mit Kontext; SDL-Text am Fehlerort übernehmen.
Eventergebnis unterscheidet behandelt, ignoriert und fehlgeschlagen.
Alle Engine-Mutatoren inventarisieren, einschließlich offers/setRoots/setSurfaces,
declare/assemble und save/restore. Unsupported-Deklarationen nach 2131 zurückweisen.
2185 besitzt Feature-Ressourcen, 2151 Persistenzschema. Stabile geliehene Handles
und nicht bewegliche Engine-Owner sind Voraussetzung.

## Gemeinsame Szenario-Kameraprojektion

Khronos definiert Half-Extents und Near/Far-Bedingungen:
https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#cameras
Watches ersetzte NaN/negative Werte durch Defaults, Carries übernahm nur FOV und
verlor Near/Far/Orthographic. Beide benutzen jetzt die vorhandene Lens-Grenze;
keine zweite numerische Validierung.

Beide Pfade benutzen dieselbe Szenario-zu-Viewpoint-Abbildung mit nodiscard expected.
Nur perspektivischer FOV=0 und Near=0 sind erklärte Defaults (55 Grad, 0,05 m).
Far=0/+Inf bezeichnet unendliche Perspektive. Orthographie verlangt positive
X/Y-Halbausdehnung und endliche Far>Near; Near=0 ist gültig. Öffentliche Felder
und Setter dokumentieren die Trennung von Deklaration und Runtime-Validierung.
Kandidat vor Eye-Veröffentlichung durch Lens::From prüfen; fehlende mitgeführte
Kamerabasis liefert einen Fehler, keinen vorgetäuschten Erfolg.

ScenarioViewsPreserveProjection: 346 Checks, analytisches Dreieck und Reverse-Z-Tiefe,
stehend/mitgeführt, Defaults, Near=0 orthographisch, 15 ungültige Deklarationen je Pfad
mit wiederholter Ablehnung, Bildbestand und Recovery; vor und nach Negativkontrolle grün.
Negativkontrolle mit alter Watches-/Carries-Abbildung: Kamera-Test mit 73 fehlgeschlagenen
Checks (einschließlich Folgefehler), zusätzlich zum Mipmap-Fehler. Mutation zurückgenommen.
Beide PNGs selbst geöffnet: identische orthographische Lage/Ausdehnung. API-Vertragstest,
keine Place-/Fotorealismusabnahme. Bestehende Places deklarieren stehende Perspektiven.

## Weitere konkrete Lücken

DrawsInto ändert Dimensionen/Target, baut aber die planabhängigen Frame-Attachments
und Present-Pipeline nicht als zusammenhängenden Kandidaten neu auf. Größen- und
Formatwechsel müssen dieses Ressourcenpaket atomar ersetzen, nicht nur das Target.
Noch kein Nachweis korrekter Pixel nach einem solchen Wechsel.

setGeometry liefert derzeit Weltgeometrie; Carries lehnt sie ohne importierten
Subject-Anteil ab. Explizite native Geometrie-zu-Entity-Zuordnung im API-SOLL aus
2096 prüfen. Die Kamera-Fixture nutzt deklarierte glTF-Körpergeometrie.

FollowCamera liest die Pose der beim Assemble aufgelösten Entity unabhängig vom
Geometrie-Upload. Viewwechsel, Reorder, fehlende/unplatzierte/mehrdeutige Ziele und
veraltete Assembly über öffentliche API geprüft. Gemeinsamer Resolver mit Audio;
Körperindex-0-Negativkontrolle verletzt das Kamera-Oracle. Beide Ziel-PNGs geöffnet:
jeweils zentriertes Dreieck bei korrekt unterschiedlicher Kameraposition.
Native Geometrie-zu-Entity-Zuordnung und Instanzen-Posen bleiben nach 2096 offen.

## Gemeinsame Körperbindung für Kamera und Audio

Simulationskörper behalten jetzt ihre Entity-ID auch nach Filterung unplatzierter Bodies.
Audio und Kamera lösen Körpernamen vor der Verwendung in native Körperzuordnungen auf.
Trigger gehören jetzt zur Assembly und prüfen alle lebenden Körper im Simulationsschritt.
Native Simulationskörper müssen ihre Entity-Handles behalten. Namen einmal gegen
Assembly auflösen, unbekannte/mehrdeutige Ziele ablehnen; Hot Paths verwenden Handles.
Transform/Velocity als engine-eigenen Zustand führen, nicht aus Renderteilen ableiten.
Auch Instances benötigen diesen gemeinsamen räumlichen Vertrag; keine zweite Audio-Welt.
Deklaration, Assembly und Bindungen als zusammengehörige Generation publizieren;
fehlgeschlagener Neuaufbau erhält den alten gültigen Satz. Prepare darf keine Körper
vorheriger Deklarationen binden. Reorder, unplatzierte Templates, mehrere bewegte
Körper, Zielwechsel und fehlgeschlagene Reassembly über öffentliche API prüfen.

Scene, Columns, Tabellen und Physikkörper in einem nicht verschiebbaren Heap-Besitzer
zusammenhalten; Column leiht die Scene-Adresse. Assemble-Kandidat erst nach allen
Prüfungen publizieren. Physikkörper behalten Entity-Owner; leere Assembly ersetzt
alten Zustand auch ohne Renderziel. Scene-Borrows invalidieren nur bei Erfolg.
Reserve + Bodies + Kinds + Instances + Player-Mind vor Allokation auf 65536 Slots
begrenzen (gesetztes Enginebudget); Summenüberlauf ablehnen. Rollback und
Komponenten-Lebensdauer über öffentliche API und Negativkontrolle prüfen.
Audio bindet nur an die aktuelle Deklarationsrevision; fehlende/unplatzierte/mehrdeutige
Ziele ablehnen. Stereo-Reorder/-Zielwechsel und Headless-Mehrkörper-Trigger geprüft;
front()- und Rendererabhängigkeits-Negativkontrollen rot. Trigger tragen volle Entity-Handles.

## Abnahme

- [x] Target-Kandidaten vor Veröffentlichung vorbereiten; SDL-Fehlertexte besitzen
      Speicher. Fenster-Claims und Offscreen-Textur bleiben bei Ablehnung erhalten.
- [x] Target-Fehlergrenzen und gültige Fenster-/Offscreen-Pfade: 44 Consumer-Checks.
- [x] Vorzeitiges Targeted-Publizieren erzeugt genau einen Fehler im 44-Check-Oracle.
- [x] Fehlende Kamera, Recovery, Extent und importierte Kamera: 36 Checks;
      angefordertes Neu-Framing bleibt beim Rebind erhalten.
- [x] Alte Bereitschaft samt Standardbasis wieder eingesetzt: Kamera-Consumer
      bricht an der ursprünglichen Lens-Assertion ab; 25 andere Tests bestehen.

- [x] Geprüfte Float-Lens und Zustandserhalt nach Near-Plane-Ablehnung: 52 Checks.
- [x] Verengungsprüfung entfernt und Kamera zu früh publiziert: acht numerische
      Checks und ein Zustandserhalt-Check schlagen fehl. Mutationen zurückgenommen.

- [ ] Öffentliche Übergangstabelle nennt erlaubte Reihenfolge und Fehlergarantien.
- [ ] Fehler an jeder Build-/Validate-/Publish-Grenze injizieren; gültiges altes
      Szenario bleibt nutzbar oder ausdrücklich Failed, nie halb veröffentlicht.
- [x] Irrelevante Events sind unhandled; Input-/UI-Fehler tragen eine eigene Diagnose.
- [ ] Wiederholtes Declare, Targetwechsel und Featurewechsel ohne Ressourcenwachstum.
- [ ] Negativkontrolle publiziert vor Validierung; Zustandserhalt-Oracle wird rot.
