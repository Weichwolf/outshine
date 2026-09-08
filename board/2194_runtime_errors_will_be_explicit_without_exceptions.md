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
