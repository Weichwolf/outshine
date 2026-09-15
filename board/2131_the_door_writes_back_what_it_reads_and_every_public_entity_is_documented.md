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
ReadScenario publiziert erst nach vollständiger Prüfung; Fehler erhalten den Vorgänger.
Physik-Rundlauf erhält Schrittzeit, Nachhollimit und XML-Sonderzeichen. Referenz:
https://www.w3.org/TR/xml/#AVNormalize und #NT-CharRef.
SDL-Achsen werden an beiden Endpunkten korrekt normalisiert; vollständiger Wertebereich
geprüft. Referenz: https://wiki.libsdl.org/SDL3/SDL_GetGamepadAxis.
Input-Regressionen prüfen Rendererunabhängigkeit, UI-Hits, Bindungspriorität und Host-
Ablehnung; Negativkontrollen gegen Renderersperre/frühen Abbruch scheitern am Verhalten.
Mind/Region/Door geprüft: Metadaten; Layer ersetzt per Region-ID bzw. Door-Endpaar.
Standing/Placement/Surface geprüft. Restliche Asset-/Audiofelder anhand Loader,
Overrides, Mixer/BusGraph dokumentiert; wirkungslose Felder und Grenzen ausdrücklich benannt.
## Öffentliche Welt-/Generatorverträge korrigieren
Georeference::RadiusM hat Erdradius als Default, wird aber von Engine::generated als
Request::ExtentM weitergegeben; der Regionsvertrag ist uneindeutig. Structures nutzt
inzwischen einen eigenen widthM-Parameter. Reader/Writer erhalten radiusM. Generating::Parameters werden serialisiert und als geliehene native
Parameter weitergereicht. Radius-/Extent-Kopplung bleibt ein ungültiger SOLL-Vertrag;
nicht durch Dokumentation oder identische Umbenennung legitimieren.
Mit 2126: Georeferenz = Position/Referenzrahmen; Generierungsregion mit einheitlichem
Raumvertrag. Objektmaße bleiben native Generatorparameter/Feature-Grundrisse.
Schema, Reader, Writer, Public API und Engine-Aufrufer gemeinsam migrieren. Altes
world.radiusM nicht still neu interpretieren: als ungültigen Legacy-Vertrag ablehnen
oder explizit migrieren. Keine Ortsdaten im Code. API-Dokumentation erklärt die neuen
Einheiten, Reichweite, Ownership und Fehlerpublikation nach Implementierungsprüfung.
Abnahme: öffentlicher registrierter Probe-Generator sieht deklarierte Region/Parameter;
Reader/Writer-Rundlauf erhält sie; Fehler bewahrt Vorgängerszene. Gebäudemaße bleiben
bei geänderter Generierungsregion gleich. Fehlende Weiterleitung als Negativkontrolle.
## Dokument als native Deklaration
Document::subject entfernt: glTF-Auswahl gehört zum internen Assetpfad, nicht zur
öffentlichen Datenstruktur. Owned Container, Lebensdauer und Deklarationsgrenzen
dokumentieren; interne First-glTF-Auswahl bleibt bis zur Assetmigration offen.
Body-Deklaration dokumentiert; ungenutzte Such-/Geometriehelfer entfernt.
Kind-/Instanznamen vor Aufbau auf leer/doppelt geprüft; getrennte Namensräume.
API-Verträge dokumentiert; Altcode rot, drei Assembly-Regressionen mit Fehlererhalt grün.
Body-/Kontakt-/Antriebsverträge geprüft; Anbindung, Kraftüberläufe, LoadFalloff und
CircleM-Radius/Durchmesser bleiben offen. Player- und UI/Capacity-Writer erhalten
Werte mit geprüften Rundläufen und Negativkontrollen; Nachweise in Git.
Player-Starts/Augenhöhe/Geh-/Laufgeschwindigkeit bleiben ohne Runtime-Anbindung.
Region-/Placement-Export erhält geprüfte Metadaten, lokale/geografische Pose und
Skalierungsachsen; unabhängige Rundläufe und Negativkontrollen grün. Details in Git.
Geometrie-/Referenzvalidierung und Runtime-Anbindung bleiben offen.

Rohdokumente exportieren Layer-ID/Pfad/Set in Reihenfolge. Engine::readScenario
konsumiert Referenzen nach erfolgreicher Auflösung; Export enthält den Snapshot.

## Antriebsvertrag
Import/Export/Declare/Assembly nutzen denselben geprüften Antriebsvertrag.
Ungültige native Drives werden vor Publikation abgelehnt. Gemeinsamen noexcept-Validator
für Kategorie, endliche Zahlen, nichtnegative Peaks/CircleM, nichtverschwindende
endliche Achse und passenden Kraft-/Drehmomentkanal nutzen. Ratio darf null/negativ
sein; keine unbewiesene Getriebepolitik. Lineare Nullkraft bleibt gültig.
Reader muss vollständige endliche Zahlen lesen, keine Ersatznull für ungültige Tokens.
Alle vier Grenzen (Import/Export/Declare/Assembly) prüfen denselben nativen Vertrag;
Ablehnung erhält vorige Deklaration/Simulation. Kein Aufbau einer Fahrzeugphysik.
Abnahme: beide Kategorien/Modi, Nullwerte, NaN/Inf, negative Peaks, falscher Kanal,
Nullachse, ungültiges Enum, malformed XML und Fehlererhalt; Gegenprobe ohne Validierung.
Fünf Tests grün; entfernte API-Prüfung verletzt 42 Checks. Lint vollständig grün.
