Type: debt
State: active
Area: build, include, engine, generators, audio
Parent: 2188
Depends: 2209
# Runtime errors will be explicit without exceptions

## Entscheidung

Nutzerentscheidung: exceptionsfreie Engine-Runtime; erwartete Fehler über
`[[nodiscard]] std::expected<T, Error>`, statische Invarianten über `static_assert`.
Vorhandene expected-/RAII-Verträge nutzen. Kein mechanisches noexcept an jede
Funktion, keine entfernten Fehlerprüfungen als Ersatz für einen Fehlervertrag.
SDL3 verlangt keinen C++-Exception-Pfad. Die Core Guidelines E.25/E.26 beschreiben
explizite Fehlerbehandlung ohne Exceptions; F.6 beschreibt noexcept-Verträge:
https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Re-no-throw
https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rf-noexcept

## Befund und Umsetzung

- Audio-Parameter sind nichtwerfend vorbereitet. Graphen beim Setup in ganzzahlige
  Kanten und eine deterministische topologische Reihenfolge kompilieren; eindeutige
  IDs, vorhandene Ziele und Zyklen prüfen. Letzter deklarierter Knoten bleibt Ausgang,
  nicht letzter ausgeführter Knoten. Vorwärtsreferenzen sind gültig. Setup begrenzt
  alle Quellen zusammen auf 1024 Knoten/4096 Kanten (gesetzte Enginebudgets).
  Analytische Signale, Permutationen und Ablehnungen mit Zustandserhalt prüfen.
  Scratch beim Setup für den größten Quellgraphen vorbereiten; Verarbeitung in
  256-Frame-Teilstücken ohne zusätzliche Ausgabelatenz. Maximal 1024 × 256 × 8 Byte
  Knotenspeicher (2 MiB), dazu drei 256-Sample-Double-Puffer (6 KiB). Gesetztes Budget,
  kein Hardwaremesswert. Erster Block und variable Größen: keine C++-Heap-Allokation;
  zusammenhängende und aufgeteilte Ausgabe müssen inklusive Delay/Hall übereinstimmen.
- Instanzübernahme: eigener WorldInstanceSink schreibt in vorbereiteten span; Add
  allokiert nicht. Shipped-DrawSources liefern höchstens eine Instanz pro platziertem
  Körper; daran Setupgröße ableiten, insgesamt höchstens 2^20 Instanzen. Mehrbedarf
  explizit ablehnen. Kandidaten und Placed/Instanced-Zähler erst nach Draw-Erfolg
  veröffentlichen. Kapazität, Datenzuordnung und allokationsfreier Callback prüfen;
  absichtlich eingefügte Allokation muss den Test brechen. System-OOM bleibt separat.
- BuildingMesh::Mesh liefert bereits expected und rollt angehängte Geometrie bei
  Fehlern zurück. Verbliebenes catch(...) erhält Allokationsfehler; erst durch
  explizite Scratch-/Output-Allokationsfehler ersetzen, dann exceptionsfrei bauen.
- Alle Laufzeitaufrufe prüfen: filesystem, format, Containerzugriffe, expected::value,
  Allokation, Fremdbibliotheken und Callbacks. Fataler systemweiter OOM ist getrennt
  von behandelbarer Streaming-Budgeterschöpfung; Fehlerdiagnosen dürfen kein
  unbeschränktes Allokieren voraussetzen.
- Exceptionpflichtige Fremdpfade nur in abgegrenzten Adaptern mit eigener
  Compilerkonfiguration; dort übersetzen, bevor die Engine aufgerufen wird.
- Danach -fno-exceptions für eigene Runtime/Generatoren und deren Tests aktivieren;
  Build und Compile-Datenbank müssen dieselbe Konfiguration führen. Tools und
  Drittanbieter explizit prüfen, keine pauschale Flag-Vererbung.

## Audio-Routing

BusGraph::Build leert den bisherigen Graphen vor der Validierung und akzeptiert
nicht endliche Gains. Kandidatenaufbau mit separaten Bus-/Routing-/Soundphasen,
Publikation erst nach vollständigem Erfolg. Ungültige IDs, Ziele, Zyklen, Budgets
und Zahlen müssen den vorherigen Graphen samt Stimmenzähler erhalten. Unabhängiger
Audio-Test prüft Ablehnung, Weiterverwendung und erfolgreichen Ersatz.
Mixer::Stands baut Routing, Quellen, Hall und Laufzustand als Kandidat auf; Abtastrate
und Besitzwechsel erst nach Erfolg. Kontrollmixer prüft identische Folgeblöcke nach
abgelehnter Änderung von Rate/Routing und Zustandserhalt bei später Quellenablehnung.

Audio-Qualität, Stereo-/Kopfhörer-Ausgabe und Backend-Evaluation: WI 2212.

## SDL3_mixer als Wiedergabebasis

Lokal SDL3_mixer 3.2.4 vorhanden; noch keine integrierte Outshine-Abhängigkeit.
WI 2212 priorisiert Synthese und Zweikanalqualität. SDL3_mixer für ergänzende
Standarddekodierung, Tracks und Ausgabe evaluieren; keine Vorfestlegung des DSP-Backends. MIX_CreateMixer
mit Float-Stereo und MIX_Generate passt zum vorhandenen Engine::mix-Speichervertrag.
Rückgabe >= 0 bedeutet Erfolg, auch wenn nur angehängte Stille geliefert wurde.
https://wiki.libsdl.org/SDL3_mixer/MIX_Generate
Outshine behält Weltposition/Listener, Quellenbudget, Verdeckung und Akustiksteuerung.
SDL-3D ist listenerrelativ, mischt Quellen mono und liefert kein Doppler oder frei
wählbare Distanzmodelle: https://wiki.libsdl.org/SDL3_mixer/MIX_SetTrack3DPosition
Migration durch unabhängige PCM-/WAV-Fixtures, Blockkontinuität, Gain, Stop/Loop,
Gerätelosigkeit und Fehler-/Lifetime-Tests beweisen. Synthese und vorhandene
Szenariofähigkeiten erhalten; eigener DSP nur für nachgewiesene Backendlücken.
Vor Runtime-Umbau Buildabhängigkeit/Version und Init-/Shutdown-Ownership festlegen.
Keine behauptete Echtzeitgarantie aus SDL-Thread-Safety; Allokation/IO separat messen.

## Vorbereitete Audio-Parameter

Numeric-Parameter vor Publikation mit ParseFiniteNumber validieren und in nativen
Voice-Werten speichern; kein stod/catch oder Stringparsing im Audioblock. Negative
Delayzeiten und nicht endliche Werte ablehnen. Delay-Ringe beim Setup reservieren,
gemeinsam auf 8 Mi Samples Double begrenzen (64 MiB); Budget ist eine Enginegrenze,
kein Hardwaremesswert. Fehler müssen den laufenden Mixer erhalten. Hall-Setup,
Quellvirtualisierung und vollständige Echtzeitmessungen bleiben offen. Hallparameter
vor Publikation validieren: endliche RT60 >= 0, Damping/WetShare in [0,1].
Hallringe zusammen auf 8 Mi Double-Samples (64 MiB) begrenzen; Abtastraten dürfen
dieses Setupbudget nicht umgehen. Abgelehntes Hall-Setup erhält Rate und Laufzustand.

## Vollständige CLI-Zahlenkonvertierung

QueryTerrainHeight validiert die Koordinaten jetzt vor Engine-/SDL-Initialisierung.
Der vorherige atof-Pfad akzeptierte unter anderem Zahlenpräfixe und Ersatznullen.
C++-Vertrag: https://eel.is/c++draft/charconv.from.chars
Gemeinsamer kleiner Parser in base/format: string_view, nodiscard expected<double,
NumberError>, noexcept; from_chars statt Locale/Exceptions/temporärer Strings.
Akzeptiert endliche dezimale Zahlen samt Vorzeichen und Exponent; keine Rand-Leerzeichen,
Restzeichen, NaN/Inf oder Über-/Unterläufe. Keine Ersatznull bei ungültiger Eingabe.
height erhält einen geliehenen span der Argumente und verlangt exakt zwei, Latitude in [-90,90], Longitude in [-180,180];
Ablehnung vor Engine-/SDL-/Provider-Aufbau. Gültige Abfrage bleibt unverändert.
Unabhängige Parser-/CLI-Fälle und Negativkontrolle ohne Endzeigerprüfung sichern den
Vertrag; ignorierte Ergebnisse scheitern unter den Clientflags. Weitere Consumer
mit ihren eigenen Fachverträgen migrieren. Keine vollständige Runtime-Abnahme.

## Abnahme

- [ ] Runtime und Generatoren nachweislich ohne Exceptions gebaut und getestet.
- [ ] Fehlerergebnisse nodiscard; Ignorieren scheitert als Compiler-Negativkontrolle.
- [ ] Statische Ownership-/Layout-/Zustandsinvarianten passend abgesichert.
- [ ] Ungültige Eingaben und ausgeschöpfte Budgets liefern Fehler ohne Teilzustand.
- [ ] Bibliotheks-/Callback-Grenzen dokumentiert und inklusive Fehlerpfaden geprüft.
- [ ] Fataler OOM-Vertrag dokumentiert; keine behauptete OOM-Erholung durch expected.
- [ ] make lint einschließlich clang-tidy und betroffene Make-Suiten ausgeführt;
  neue Fehlerpfade mit wirksamen Negativkontrollen geprüft.

Parallel zum fachlichen Tidy-/API-Umbau aus 2093 abarbeiten. Keine erwartete Änderung
gültiger Renderbilder; geometrische Verhaltensänderungen erfordern PNG-Abnahme.
