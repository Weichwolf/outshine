Type: bug
Depends: 2211
State: active
Area: include, scenario
Tags: measured, gate, door
Supersedes: 2107

# Öffentliche API erfüllt dokumentierte Verträge; Szenarien bleiben serialisierbar

## Ziel und Zuständigkeit

Jede öffentliche Entität dokumentiert Einheiten, Koordinaten, Ownership, Lebensdauer,
Invalidierung, Threadbindung, Vorbedingungen, Kosten und Fehlergarantien soweit relevant.
Implementierung gegen WI 2188 prüfen; Dokumentation eines Mangels behebt ihn nicht.
WI 2211 blockiert die vollständige Umsetzung deklarierter Provider, nicht die übrige
API-Arbeit. Zustandsübergänge: WI 2191; Exceptions/Budgets: WI 2194; Lint: WI 2093.
Benchmark-Orchestrierung bleibt im Client, die Engine liefert nutzbare Laufzeitverträge.

## Offene Arbeit

- Öffentliche Header anhand aktueller Doxygen-Diagnosen vollständig prüfen; explizite
  Parameter-/Rückgabetags ergänzen. Keine Namenswiederholung als Vertrag ausgeben.
- Declare/Read/Restore/Layer-Pfade auf Teilmutation prüfen. Kandidaten validieren und
  Fehler mit Ursache weiterreichen; stabile Vorgängerzustände ausdrücklich nachweisen.
- Alle deklarierten, unterstützten Szenariowerte verlustfrei schreiben. Reader,
  Writer und Grammatik abgleichen; keine Funktion zum Bestehen des Guards entfernen.
  Abgeleitete Laufzeitwerte gehören nicht in die Eingabegrammatik.
- Unacted-Einträge durch wirkliche Umsetzung oder ausdrückliche Ablehnung ersetzen;
  keine still getragenen Abschnitte oder doppelten Diagnosen. Provider nach WI 2211.
- Loading-Messfelder passend zu MiB/Mibit/s benennen; Durchsatz aus demselben
  Messintervall ableiten, statt kumulierte Poolbytes durch einzelne Preload-Zeit teilen.
- Root-/Providerwechsel als validierten Lebenszyklus behandeln; aktuelle Setup-
  Vorbedingung migriert vorhandene Ressourcen nicht und wird nicht erzwungen.
- Event-Vertrag: Scrolländerung, Ereigniskoordinaten und Richtung prüfen.
  Bindungsdispatch ohne Renderer sowie UI-Fallback/Priorität sind dynamisch geprüft.
- Terrainabfragen: Residency von möglicher Tile-Vorbereitung trennen; interne
  Koordinatengrenzen prüfen. Öffentliche Höhenabfrage validiert bereits Winkel/Abdeckung.
- Zeitsteuerung: Überlastpolitik und Frame-Arbeitsbudget bestimmen, Restzeit und
  Zustände bei fehlgeschlagenen Schritten prüfen. Positive endliche StepS und gültige
  Nachhollimits allein sind kein Echtzeitnachweis.
- Diagnose-API: Legacy-error nicht als Ergebnis letzter expected-Aufrufe verwenden.
  Messwerte können veraltet sein; Snapshot-/Frischevertrag bei Nutzung berücksichtigen.
  Timing-Ring-Allokation ist noch nicht budgetiert oder gemeinsam transaktional.

## Abnahme

- Vollständiger make lint mit null clang-tidy- und Dokumentationsdiagnosen;
  alle öffentlichen Header erfasst. Grüne Zähler ersetzen keine Vertragsprüfung.
- Unabhängige öffentliche API-Tests für Fehler, Wiederverwendung und Lebensdauer;
  Mutationskontrollen müssen am beobachteten Verhalten scheitern, nicht am Build.
- Reader → Writer → Reader erhält die deklarierte Bedeutung für alle unterstützten
  Abschnitte, einschließlich Defaults, optionaler Werte und Layer. Erst eine
  kanonische Serialisierung darf zusätzlich byte-identische Ausgabe verlangen.
- Writer-Negativkontrolle entfernt gezielt ein unterstütztes Feld; der entsprechende
  Rundlauf muss scheitern. Ein statischer Grammatikzähler allein reicht nicht.
- Bestehende Regressionen für Loading-Überlauf, Preload-Budgets, Readback-Auswahl,
  Event-Ergebnisse, Höhenkoordinaten, Zeitwerte und exakte Nachhollimits erhalten.
  Implementierungsverlauf und einzelne Messergebnisse bleiben in Git/Temp-Logs.
- Bildwirksame Änderungen über den Client rendern und PNGs selbst prüfen;
  CPU-Messzeiten nicht als GPU-Ausführungs- oder Präsentationslatenz ausweisen.

## Verbleibende konkrete Lücken

- InputMap und declare erhalten Bindungen bei Fehlern; Views, deklarierter Zustand
  und Renderer können weiterhin teilweise verändert werden. Szenario-Publikation
  nach WI 2191 gemeinsam transaktional machen; Eingaben dürfen nicht isoliert bleiben.
- Wheel nutzt die Ereignisposition; Richtung und Scrollgrenzen sind geprüft.
  HiDPI-Umrechnung und nichtendliche Event-/WheelStep-Werte noch validieren.
- XML-Attribute dekodieren Referenzen und normalisieren Whitespace; Literal-UTF-8
  sowie Elementtext sind noch nicht vollständig geprüft. Keine XML-Konformität behaupten.
- Motion.Dial bleibt gespeichert, Time.Rate ungenutzt; laufende astronomische Zeit
  nach WI 2213 anbinden. Die aktuelle Sonnenzeit wird bei declare berechnet.

## Bestandsschutz durch Verhaltenstests

ReadScenario publiziert erst nach vollständiger Prüfung; Fehler erhalten den Vorgänger.
Physik-Rundlauf erhält Schrittzeit, Nachhollimit und XML-Sonderzeichen. Referenz:
https://www.w3.org/TR/xml/#AVNormalize und #NT-CharRef.
SDL-Achsen werden an beiden Endpunkten korrekt normalisiert; vollständiger Wertebereich
geprüft. Referenz: https://wiki.libsdl.org/SDL3/SDL_GetGamepadAxis.
InputBindingsDoNotRequireRendering prüft die öffentliche Weiterleitung aller Geräte;
InputBindingsPrecedeUiActions prüft tatsächliche UI-Hits, Bindungspriorität, Host-
Ablehnung, Entfernen der Bindung, fehlenden Host und Hits außerhalb der Oberfläche.
Negativkontrollen gegen Renderer-Sperre bzw. vorzeitigen Abbruch bei ungebundenem
Ereignis müssen am Verhalten scheitern. Das ersetzt keinen Scroll-/Capture-Vertrag.

## Nächster Umsetzungsschritt

InputMap::Build erstellt einen lokalen Kandidaten und tauscht ihn ausschließlich
nach vollständiger Validierung ein. Fehler erhalten alle vorhandenen Event-/Action-
Zuordnungen; Erfolg löscht die vorherige Diagnose. Test: unbekanntes Event nach
gültigem Präfix, doppelte Events, Erholung und leere Map. Engine-declare muss danach
zusätzlich seine vorzeitige Publikation beseitigen; Map-Atomarität allein genügt nicht.

Declare-Input: lokale Map und validierte Pump vorbereiten. Die Pump referenziert
bereits den stabilen Session-Map-Speicher; erst bei Erfolg Map per Nothrow-Move
ersetzen und Pump aktivieren. Alle drei Erfolgszweige berücksichtigen, insbesondere
Generatorfehler nach später Zustandsänderung. Frühe Input-, spätere View- und
Generatorfehler öffentlich nachweisen. Andere Declare-Teilmutationen bleiben WI 2191;
dieser Schritt behauptet keine vollständige Szenario-Transaktion.

Wheel-Schritt: mouse_x/mouse_y aus SDL_MouseWheelEvent verwenden statt aktuellem
SDL_GetMouseState. Referenz https://wiki.libsdl.org/SDL3/SDL_MouseWheelEvent.
SDL hat FLIPPED bereits in x/y angewandt; UI erhält diese Systempräferenz, keine
zweite Umkehrung. Test mit versetztem Scrollcontainer und eingespeisten Events,
beiden Vorzeichen, FLIPPED, Null, außerhalb und beiden Scrollgrenzen. HiDPI-
Umrechnung von Fensterkoordinaten in UI-Pixel für alle Mauspfade separat prüfen.

Wheel-Validierung: WheelStepPx endlich und nichtnegativ; Null deaktiviert Scrollen.
Declare lehnt ungültige Konfiguration vor Mutation ab. Bei aktiver UI müssen
mouse_x/mouse_y/y und der berechnete Pixelweg endlich sein; sonst expected-Fehler
vor Scrollmutation. Test NaN/±Inf je konsumiertem Feld, Multiplikationsüberlauf,
Nullkonfiguration und gültige Folgeereignisse als Zustandsnachweis.
