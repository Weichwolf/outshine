Type: bug
State: active
Area: engine, include, scenario
Tags: architecture, state, errors
Parent: 2188
Depends:

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

## Aktiver Schritt: geprüfte GPU-Projektion

Khronos definiert die Kamera-Halbausdehnungen und Near/Far-Bedingungen:
https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#cameras
SetProjection prüft bisher Double, konvertiert aber ungeprüft auf Float. Endliche
Werte können überlaufen, positive Werte zu null werden und Near/Far zusammenfallen.
Aim verändert die Projektion außerdem vor der noch fehlbaren Near-Plane-Prüfung.

Lens erzeugt einen geprüften Kandidaten als nodiscard expected<Lens, LensError>,
allokationsfrei und noexcept. Vor Konvertierung Wertebereich prüfen; danach gültige
Float-Intervalle und endliche, nicht degenerierte Projektionskoeffizienten prüfen.
Ungültige Kameraart und negative orthographische Near-Ebene ablehnen. Erst nach
vollständiger Aim-Prüfung Projektion und Basis gemeinsam veröffentlichen.
Bestehende reverse-Z-Mathematik und gültige Bilder bleiben unverändert.
Prüfung: unabhängige Near/Far-Abbildung, Float-Überlauf/Unterlauf, zusammenfallende
Ebenen, ungültige Kameraarten; Consumer erhält alte Kamera nach Ablehnung.
Negativkontrolle lässt ungeprüfte Verengung wieder zu und muss Grenzfälle verfehlen.

## Weitere konkrete Lücken

DrawsInto ändert Dimensionen/Target, baut aber die planabhängigen Frame-Attachments
und Present-Pipeline nicht als zusammenhängenden Kandidaten neu auf. Größen- und
Formatwechsel müssen dieses Ressourcenpaket atomar ersetzen, nicht nur das Target.
Noch kein Nachweis korrekter Pixel nach einem solchen Wechsel.

Advancing ersetzt auch NaN/negative Projektionswerte durch Defaults; der mitgeführte
Kamerapfad übernimmt Near/Far/Orthographic nicht vollständig. Gemeinsame Abbildung
für beide Pfade herstellen: nur erklärte Auslassungswerte defaulten, ungültige Werte
ablehnen. Double-zu-Float-Grenzen des Renderers dabei prüfen.

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
