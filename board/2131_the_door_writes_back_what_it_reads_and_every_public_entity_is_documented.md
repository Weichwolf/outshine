Type: bug
Parent: 2188
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
  Nichtendliche konsumierte Werte und Pixelweg-Überlauf werden abgelehnt;
  HiDPI-Umrechnung sowie horizontales Scrollen bleiben offen.
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

## Öffentliche Welt-/Generatorverträge korrigieren

Binding-Dokumentation, leere Aktionsnamen, zustandsloser InputPump und getrennte
Geräteübersetzung sind umgesetzt; vorhandene Geräte-/UI-/Fehlererhaltstests behalten.

Georeference::RadiusM hat Erdradius als Default, wird aber von Engine::generated als
Request::ExtentM weitergegeben; der Regionsvertrag ist uneindeutig. Structures nutzt
inzwischen einen eigenen widthM-Parameter. ReadWorld liest radiusM, WriteScenario verliert es. Generating::Parameters werden inzwischen serialisiert und als geliehene native
Parameter weitergereicht. Radius-/Extent-Kopplung bleibt ein ungültiger SOLL-Vertrag;
nicht durch Dokumentation oder identische Umbenennung legitimieren.

Mit 2126 umsetzen: Georeferenz enthält Position/Referenzrahmen, keine Objektgröße.
Generierungsregion hat einen einheitlichen räumlichen Vertrag; Gebäudeabmessungen sind
Generatorparameter bzw. stammen aus Feature-Grundrissen. Vorhandene owned Scenario-
Parameter über formatunabhängige, für make geliehene Daten weiterreichen; Generation
darf nicht vom Szenarioparser abhängen. Unbekannte/ungültige Parameter ausdrücklich
ablehnen. Kein Ignorieren und keine stillen größenabhängigen Ersatzwerte.

Schema, Reader, Writer, Public API und Engine-Aufrufer gemeinsam migrieren. Altes
world.radiusM nicht still neu interpretieren: als ungültigen Legacy-Vertrag ablehnen
oder explizit migrieren. Keine Ortsdaten im Code. API-Dokumentation erklärt die neuen
Einheiten, Reichweite, Ownership und Fehlerpublikation nach Implementierungsprüfung.
Abnahme: öffentlicher registrierter Probe-Generator sieht deklarierte Region/Parameter;
Reader/Writer-Rundlauf erhält sie; Fehler bewahrt Vorgängerszene. Gebäudemaße bleiben
bei geänderter Generierungsregion gleich. Fehlende Weiterleitung als Negativkontrolle.

## OSM-Koordinaten erhalten
WriteScenario rundet LatLon mit to_string auf sechs Nachkommastellen; unterschiedliche
Punkte können zusammenfallen. std::format mit Roundtrip-Darstellung wie Number nutzen.
Test vergleicht ursprüngliche Double-Werte mit gelesenen Werten: nahe Punkte,
negative Koordinaten, Pole/Datumsgrenze und Weg/Fläche; alter Writer muss scheitern.
Reader-Validierung, verlustbehafteter uint64-Reliefseed und fehlende Attribute bleiben offen.
