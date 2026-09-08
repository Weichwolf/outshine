Type: bug
State: active
Area: engine, include, scenario
Tags: architecture, state, errors
Parent: 2188
Depends: 2210

# Engine transitions will publish only valid state

## Befund und Entscheidung

Declaring.cpp handleEvent gibt für irrelevante Events unexpected(S_->Error) zurück:
nicht behandelt und Fehler sind vermischt, Error kann leer oder veraltet sein.
EngineHeld.h verteilt Phasen über Taken, Targeted, FrameOpen, Carrying, Mixing usw.
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
und nicht bewegliche Engine-Owner sind die geprüfte Voraussetzung.

## Geprüft: gemeinsame Szenario-Kameraprojektion

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
Konventionssuite zuletzt 27/28: nur bekannter Mipmap-Fehler aus 2179 rot
(420 Kanäle, max. 0,00268555, Tiefe identisch), kein gelockerter Test.
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

Carries aktualisiert die Kamera nur bei Körperindex 0; View.Follows wird nur gelesen,
geschrieben und auf nichtleeren Namen geprüft, nicht gegen den Zielkörper aufgelöst.
Ziel beim Assemble eindeutig auflösen; unbekannte/mehrdeutige Namen ablehnen, aktive
View an den richtigen Körper binden. Zwei getrennte bewegte Körper und Viewwechsel
müssen das Ziel unabhängig von Deklarationsreihenfolge zeigen. Quellbefund, noch kein
Mehrkörper-Laufzeitnachweis; der Projektions-Test verwendet ausdrücklich einen Körper.

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
- [ ] Irrelevantes Event ist Ignored; echter Fehler trägt passenden Code/Kontext.
- [ ] Wiederholtes Declare, Targetwechsel und Featurewechsel ohne Ressourcenwachstum.
- [ ] Negativkontrolle publiziert vor Validierung; Zustandserhalt-Oracle wird rot.
