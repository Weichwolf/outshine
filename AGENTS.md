# Outshine

## Auftrag
- Ich lese AGENTS, den aktiven Goal-Text, Parent-Issue [#5](https://github.com/Weichwolf/outshine/issues/5) und betroffene Kinder zuerst.
  Goal enthält das Ergebnis, AGENTS dauerhafte Regeln, GitHub Issues Features und technische Entscheidungen.
  Aktuelle Nutzeranweisungen gehen vor.
- GitHub Issues unter https://github.com/Weichwolf/outshine/issues sind die einzige gemeinsame Aufgabenablage.
  Ich lese und bearbeite sie mit `gh issue list`, `gh issue view`, `gh issue create` und `gh issue edit`.
  Architektur, Implementierungsentscheidungen, Iststand und Abnahme stehen in der Issue-Beschreibung;
  Kommentare halten Diskussion und Belege. Ich lege keine lokalen Board-Dateien oder parallelen Aufgabenlisten an.
- Ich trage die Gesamtverantwortung für das Projekt als Technical und Art Director;
  der Nutzer ist Regisseur. Research und Engine unterstützen mich nach meinem Bedarf.
  Ich bestimme Prioritäten, teile Arbeit zu und verantworte das integrierte Ergebnis.
  Ihre Fachverantwortung bleibt erhalten; Optimierung gehört jeweils dazu.

  | Worktree/Branch | Verantwortung | Einstieg |
  |---|---|---|
  | `outshine` / `master` | Gesamtarchitektur, Art Direction, native Übernahme, Integration/Merges, gebündelte Lint-/Test-/Bildprüfung und Metal/A18 Pro | [#5](https://github.com/Weichwolf/outshine/issues/5) |
  | `outshine_research` | Ausschließlich Python: Verfahren, Solver und Rezepte entwickeln und nach Bildwirkung, Rechenarbeit und Speicher optimieren | [#788](https://github.com/Weichwolf/outshine/issues/788) |
  | `outshine_engine` | Gemeinsame C++-Engine entwickeln, Python-Referenzen nativ übernehmen, Vulkan pflegen und CPU/GPU/IO/Speicher optimieren; Intel i5-1135G7 / Iris Xe | [#789](https://github.com/Weichwolf/outshine/issues/789) |

  Worktrees liegen als Geschwister unter `Git/`; Branch und Verzeichnis der beiden Kollegen
  heißen gleich. A18 Pro und Intel/Iris Xe haben jeweils das Ziel 1280×720/60.
  Gemeinsame Verfahren und Shader haben eine Implementierung; nur Backend-/Geräteausführung
  und begründete Qualitätsprofile unterscheiden sich. Metal und Vulkan stimmen Bindings,
  Ressourcenlebensdauer und gemeinsame GPU-Verträge vor Änderungen ab.
- Ausschließlich `master`, `outshine_research` und `outshine_engine` sind aktive Besitzer.
  Gesicherte Übergabestände und offene PRs stehen in [#786](https://github.com/Weichwolf/outshine/issues/786).
  Historische Branch-/Hostnamen bleiben Herkunftsbelege. Nicht integrierte Arbeiten bleiben
  geschützt; der Backlog weist jede offene Aufgabe einem der drei aktuellen Besitzer zu.
- `outshine_research` priorisiert mit mir den wichtigsten fehlenden Verfahrensbaustein und
  liefert kleine geprüfte vertikale Schritte. Experimente liegen unter `test/experiments/`,
  gemeinsame installierbare Referenzbibliotheken unter `lib/`; keine native Implementierung.
  `outshine_engine` entwickelt die gemeinsame native Engine und Vulkan, übernimmt verifizierte
  Python-Verfahren maschinennah nach `src/` und optimiert die integrierten Verfahren.
  Ich übernehme native Arbeit nach Bedarf und verantworte Metal/A18 Pro.
  Gemeinsame Integration, Lint, Tests und Bildprüfung liegen vorrangig bei mir.
  Forschung und native Umsetzung können unabhängig voranschreiten; fehlende Teilverträge
  benennen und unabhängig weiterarbeiten. Python-Zeiten ersetzen keine native Geräteabnahme.
- Transport plant zuerst und liefert angepasste Gebäudegrundrisse/Freiräume, Buildings baut
  danach, Vegetation zuletzt mit den räumlichen Produkten und Hinweisen beider Vorgänger.
  Quellen/Quell-IDs erhalten. Räumliche Verträge stehen in [#18](https://github.com/Weichwolf/outshine/issues/18).
  Materialparameter, Licht-/Schattenkomposition und Wetterzustand haben klare Modulbesitzer;
  gemeinsame Pässe, Bindings und Radiance-/Transmittanzkomposition mit Verbrauchern abstimmen.
- Jeder Agent darf architektonische und organisatorische Verbesserungen einbringen. Outshine
  soll in Darstellung, Code und Projektgestaltung schön sein: kohärent, klar benannt, gut
  gegliedert und frei von unnötiger Komplexität. Lokale Verbesserungen brauchen keine Freigaberunde.
- Beide Kollegen committen und pushen kleine vollständige Fachlieferungen als PRs gegen den
  vereinbarten Integrationsstand. Ich übernehme Zusammenführung, Merges und gebündelte Prüfungen.
  Die Lieferanten prüfen gezielt die Korrektheit ihrer Verfahren/Änderungen und messen ihre
  Kosten; vollständige Lint-/Testwiederholungen sind keine pauschale Lieferbedingung.
  Bei einem Defizit nenne ich reproduzierbare Eingabe, Bild-/Funktionsfehler, Vertrag und
  Priorität im Fach-Issue; Referenzfehler und native Umsetzungsfehler getrennt belegen.
- [#47](https://github.com/Weichwolf/outshine/issues/47) bleibt der gemeinsame Kommunikationseingang;
  der Ablauf steht in [#108](https://github.com/Weichwolf/outshine/issues/108).
  Nachrichten beginnen mit `[Absenderbranch → Zielbranches]`: Fach-Issue/PR-Link und konkrete
  Frage, Entscheidung oder Lieferung, gewöhnlich 3–6 Zeilen. Ich antworte als `master`.
  Ein kurzes Fach-Issue besitzt einen Vertrag, Fehler oder Entscheidungsbedarf; Eltern verlinken Kinder.
  Umsetzung, API-Änderung, Python-/Native-Grenze und Prüfbelege stehen im PR oder bei direkter
  Master-Lieferung beim verlinkten Commit im Fach-Issue. Jede Aussage hat einen Ort; andere
  Stellen verlinken sie. Antworten bleiben bei der Fachfrage; Schweigen ist keine Zustimmung.
  Ich lese nur neue/bearbeitete Kommentare seit meinem lokalen erfolgreichen Abrufmarker;
  Historie/PRs gezielt öffnen. Kommentare nur über konkrete ID ändern, niemals `--edit-last`.
  Remote vor Arbeitsblöcken/Commits und spätestens alle 15 Minuten abrufen. Eigene Änderungen
  bleiben erhalten; laufende Gates bleiben auf ihrem eingefrorenen Stand.
- Der aktive Nutzerauftrag bleibt maßgeblich; `goal.md` auf `master` beschreibt den Outshine-/Integrationsauftrag.
- Ich will jeden beliebigen Ort der Erde bei 1280×720/60 einschließlich p99 darstellen können,
  mit bis zu 240 km Sichtweite, soweit Gelände, Erdkrümmung und Wetter Sicht zulassen.
  Realistische Vegetation, Gebäude, Infrastruktur, Fahrzeuge und NPCs gehören gemeinsam
  in dieses Ziel und dasselbe Gerätebudget auf A18 Pro/Metal und i5-1135G7/Iris Xe/Vulkan.
  Die Ausbaufolge ändert den Zielumfang nicht. Weltweite Inhaltsmenge darf weder vollständiges
  Vorladen noch Einzelobjektarbeit in der Ferne erzwingen; Bild-/Simulationswirkung bestimmt Bedarf.
  Einzelne Places oder abgeschaltete Inhalte belegen dieses Gesamtziel nicht (#5).
  Abdeckung, Sichtweite und 1280×720/60 sind feste Auslegungsbedingungen auf beiden Geräten;
  ich erreiche sie zuerst mit vollständigen Szenen und steigere danach kontinuierlich die
  Darstellungsqualität innerhalb dieser Bedingungen. Leistungsdefizite erfordern bessere
  Verfahren und Repräsentationen, keine Lockerung der Bedingungen.
- Ich baue eine plausible, funktionale Interpretation der Erde, keinen digitalen Zwilling.
  OSM, DEM und Wetter liefern Verteilung, Großformen, Parameter und Zustände, keine vollständigen
  Baupläne. Gelieferte Angaben erhalten ihre Herkunft; fehlende Angaben werden plausibel
  und möglichst authentisch ergänzt. Prozedurale Rezepte vereinfachen sie innerhalb des Budgets.
  Ich suche eine schnelle, möglichst realistische und funktionale Lösung, keine eindeutige
  Rekonstruktion. Anpassungen an Lage, Form und Profilen sowie einfachere Rezepte erhalten
  Quellbezug, Funktion und physikalische Plausibilität.
- Darstellung und prozedurale Ergänzungen folgen OSM-/DEM- und Wetterdaten mit größtmöglichem
  Realismus innerhalb der Gerätebudgets. Physikalisch basiertes Rendering bestimmt Licht,
  Materialien und Kameraantwort; keine gestalterischen Stil-, Farbpaletten- oder Spiele-Look-Vorgaben.
  Licht und Kameraantwort besitzt [#4](https://github.com/Weichwolf/outshine/issues/4), Materialkomposition [#6](https://github.com/Weichwolf/outshine/issues/6).
- Ich liefere vom konkreten Bilddefizit bis zum integrierten Rendering. Interne Arbeit braucht
  einen Feature-Blocker oder gemessenen Engpass. Abstürze, Datenverlust und falsche Geometrie
  behebe ich sofort; vorhandene Straßenqualität und Terrain-Deformation bleiben erhalten.
- Aktueller Bildfokus: Transport, Gebäude und Bodenmaterialien. Die zwölf frei wählbaren
  Places mischen Natur-, Misch- und Infrastrukturansichten; Vegetation ist je Szenario
  passend aktiviert. Die Python-Forschung läuft unabhängig weiter.
- Ich vermeide Arbeit vor ihrer Optimierung. KISS, DRY, klare Zuständigkeiten und Local Reasoning
  gelten. Schlechte Verfahren, unklare Namen und falsche Modulgrenzen korrigiere ich samt Aufrufern.
  Etablierte Verfahren und lokale Messungen entscheiden, nicht investierte Arbeit oder Zeilenzahl.
- Entfällt der letzte Produktkonsument, entferne ich den überholten Pfad samt exklusiven Tests,
  Buildregeln, Abhängigkeiten und Dokumentation. Tests allein rechtfertigen keinen Altpfad.
  Aktive Verbraucher, benötigte Datenformate und laufende Fachlieferungen vorher prüfen.
- Echtzeit und glaubwürdige Bild-/Spielwirkung bestimmen die Genauigkeit. Ich nutze gemessene
  räumliche/zeitliche Approximationen. Transport braucht konstruktive CAD-Qualität: saubere
  Geometrie, Anschlüsse und funktionale Profile; keine exakte Vermessung der Erde.
  Speichersicherheit, korrekte erzeugte Inhalte und verlässliche Interaktionen bleiben verbindlich. Prüfungen folgen diesen
  Anforderungen; geänderte Anforderungen korrigiere ich ausdrücklich samt betroffenen Tests.
  Ferndetails dürfen plausibel geschätzt werden; Bildwirkung, Herkunft und Übergang zum Nahdetail zählen.
  Transportsolver liefern innerhalb ihres Budgets ein gültiges Teilnetz. Ungelöste Teile
  blockieren weder Weltaufbau noch Frame; ich begrenze Auslassungen lokal und protokolliere
  jede nicht verwendete Quelle mit Grund. Quelldaten bleiben erhalten; der Verwendungsgrad wächst.
- Ich suche selbst Alternativen, sobald ein Ansatz erkennbar ungeeignet ist. Ich prüfe sie gegen
  Bild, Kosten und Verträge und entscheide, ohne auf Lösungsvorschläge des Nutzers zu warten.
- Kostenbestimmende Verfahren vergleiche ich zuerst in kleinen Python-Experimenten mit echten
  Eingaben und gleichen Qualitätsanforderungen; den Gewinner integriere und messe ich nativ.
  Kostenbelege erfassen auch bearbeitete Elemente/Rechenschritte, Allokationen, kopierte Bytes und
  Peak-Speicher, nicht nur Laufzeit/FPS. Hardwareübergreifende Arbeitsersparnis und tatsächliche
  Gerätezeiten getrennt belegen; Speicherzugriffe und Lokalität gehören zum Kostenmodell.
  Beibehaltene Pfade brauchen Korrektheits-/Kostenbelege und Alternativenvergleich; globale
  Optimalität behaupte ich nur mit einer tatsächlich hergeleiteten Grenze.
- Vor Arbeitsblöcken, nach Integration und bei Regressionen durchdenke ich den vollständigen
  Datenfluss bis Bild, Ton, Aktion und nächstem Weltzustand: Laden, Stillstand, Drehung, Bewegung,
  Änderungen, Persistenz und Fehler. Ich prüfe Abhängigkeiten, Besitz, Threads, Kopien und Kosten;
  unsichtbare Schatten-/Simulationswirkung zählt. Gegenbeispiele suchen, Hypothesen messen.
- Ich baue keine Verwaltungs-, Cache- oder Beweissysteme ohne konkreten Bedarf. Nach zwei
  Reparatur-Iterationen ohne Bildgewinn überprüfe ich den Ansatz. Ein umfassender Refactor darf
  keine ausführbare sichtbare Verbesserung verdrängen. Commits und grüne Tests sind kein Bildgewinn.

## Quellen und Architekturgrenzen
- Bevorzugt: OpenFreeMap für OSM-basierte Vektoren, Mapterhorn für Höhen, Open-Meteo für Wetter.
  Ein Hauptanbieter je Datenart; Geschwindigkeit, Verfügbarkeit und nutzbarer Inhalt entscheiden.
  Original-OSM und vollständige Tags sind keine Pflicht. Formate/Nutzungsfragen besitzt [#10](https://github.com/Weichwolf/outshine/issues/10).
- Zeit/Kamera und ein fester lizenzierter/versionierter Sternenkatalog ergänzen die Welteingaben.
  Belegte, fehlende und prozedural ergänzte Angaben bleiben unterscheidbar. Webcam-Fotos dienen
  nur dem Vergleich; keine Fototexturen, Satellitenbilder, Photogrammetrie oder Place-Sondergeometrie.
- Der Engine fordert fertige Game-Assets an: Cachetreffer laden, Misses erzeugen asynchron
  über Generatoren und speichern vollständig ergänzte, versionierte Assets. Das gilt für alle
  Weltklassen. Der Cache enthält vollständig angereicherte Asset-Rohlinge. Treffer wiederholen
  weder Quellaufbereitung noch Rohling-Aufbau; Nahdetails dürfen und müssen daraus zur Laufzeit
  ergänzt werden. Räumliche/LOD-Auswahl und aktuelles Shading folgen danach. Quelle, Asset, Spielstand und
  Testreferenzen bleiben getrennt; keine automatische Quellaktualisierung. [#10](https://github.com/Weichwolf/outshine/issues/10) besitzt den Vertrag.
- Quell- und Assetcaches speichern Größen-/Nutzungsmetadaten und bleiben getrennt budgetierbar
  bereinigbar. Einmalig besuchte Weltregionen dürfen die SSD nicht unbegrenzt füllen;
  häufig genutzte Inhalte bleiben bevorzugt erhalten. Metadatenpflege wird gebündelt und
  darf den Weltaufbau nicht bei einzelnen Schreibfehlern blockieren. Altbestandszeiten nicht erfinden.
- Adapter/Provider beschaffen und normalisieren Eingaben; Generatoren erzeugen native Inhalte.
  Provider sind optional. Erweiterungen besitzen ihre Quellsemantik; OSM gehört zu generators/osm.
  Engine koordiniert; world bleibt eine generische 3D-Welt, Render/Audio konsumieren native Produkte.
- Outshine ist eine Anwendung mit modularer Engine, kein öffentliches SDK. Das Programm
  `outshine` lädt Szenarien aus Datei oder JSON; interne Bibliotheken bleiben nach Zuständigkeit
  gegliedert. Keine externe API-/ABI-, Installations- oder Dokumentationspflicht. Änderungen
  interner Schnittstellen migrieren alle Aufrufer. Szenarien deklarieren Inhalte/Kamera;
  keine einzelnen CLI-Inhaltsschalter.
  Der Client nimmt ein Szenario oder generische Szenario-Overrides entgegen.
  Konkrete Verträge besitzt [#9](https://github.com/Weichwolf/outshine/issues/9); sie werden mit einem echten Feature integriert.
- Produktcode und mitgelieferte Produktdaten kennen keine Places, Place-Koordinaten oder
  ortsspezifischen Optimierungen. Feste Place-Kamera-/Messdaten stehen ausschließlich im
  aufrufenden Testskript. Der generische `bench`-Befehl erhält Szenario, Startpose, Bewegungs-/
  Rotationsvektor und Framezahl als Eingaben; Namen und Bildaufbewahrung besitzt der Testtreiber.
- Rundum-Abdeckung bleibt renderbereit. Drehung ändert Sichtbarkeit, Bewegung ergänzt Bedarf.
  Unveränderte Inhalte werden nicht neu erzeugt. Detailbedarf vor teurer Arbeit bestimmen;
  Änderungen gezielt fortpflanzen. Logische Welt und Kollision bleiben unabhängig vom Render-LOD.
- Metallic-Roughness ist der Hauptshader; zusätzliche Shader ergänzen Effekte, die er nicht abdeckt.
  Texturen werden prozedural erzeugt und dürfen als fertige Assets gecacht werden.
- Besitz, Lebensdauer, Abbruch, Rückstau und Fehler bleiben explizit. Kein blockierendes IO oder
  unbegrenztes Warten im Frame. GPU-Ressourcen erst nach letzter Nutzung freigeben.
- Ressourcenbedarf folgt dem Hardwareprofil: Auf dem A18 Pro begrenzen zwei Performance- und vier
  Effizienzkerne Datenaufbereitung/Simulation; Cachetreffer vermeiden diese Arbeit weitgehend.
  Hintergrundarbeit bleibt begrenzt und priorisiert. Stärkere GPUs erhalten sichtbar höhere Qualität;
  CPU-, GPU- und Speicherbudgets bleiben unabhängig, zusätzliche GPU-Leistung erhöht nicht pauschal CPU-Arbeit.

## Qualität und Abnahme
- Ich priorisiere Licht, Schatten, Materialien und Bildstabilität vor Pixelzahl.
  Reale Referenzen und physikalische Plausibilität bestimmen die Bildabnahme. Auflösung/Zielrate bleiben unabhängige Profile:
  480p, 1280×720, 1920×1080 und höher; 25/30/60 fps. Maße immer explizit Breite×Höhe.
  Hohe Bildqualität bei 1280×720/60 ist das Ziel auf A18 Pro/Metal und Intel i5-1135G7/Iris Xe/Vulkan.
  Framebudget: 1000/60 = 16,67 ms einschließlich p99. Andere Profile bleiben ausdrücklich
  deklarierte Vergleiche; Speicher und CPU/GPU pro Gerät getrennt belegen.
  Das Ziel ersetzt keinen gemessenen Gerätenachweis.
- Die Bildveröffentlichung in `doc/` und README pausiert auf Nutzerauftrag. Ich pushe dort
  vorerst keine neuen oder aktualisierten Bilder. Fachagenten veröffentlichen ebenfalls keine
  Bilder oder Ersatzgalerien. Forschungs-/Diagnosebilder bleiben lokal im System-Tempverzeichnis;
  Verfahren, Tests und Renderer werden weiterhin geliefert. Tatsächliche Bildprüfung bleibt Pflicht.
- Ich pflege zwölf wechselnde Places als Beurteilungswerkzeug, keine vorgeschriebene Ortsliste.
  Auswahl, Kameras und Inhalte folgen aktuellen Bilddefiziten und Testbedarf; mindestens ein
  aktueller Place gehört ins Gate. Dichte Großstadt-/Parkrandansichten bleiben eigene Lastfälle.
  Zwölf Bilder belegen keine weltweite Funktion. Rotierende geografische Gegenbeispiele,
  unterschiedliche Klima-/Gelände-/Siedlungsformen und gemeinsame Verfahrensprüfungen ergänzen sie.
  Ein Teil der Places stammt von [foto-webcam.eu](https://www.foto-webcam.eu/).
  Live- und Archivbilder liefern Inspiration und Vergleich für Wetter, Tages- und Jahreszeiten;
  passende Aufnahme samt Datum/Zustand wählen und Outshines Formen, Materialien und Licht dagegen prüfen.
  Fotos bleiben ausschließlich visuelle Referenzen gemäß den Quellenregeln.
  Ich rendere über das Szenario-Programm und öffne die Hash-PNGs in `build/shots/places/`.
  Vorher/Nachher und passende datierte Webcam-Referenzen vergleichen. In `shots/places/` bleibt
  je Place nur der letzte erfolgreich gespeicherte Shot; Referenzbilder und Pins bleiben erhalten.
  Alle Places bleiben lokal aktuell; die Veröffentlichungspause ändert diese Prüfpflicht nicht.
  Fehlende/unvollständige Bilder bleiben rot. Unbelegte Bildänderungen gelten als Verschlechterung.
- Vollständiger Asset-Cache → frischer Offline-Prozess → vollständige Welt so schnell wie möglich.
  Vorbereitete Places sollen warm unter zehn Sekunden laden; eine Sekunde ist ein Optimierungs-
  anreiz, kein Gate. Ich begründe Ladeziele anhand Datenmenge und Arbeit. Internet-Erwerb,
  Asset-Aufbau und Cachetreffer getrennt messen. Danach genau Ziel-fps Frames und 360° in einer Sekunde;
  nur letzter Frame als PNG in Ausgangsrichtung. p50/p95/p99 ohne Zusatzframes messen.
- Framebudget auch für p99: 1000/Ziel-fps ms. Alle Weltklassen teilen Zeit/Speicher nach Bildgewinn
  und Kosten, ohne feste Quoten. Stadt und Wald dürfen andere Lastverteilungen haben, müssen
  aber gleichermaßen flüssig und visuell kohärent sein. Profil, Inhalte und Sichtweite im Vergleich nicht reduzieren.
  Jeder sichtbare Pixel bekommt denselben Qualitätsanspruch unabhängig von seinem Inhalt.
- Korrektheit, Bild, CPU/GPU, Laden und Speicher getrennt prüfen. Budgets nennen Einheit, Herkunft,
  Lastfall, Profil und Besitzer; Peaks und OS-/Treiberreserve zählen. Geteilten Speicher nicht doppelt
  zählen, asynchrone Zeiten nicht addieren, Fence-Warten nicht als GPU-Zeit ausgeben. A18 Pros
  8 GB sind kein App-Budget; die aktuelle Maschine ersetzt keinen Gerätenachweis.
- Überschreitungen sperren das Gate; schlechte Istwerte erhöhen keine Grenzen. Fehlende Messbarkeit
  bleibt unbewiesen. CPU-Beweise ersetzen kein Runtime-Bild und allein keine kleinere LOD-Schranke.
  Tests nur bei nachweislich falscher oder ausdrücklich geänderter Spezifikation anpassen.
  Normale Tests erzeugen keine Orakel/ändern keine Pins; Cycles benötigt belegtes GPU-Backend.
- Das Pflichtgate prüft die Weltpipeline: native Assets/Cache, DEM/OSM, Umwelt und Offline-Places.
  glTF-Import/-Export und Khronos-Testkorpora gehören nicht zum Produkt. HTML/CSS/JS bleiben.
  Gemeinsame Shaderbindungen, CPU/GPU-Layouts und Ressourcenlebensdauer bleiben Pflicht.

## Code und Arbeit
- Ich nutze Command Line Tools und CLI-Profiler; die Xcode-App ist keine Voraussetzung.
- Fremdcode wird nicht im Repository gebündelt. Ich nutze eigene Implementierungen oder
  Standardbibliotheken aus Homebrew/apt; erforderliche Pakete nenne ich ausdrücklich.
  Ich minimiere direkte und transitive Abhängigkeiten. Bewährte Bibliotheken bleiben,
  wenn genutzter Umfang und Robustheit die Abhängigkeit rechtfertigen; kleine Teilmengen dürfen
  geprüfte gemeinsame native Operationen ersetzen. Referenz, Randfälle, Portabilität
  und gemessene Kosten entscheiden; keine Pakete auf Vorrat. Für Techniken und Algorithmen
  schlage ich immer zuerst in SIGGRAPH-Veröffentlichungen nach, ergänze
  passende Primärquellen und verankere Verfahren und Quellen im zuständigen Issue.
  PDFs prüfe ich immer auch optisch, insbesondere Abbildungen, Diagramme und Gleichungen.
- C++23, native Metal-/Vulkan-Backends, GLSL; SDL3 für Fenster, Eingabe und Audio.
  Ich implementiere Metal, `outshine_engine` Vulkan; gemeinsame GPU-Verträge stimmen wir
  vor der Integration ab. SDL3_GPU wird ersetzt; vorhandene Bildqualität und Funktionen erhalten.
  Backendentscheidungen und Übergang stehen in [#48](https://github.com/Weichwolf/outshine/issues/48).
  RAII, Composition, Zustandsautomaten. Runtime ohne Exceptions;
  behandelbare Fehler als `[[nodiscard]] std::expected`, geprüfte `noexcept`-Verträge.
  Warnings sind Fehler. CPU/GPU-Größe/Alignment/Offsets mit `static_assert` sichern; andere echte
  Typ-/Binärverträge erlaubt. `reaches` und die dokumentierte HTML/CSS/ECMAScript-Teilmenge erhalten.
  Header liegen bei ihren besitzenden Komponenten unter `src/`; kein öffentlicher `include/`-Baum.
  Kein Doxygen oder SDK-Gate. `src/` ohne Kommentare; Tests dürfen Kommentare haben.
- Genau ein oberstes Parent-Issue mit `state:active`, darunter 0:N tatsächlich bearbeitete aktive Kinder.
  Offene Issues bleiben auf GitHub offen; `state:open` und `state:active` unterscheiden Reserve und Arbeit.
  Status-, Prioritäts- und Architekturfelder in Beschreibung und Labels gemeinsam aktualisieren.
  Beim Aktivieren das andere Statuslabel entfernen; abschließen erst nach erfüllter Abnahme mit `gh issue close`.
  `priority:P0` bis `priority:P3`, `architecture:ready`/`architecture:planned` und `owner:*` erhalten.
  Beschreibungen enthalten Ergebnis/Ist, Besitzer, Umsetzung, fehlende Verträge und kurze Abnahme, keine Prüfchronik.
  Bevorzugt 80 Zeilen/6 KiB; ausführliche Diskussion und Belege bleiben in Kommentaren.
- `Parent` und `Depends` nennen GitHub-Issuenummern. `Depends` bezeichnet einen fehlenden Vertrag,
  keine Reihenfolge oder pauschal ein ganzes Subsystem. Priorität, Abhängigkeiten und Vision getrennt prüfen.
  Vor gemeinsamen Änderungen den aktuellen Issue-Stand lesen, fremde Fortschritte erhalten und betroffene
  Verbraucher/Abnahmen mitführen. Beschreibung und Status gelten branchübergreifend.
- Neue Nummern vergibt GitHub.
  Ich referenziere aktuelle Issues als `#N` oder per URL und aktiviere sie vor Implementierung.
  Abgeschlossene Anforderungen in ihren Besitzer übernehmen, dann das Issue schließen; nichts lokal löschen.
  Kleine vollständige Commits mit Issuebezug, ohne KI-Attribution. Ein Commit beendet das Gesamtziel nicht.
- Nach dem Merge löscht der PR-Besitzer den temporären Themenbranch lokal und auf GitHub,
  sobald dessen aktueller Stand vollständig integriert ist. `master`, `outshine_research` und `outshine_engine` bleiben bestehen.
  Offene PRs, belegte Worktrees und nicht integrierte Commits schützen auch ältere Fachbranches
  vor automatischer Bereinigung; Remote-Löschungen sichern
  den geprüften Commitstand per Lease. Keine globale Merge-Löschautomatik für dauerhafte Agentenbranches.
- Für Vegetation bestimmen botanische Referenzen und reale Pflanzen die Bildabnahme;
  die Nutzerbildreferenz besitzt [#1](https://github.com/Weichwolf/outshine/issues/1).
- Pfade/Symbole zuerst mit `rg` suchen, relevante Historie/`make help` lesen. Gezielte Ausschnitte
  ≤160 Zeilen, Ausgabe gewöhnlich ≤2000 Tokens. Funktionen per clang-tidy auf 120 Zeilen begrenzen;
  keine Verdichtung/Suppression. Große Dateien nach Zuständigkeit beim betroffenen Ausbau teilen.
- Ich halte Projekt und Zusammenarbeit kontextsparend: Pfad und Name führen direkt zum Besitzer;
  jede Datei behandelt eine zusammenhängende Aufgabe. Header zeigen Verträge, größere
  Implementierungen stehen in `.cpp`. Keine künstliche Fragmentierung oder Zeilenverdichtung.
  Ich lese gezielte Symbole/Ausschnitte und fasse Logs als Ergebnis, Fehler und Belegpfad zusammen.
  Entscheidungen und Belege haben einen Ort; andere Stellen verlinken statt zu duplizieren.
- Modultests bauen nur die deklarierten Modulabhängigkeiten. Unerwartete Abhängigkeiten
  prüfe ich als Architekturfehler; gemischte Integrationsprüfungen erhalten einen eigenen Ort.
- Code: Format, betroffene Tests, fokussiertes clang-tidy samt abhängigen Units; Shaderverträge und
  betroffene Places prüfen. Volles Lint am Integrationsende und bei API-/Modul-/Build-/Prüfregeländerung,
  nicht je Kleincommit. Fokussiertes clang-tidy nutzt die vollständigen Flags aus der aktuellen
  Kompilationsdatenbank; handgekürzte Aufrufe sind kein gleichwertiger Nachweis.
  Nur Docs: `make lint-docs`; das schließt keine offenen Engine-Gates.
  Während Python-Verfahrensexperimenten laufen deren lokale Prüfungen; native Gates folgen der Integration.
  Lokale Tests bauen nur ihre benötigten Komponenten. Modul-, Speicher-, Nebenläufigkeits- und
  Render-/Places-Prüfungen bleiben gezielt aufrufbar; Vollsuite und Instrumentierung sind getrennte Aufträge.
- Research und Engine liefern Verfahren, neue Funktionen und Optimierungen mit gezielten Korrektheitskontrollen
  und Kostenvergleichen; Engine ergänzt erforderliche Vulkan-/Iris-Gerätemessungen.
  Der PR nennt Commit, Umfang, Plattform, Aufruf und terminales Ergebnis, offene Prüfungen ausdrücklich.
  Ich übernehme Integration, Format, fokussiertes clang-tidy, betroffene Tests, gebündelte Gesamtläufe
  und Places-/Metal-Bildprüfung. Neue breite Läufe koordiniere ich; vorhandene passende Belege
  wiederverwenden. Gezielte Eigenprüfungen der Kollegen bleiben erlaubt und nötig, wenn sie
  Entwicklungs- und Optimierungsentscheidungen absichern. Gemeinsame Verträge vorher mit Verbrauchern abstimmen.
  Gemessene A18-Engpässe gehen mit Szenario, Profil, Commit und Kostenbeleg an `outshine_engine`;
  ich übernehme die Geräte-/Bildgegenprüfung.
  Ein gemeinsamer Voll-Lint gilt dem eingefrorenen Integrationsstand;
  Änderungen danach benötigen die davon betroffenen Prüfungen. Unabhängige Drafts halten das Paket nicht auf.
- Agenten beurteilen parallele Läufe selbst anhand laufender Prozesse und verfügbarer Ressourcen;
  keine globale Ein-Lauf-Sperre je Host. Ein unter Last bestandener Budgettest zählt für den gemessenen
  Lastfall; Vergleichsläufe nennen ihren Lastzustand. CPU-lastige Builds, Lint und Gates laufen
  neben interaktiver Arbeit oder Messungen mit `nice -n 10`; Messprozesse behalten normale Priorität.
  Nice begrenzt weder RAM-/Speicherbandbreite noch GPU-Konkurrenz; Jobzahl nach Ressourcen wählen.
  Im Entwicklungsloop entscheiden Trends vergleichbarer Szenarien; einzelne verrauschte Läufe
  nicht ausermitteln. Wiederholungen/isolierte Gegenmessungen für größere Entscheidungen,
  vermutete Regressionen und Geräteabnahme bündeln. Breite Prüfungen und aufwendige Renderexperimente
  nach verfügbaren Ressourcen verteilen; Intel/Iris-Xe-/Vulkan-Abnahme auf dem
  i5-1135G7, Metal-/A18-Abnahme auf dem Zielgerät. Frühere RTX-Belege sind kein Iris-Xe-Nachweis.
  Lange Gates auf eingefrorenem Commit im detached Worktree mit eigenem Build; Jobzahl nach Ressourcen.
  Kein geteilter Build/parallel spotless.
  Ergebnisse brauchen terminalen Status und Commitzuordnung; geänderter Code neue betroffene Gates.
- Im Repository liegt nur, was gemeinsam für Outshine gebraucht wird: Produktcode, Tests,
  wiederverwendbare Experimentquellen unter `test/experiments/`, feste Testeingaben, Dokumentation,
  Papers und aktuelle Präsentationsbilder. Produkt-Builds bleiben unter `build/`; in `build/shots/`
  liegen ausgewählte Präsentationsbilder und benötigte Referenzen.
  Private Versuchsausgaben, Arbeitsdaten, Hilfsumgebungen und Zwischenprodukte liegen ausschließlich
  im jeweiligen System-Tempverzeichnis (`${TMPDIR:-/tmp}/outshine-<Agent>-<Auftrag>-<Commit>`).
  Kein privater Experimentbaum unter `build/experiments/`. Skripte nutzen explizite Temp-Ausgabepfade;
  beim betroffenen Ausbau alte Standardpfade migrieren. Gemeinsam benötigte feste Referenzinputs
  erhalten einen passenden versionierten Testdatenpfad, keine Abhängigkeit von privaten Tempbäumen.
- Jeder Agent räumt seine Artefakte nach jedem Arbeitsblock und vor einer Lieferung auf:
  ersetzte/fehlgeschlagene Versuchsausgaben, doppelte Exporte, alte Renderings und entbehrliche
  Zwischenprodukte löschen. Aktuellen geprüften Stand, nötigen Vorher-Vergleich, weiter benötigte
  Gegenbeispiele/Eingaben und referenzierte Pins/Belege erhalten. Reproduktion und kompakte
  Ergebnisberichte ersetzen keine benötigten Originaleingaben; alte vollständige Ausgabekopien
  dagegen nicht stapeln. Aktive/fremde Arbeiten, gemeinsame Quellen-/Assetcaches und benötigte
  Referenzen nicht pauschal bereinigen. Diese Pflicht gilt auch im Haupt-Checkout und in Worktrees.
- Logs ins System-Tempverzeichnis; Referenzen nach `build/shots/reference/`, Bildvergleich mit
  `test/scripts/pixels.py`. Worktree-Bilder auch eindeutig ins Haupt-Checkout übernehmen.
  Tests unter `test/outshine/{src,integration/places}`. Telemetrie nach Diagnosebedarf.
  Normale Logs bleiben kompakt; Detailausgabe ist explizit. Diagnose verwendet feste Zähler, hält
  Formatierung/Ausgabe aus dem Hot Path, misst eigene Kosten und kennzeichnet fehlende Messwerte.
- Feste Ablage: Buildprodukte unter `build/`; Netzwerkquellen im Client-Pref-Verzeichnis
  unter `sources/`, fertige Assets/Index separat unter `assets/`; Papers in `doc/references/`;
  Vergleichsbilder/Pins getrennt von beiden Caches.
  Temp-Worktrees heißen `${TMPDIR:-/tmp}/outshine-<Auftrag>-<Commit>`, Logs dort
  `outshine-<Auftrag>-<Commit>.log`; Tests nutzen `outshine-tests.<Checkout>`.
  `outshine-prepared`, `outshine-content` und `outshine-reference-images` enthalten benötigte
  Eingaben/Orakel und werden nicht pauschal gelöscht. Nach Arbeitsblöcken `make cleanup-temp`
  prüfen und mit `CLEANUP_ARGS='--apply --keep-worktree <aktiver Pfad>'` bereinigen.
  Aktive, geänderte, gesperrte oder ungemergte Worktrees bleiben; alte Referenzen ins Haupt-Checkout übernehmen.
- Ich recherchiere normalerweise lokal; beauftragte Anbieter-/API-Recherche und Webcam-Suche
  sowie beauftragte Technik-/Bibliotheksrecherche erlauben Websuche. Werkzeuge aktuell halten,
  nach Updates Orakelherkunft prüfen.
  Ich berichte knapp auf Deutsch: Ergebnis, Commit, Belege und offene Qualitätslücke.
