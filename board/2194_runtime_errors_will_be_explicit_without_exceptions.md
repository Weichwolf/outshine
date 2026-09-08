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

- Mixer::Named nutzt stod/catch: nichtwerfendes Parsing mit definiertem Vertrag für
  Syntax, Restzeichen, Wertebereich und nicht endliche Eingaben; Consumer prüfen.
- Asking::Instancing::Add fängt vector-Allokation: begrenzte Kapazität vorbereiten,
  Budgetablehnung ohne partielle Veröffentlichung. Nicht nur catch entfernen.
- BuildingMesh::Mesh fängt alles und leert Raised: Scratch-/Output-Budgets und
  transaktionalen Fehlerpfad erhalten, Caller auf typisierte Fehler umstellen.
- Alle Laufzeitaufrufe prüfen: filesystem, format, Containerzugriffe, expected::value,
  Allokation, Fremdbibliotheken und Callbacks. Fataler systemweiter OOM ist getrennt
  von behandelbarer Streaming-Budgeterschöpfung; Fehlerdiagnosen dürfen kein
  unbeschränktes Allokieren voraussetzen.
- Exceptionpflichtige Fremdpfade nur in abgegrenzten Adaptern mit eigener
  Compilerkonfiguration; dort übersetzen, bevor die Engine aufgerufen wird.
- Danach -fno-exceptions für eigene Runtime/Generatoren und deren Tests aktivieren;
  Build und Compile-Datenbank müssen dieselbe Konfiguration führen. Tools und
  Drittanbieter explizit prüfen, keine pauschale Flag-Vererbung.

## Aktiver Schritt: vollständige CLI-Zahlenkonvertierung

Main::AskHeight nutzt atof und initialisiert SDL vor Prüfung der Koordinaten.
Gemeinsamer kleiner Parser in base/format: string_view, nodiscard expected<double,
NumberError>, noexcept; from_chars statt Locale/Exceptions/temporärer Strings.
Akzeptiert endliche dezimale Zahlen samt Vorzeichen und Exponent; keine Rand-Leerzeichen,
Restzeichen, NaN/Inf oder Über-/Unterläufe. Keine Ersatznull bei ungültiger Eingabe.
height verlangt exakt zwei Argumente, Latitude in [-90,90], Longitude in [-180,180];
Ablehnung vor Engine-/SDL-/Provider-Aufbau. Gültige Abfrage bleibt unverändert.
Unabhängige Zahlen-Oracles einschließlich begrenzter Views ohne Nullterminierung;
Client-Subprozesse unterscheiden Parse-Ablehnung von injiziertem SDL-Startfehler.
Negativkontrolle mit altem atof-/Initialisierungspfad muss die Ablehnungschecks verletzen.
Weitere Consumer (Mixer/XML/render-CLI) anschließend mit ihren eigenen Fachverträgen
migrieren; dieser Schritt behauptet weder vollständige Runtime- noch Audio-Abnahme.

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
