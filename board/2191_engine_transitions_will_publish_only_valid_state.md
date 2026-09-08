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

## Aktiver Schritt: Kamera-Bereitschaft

Live::Aimed_ startet true; Build bindet eine Standardbasis, aber ohne Geometrie
keine Projektion. Draw überspringt dadurch Look und erreicht Lens::Projection mit
FovDeg=0. Zustände Unbound/Bound/Dirty trennen fehlende Bindung von ausdrücklich
angefordertem Neu-Framing. Erfolgreicher Submit bindet Unbound, erhält aber Dirty;
Build ohne Geometrie invalidiert Bound. Erst erfolgreiches Look beendet Dirty.
Kein pauschales Neu-Framing einer bereits korrekt gebundenen glTF-Kamera.
SwapChain::extent muss vor erfolgreicher Target-Konfiguration null liefern, wie
öffentlich dokumentiert; interne Default-Dimensionen sind keine gültige Oberfläche.

Fehlende Kamera ohne ableitbare Objekt-Bounds muss im bestehenden Look-/Aim-Pfad
als Fehler zurückkommen. Wiederholter Versuch bleibt sicher; nach vollständiger
Konfiguration muss Rendern gelingen. Bestehende numerische Projektionsprüfung nutzen.
Tests: leere Szene vor erster advance(), Wiederholung, Readback und Fenster-Ende;
Recovery mit expliziter Kamera und normale Kamera-/Pixel-Regressionssuite.
Negativkontrolle: voreilige Aimed-Bereitschaft wiederherstellen; der Consumer-Test
muss den bisherigen Assertion-Abbruch erkennen. Assertions bleiben bestehen.

## Weitere konkrete Lücken

DrawsInto ändert Dimensionen/Target, baut aber die planabhängigen Frame-Attachments
und Present-Pipeline nicht als zusammenhängenden Kandidaten neu auf. Größen- und
Formatwechsel müssen dieses Ressourcenpaket atomar ersetzen, nicht nur das Target.
Noch kein Nachweis korrekter Pixel nach einem solchen Wechsel.

Beobachtet: declare einer Surface-Szene ohne vorbereitete Kamera, danach
beginFrame/endFrame, erreicht die Lens::Projection-Assertion. Fehlende Vorbereitung
oder ungültige Projektion vor GPU-Arbeit als Fehlerwert abweisen; erlaubte Reihenfolge
öffentlich dokumentieren. Der Target-Test benutzt eine vollständig vorbereitete Kamera.

## Abnahme

- [x] Target-Kandidaten vor Veröffentlichung vorbereiten; SDL-Fehlertexte besitzen
      Speicher. Fenster-Claims und Offscreen-Textur bleiben bei Ablehnung erhalten.
- [x] Target-Fehlergrenzen und gültige Fenster-/Offscreen-Pfade: 44 Consumer-Checks.
- [x] Vorzeitiges Targeted-Publizieren erzeugt genau einen Fehler im 44-Check-Oracle.

- [ ] Öffentliche Übergangstabelle nennt erlaubte Reihenfolge und Fehlergarantien.
- [ ] Fehler an jeder Build-/Validate-/Publish-Grenze injizieren; gültiges altes
      Szenario bleibt nutzbar oder ausdrücklich Failed, nie halb veröffentlicht.
- [ ] Irrelevantes Event ist Ignored; echter Fehler trägt passenden Code/Kontext.
- [ ] Wiederholtes Declare, Targetwechsel und Featurewechsel ohne Ressourcenwachstum.
- [ ] Negativkontrolle publiziert vor Validierung; Zustandserhalt-Oracle wird rot.
