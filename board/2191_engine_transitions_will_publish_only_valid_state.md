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
EngineHeld.h verteilt Phasen über Taken, Targeted, FrameScope, Carrying usw.; Audio-Vorbereitung ist bereits optional.
Unabhängige Eigenschaften bleiben erlaubt; Phasen mit verbotenen Kombinationen
benötigen dagegen explizite Zustandsautomaten. Keine pauschale Boolean-Ersetzung.
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
Khronos-Kameravertrag: Half-Extents und gültige Near/Far-Bedingungen.
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
ScenarioViewsPreserveProjection prüft analytische Projektion, Fehlererhalt und Recovery;
Gegenprobe mit alter Abbildung scheitert. Einzelresultate und PNG-Nachweise in Git.
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
- [x] Target, Projektion und Input: Fehlererhalt, Recovery und negative Kontrollen geprüft.
      Detailnachweise der abgeschlossenen Schritte stehen in Git.
- [ ] Öffentliche Übergangstabelle nennt erlaubte Reihenfolge und Fehlergarantien.
- [ ] Fehler an jeder Build-/Validate-/Publish-Grenze injizieren; gültiges altes
      Szenario bleibt nutzbar oder ausdrücklich Failed, nie halb veröffentlicht.
- [x] Irrelevante Events sind unhandled; Input-/UI-Fehler tragen eine eigene Diagnose.
- [ ] Wiederholtes Declare, Targetwechsel und Featurewechsel ohne Ressourcenwachstum.
- [ ] Negativkontrolle publiziert vor Validierung; Zustandserhalt-Oracle wird rot.
## Generatoren bei erneuter Deklaration
SamePicture vergleicht Renderparameter, nicht Generatoren oder deren Providerdaten.
Der schnelle declare-Pfad übersprang generated: neue Parameter oder ein nun
ablehnender Producer wurden ignoriert. Korrigiert; kein gültiger Ergebnis-Cache:
Generator::make darf von geliehenen Providern abhängen, eine Revision fehlt.
Der vollständige Aufbau läuft jetzt, sobald alte oder neue Deklaration
Generatoren/generated-Assets enthält; Render-Reuse nur ohne solche Inhalte.
Das gilt auch für unveränderte Parameter und Entfernen des letzten Producers.
Public-API-Test mit Offscreen-Ziel: neun Checks grün; alter Stand sechs Fehler,
kein Buildfehler. Parameterweitergabe/Input-Erhalt grün. Wien bytegleich c307cab8.
Vollständiger Rollback und deklarierte Providerrevisionen bleiben getrennt offen.
park/resume halten nur Deklarationen, keinen Simulationssnapshot. park leert Teile
der Welt und die Renderinstanz, ohne die gesamte Simulation/Streaming-Residency zu
parkieren. resume ruft declare auf; ein Fehler kann die aktive Engine teilweise ändern,
der geparkte Eintrag bleibt dann erhalten. Vollständigen Zustandsvertrag herstellen.
inspect kann über Stood lazy Renderaufbau auslösen; settled prüft nur Weltstreaming,
keine allgemeine Renderbereitschaft. Öffentliche Dokumentation muss dies klar trennen.
run() enthält weder Pacing noch Ereignisverarbeitung oder expliziten Abbruch, sondern
eine advance-Schleife bis Fehler. Host-gesteuerte Ausführung bleibt der nutzbare Pfad;
den öffentlichen Komforteinstieg durch einen nachweisbaren Lifecycle ersetzen oder
mit vollständig migrierten Aufrufern entfernen. Keine implizite Endlosschleife als SOLL.
## Bereits geprüfte Übergänge
ViewBook/InputMap publizieren bei Erfolg; SameRenderPlan vergleicht fünf Parameter.
API-Regressionen und Negativkontrollen bestehen; Nachweise in Git.
Offen: Welt-/GPU-Rollback, View-Werte, TimeScale, Joins/Overriding/Surfacing und
Velocity-Gültigkeit (Hintergrund -10000 ist kein Bewegungsvektor).
## Renderer-Neuinitialisierung
Init prüft GPU-Wait vor Umbau, erneuert Frame-/Temporalzustand und Offscreen-Ziel;
Transmission folgt dem aktuellen Plan. Stage-Konfiguration/Passzuordnung sind getrennt.
Sky-Pipeline übernimmt tatsächliche Farbattachments; Normal-/Identitätsziele maskiert.
183 Checks normal/validiert: Framegültigkeit, Wait/Retry, Größen-/Temporalwechsel
pixelgleich zu frischer Instanz und Transmission an/aus/an. Altcode-Negativkontrollen
belegen Frame-/Transmissionfehler; feste Sky-Attachments verursachten Metal-Abbruch.
Sechs GPU-Regressionen bestehen; Wien visuell geprüft, 0/921600 Pixel verändert.
Clang-tidy 97 → 96. Vollständiger Allokationsrollback, Freigabe entfallener
Planressourcen und übrige Readback-Gültigkeitsverträge bleiben offen. Details in Git.
