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
  Parameter und Graphkanten beim Setup kompilieren; Voiced erzeugt derzeit pro Block
  Hashmap und Sample-Vektoren und reserviert Delay-Speicher beim Mischen. Scratch-/
  Delay-Budgets vorab bereitstellen, unbekannte/zyklische Kanten ausdrücklich ablehnen.
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

## Audio-Routing

BusGraph::Build leert den bisherigen Graphen vor der Validierung und akzeptiert
nicht endliche Gains. Kandidatenaufbau mit separaten Bus-/Routing-/Soundphasen,
Publikation erst nach vollständigem Erfolg. Ungültige IDs, Ziele, Zyklen, Budgets
und Zahlen müssen den vorherigen Graphen samt Stimmenzähler erhalten. Unabhängiger
Audio-Test prüft Ablehnung, Weiterverwendung und erfolgreichen Ersatz.
Mixer::Stands baut Routing, Quellen, Hall und Laufzustand als Kandidat auf; Abtastrate
und Besitzwechsel erst nach Erfolg. Kontrollmixer prüft identische Folgeblöcke nach
abgelehnter Änderung von Rate/Routing und Zustandserhalt bei später Quellenablehnung.

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
44 Zahlen-Checks einschließlich begrenzter Views ohne Nullterminierung. Vier Client-
Checks samt Compiler-Oracle; Subprozesse unterscheiden Parse-Ablehnung von injiziertem
SDL-Startfehler. Aktueller Gesamtlauf: 212 Tidy-Befunde; keine gelockerten Prüfungen.
Negativkontrolle entfernt die Endzeigerprüfung: acht Zahlentests und acht CLI-Fälle
werden rot. Vollständigkeitsprüfung wiederhergestellt. Compiler-Oracle: Ergebnis
auswerten kompiliert, ignorieren scheitert an unused-result unter den echten Clientflags.
Abschlusslauf: vier Client-Prüfungen und 29/29 Konventionstests grün. Der bekannte
intermittierende Mipmap-Fehler bleibt trotz dieses grünen Laufs offen (2179).
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
