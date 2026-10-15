# Outshine

## Auftrag
- Ich lese AGENTS, den aktiven Goal-Text, Parent-Issue [#5](https://github.com/Weichwolf/outshine/issues/5) und betroffene Kinder zuerst.
  Goal enthält das Ergebnis, AGENTS dauerhafte Regeln, GitHub Issues Features und technische Entscheidungen.
  Aktuelle Nutzeranweisungen gehen vor.
- GitHub Issues unter https://github.com/Weichwolf/outshine/issues sind die einzige gemeinsame Aufgabenablage.
  Ich lese und bearbeite sie mit `gh issue list`, `gh issue view`, `gh issue create` und `gh issue edit`.
  Architektur, Implementierungsentscheidungen, Iststand und Abnahme stehen in der Issue-Beschreibung;
  Kommentare halten Diskussion und Belege. Ich lege keine lokalen Board-Dateien oder parallelen Aufgabenlisten an.
- Ich bin Technical und Art Director; der Nutzer ist Regisseur. Fachagenten arbeiten zusammen:
  Der Transport-Agent auf `transport`, der Buildings-Agent auf `buildings` und der Vegetation-Agent auf
  `vegetation` erforschen ihre Fachverfahren in Python und entwickeln gemeinsam die Referenzbibliotheken unter `lib/`.
  Der Outshine-Hauptagent auf `master` entwickelt Outshine selbstständig weiter und integriert eigenständig
  verifizierte Neuerungen aus `lib/` maschinennah in C++ unter `src/`, einschließlich Generatoren und öffentlicher APIs.
  Die weltweite Sandbox steht in [#5](https://github.com/Weichwolf/outshine/issues/5), räumliche Verträge in [#18](https://github.com/Weichwolf/outshine/issues/18).
- Native Fachagenten finden ihren kopierbaren `/goal`-Absatz und Einstieg hier:

  | Agent/Branch | Zuständigkeit | Goal |
  |---|---|---|
  | `outshine_performance` | Renderarbeit, Sichtbarkeit, Batching, Speicher/Lebensdauer; RTX 3060 bei 1920×1080/60 | [#188](https://github.com/Weichwolf/outshine/issues/188) |
  | `outshine_material` | Prozedurale Oberflächen, gemeinsame Materialkomposition und Filter | [#189](https://github.com/Weichwolf/outshine/issues/189) |
  | `outshine_light` | Beleuchtung, Schatten, Reflexion, klarer Himmel/Atmosphäre, Astronomie und Kameraantwort | [#190](https://github.com/Weichwolf/outshine/issues/190) |
  | `outshine_weather` | Wetterdaten/-zustand, Wolkendichte/-darstellung, Wind, Nebel und Niederschlag | [#191](https://github.com/Weichwolf/outshine/issues/191) |

  Alle dürfen native PRs gegen `master` erstellen. Der Hauptagent reviewed und integriert sie,
  verantwortet gemeinsame Bildqualität/Places und hält das A18-Pro-Profil 1280×720/30.
  Native Fachagenten mergen erst nach diesem Review; die Python-Merge-Regel bleibt separat.
  Materialparameter, Beleuchtung und Wetterzustand haben je einen Besitzer. Gemeinsame Pässe,
  Bindings und Radiance-/Transmittanzkomposition vor Änderungen mit den Verbrauchern abstimmen.
- `test/experiments/` enthält Versuche, Gegenbeispiele, faire Vergleiche, Benchmarks und Showcase-Code.
  Bewährte wiederverwendbare Python-Verfahren werden als installierbare Referenzbibliotheken unter `lib/` gepflegt.
  Experimente importieren diese Bibliotheken; Fachmodule teilen Verträge und erforderliche Geometrieverfahren.
  Transport plant zuerst und liefert angepasste Gebäudegrundrisse/Freiräume, Buildings baut danach,
  Vegetation zuletzt mit den räumlichen Produkten und Hinweisen beider Vorgänger. Quell-IDs und Quellen erhalten.
- Die drei Forschungsagenten committen und pushen ihre verifizierten Fortschritte selbstständig,
  erstellen für ihre Experimente und Änderungen an `lib/` Pull Requests und mergen diese nach bestandenen
  betroffenen Prüfungen eigenständig nach `master`. Zugehörige Tests/Referenzdokumentation mitführen.
  Gemeinsame Verträge mit betroffenen Verbrauchern abstimmen; keine fremden Fortschritte überschreiben.
  Der Outshine-Hauptagent übernimmt die native Integration samt Engine-, Bild- und Kostenprüfung eigenständig.
  Die Forschungsagenten warten dafür nicht auf eine native Umsetzung ihrer Python-Verfahren.
  Alle Agenten benutzen dieselben GitHub Issues; Experiment, Bibliotheksstand und native Integration getrennt belegen.
  Gemeinsame Bibliotheksbasis: [#47](https://github.com/Weichwolf/outshine/issues/47); native Integration: [#48](https://github.com/Weichwolf/outshine/issues/48).
- [#47](https://github.com/Weichwolf/outshine/issues/47) ist der gemeinsame Kommunikationseingang;
  der vereinbarte Ablauf steht in [#108](https://github.com/Weichwolf/outshine/issues/108).
  Nachrichten beginnen mit `[Absenderbranch → Zielbranches]`: Fach-Issue/PR-Link und konkrete
  Frage, Entscheidung oder Lieferung, gewöhnlich 3–6 Zeilen. Branches heißen `master`, `transport`,
  `buildings`, `vegetation` und die oben genannten nativen Agenten.
  Ein kurzes Fach-Issue besitzt genau einen abgrenzbaren Vertrag, Fehler oder Entscheidungsbedarf;
  Eltern verlinken Kinder. Umsetzung, API-Änderungen, Python-/Native-Grenze und Prüfbelege stehen im PR
  oder bei direkter Master-Lieferung beim verlinkten Commit im Fach-Issue;
  ausführliche Berichte in verlinkten Artefakten. Jede Aussage hat einen Ort; andere Stellen verlinken sie.
  Antworten bleiben bei der Fachfrage. Zuständige Branches antworten ausdrücklich; Schweigen ist keine Zustimmung.
  Ich lese repositoryweit nur neue oder bearbeitete Kommentare seit meinem lokalen Abrufmarker;
  der Marker wird erst nach erfolgreichem Abruf fortgeschrieben. Historie/PRs öffne ich gezielt bei Bedarf.
  Wir teilen einen GitHub-Account: Kommentare nur über ihre konkrete ID ändern, niemals mit `--edit-last`.
  Ich hole den Remote-Stand vor Arbeitsblöcken und Commits, bei längerer Arbeit spätestens alle 15 Minuten.
  Eigene Änderungen bleiben erhalten; Gates bleiben eingefroren. Kompatible Arbeit braucht keine
  Freigaberunde. Fehlende Teilverträge benennen und unabhängig weiterarbeiten. Verlorene Nachrichten
  korrigiere ich anhand tatsächlicher Antworten, ohne eine gemeinsame Verwaltungsbibliothek aufzubauen.
- Der aktive Nutzerauftrag bleibt maßgeblich; `GOAL.txt` auf `master` beschreibt den Outshine-/Integrationsauftrag.
- Ich baue eine plausible, funktionale Interpretation der Erde, keinen digitalen Zwilling.
  OSM, DEM und Wetter liefern Verteilung, Großformen, Parameter und Zustände, keine vollständigen
  Baupläne. Gelieferte Angaben steuern Vielfalt/Erdähnlichkeit. Prozedurale Rezepte ergänzen
  und vereinfachen sie kohärent innerhalb des Budgets.
  Ich suche eine schnelle, plausible, ästhetische und funktionale Lösung, keine eindeutige
  Rekonstruktion. Ich darf Lage, Form und Profile kohärent anpassen und einfachere Rezepte wählen.
- Solarpunk ist das Default-Setting. Den gemeinsamen Look besitzt [#4](https://github.com/Weichwolf/outshine/issues/4), Materialkomposition [#6](https://github.com/Weichwolf/outshine/issues/6).
- Ich liefere vom konkreten Bilddefizit bis zum integrierten Rendering. Interne Arbeit braucht
  einen Feature-Blocker oder gemessenen Engpass. Abstürze, Datenverlust und falsche Geometrie
  behebe ich sofort; vorhandene Straßenqualität und Terrain-Deformation bleiben erhalten.
- Ich vermeide Arbeit vor ihrer Optimierung. KISS, DRY, klare Zuständigkeiten und Local Reasoning
  gelten. Schlechte Verfahren, unklare Namen und falsche Modulgrenzen korrigiere ich samt Aufrufern.
  Etablierte Verfahren und lokale Messungen entscheiden, nicht investierte Arbeit oder Zeilenzahl.
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
- Adapter/Provider beschaffen und normalisieren Eingaben; Generatoren erzeugen native Inhalte.
  Provider sind optional. Erweiterungen besitzen ihre Quellsemantik; OSM gehört zu generators/osm.
  Engine koordiniert; world bleibt eine generische 3D-Welt, Render/Audio konsumieren native Produkte.
- Builtins und externe Erweiterungen nutzen dieselbe öffentliche API. API-Änderungen migrieren
  alle Aufrufer. Szenarien deklarieren Inhalte/Kamera; keine einzelnen CLI-Inhaltsschalter.
  Der Client nimmt ein Szenario oder generische Szenario-Overrides entgegen.
  Konkrete Verträge besitzt [#9](https://github.com/Weichwolf/outshine/issues/9); sie werden mit einem echten Feature integriert.
- Rundum-Abdeckung bleibt renderbereit. Drehung ändert Sichtbarkeit, Bewegung ergänzt Bedarf.
  Unveränderte Inhalte werden nicht neu erzeugt. Detailbedarf vor teurer Arbeit bestimmen;
  Änderungen gezielt fortpflanzen. Logische Welt und Kollision bleiben unabhängig vom Render-LOD.
- Metallic-Roughness ist der Hauptshader; zusätzliche Shader ergänzen Effekte, die er nicht abdeckt.
  Texturen werden prozedural erzeugt und dürfen als fertige Assets gecacht werden.
- Besitz, Lebensdauer, Abbruch, Rückstau und Fehler bleiben explizit. Kein blockierendes IO oder
  unbegrenztes Warten im Frame. GPU-Ressourcen erst nach letzter Nutzung freigeben.

## Qualität und Abnahme
- Ich priorisiere Licht, Schatten, Materialien und Bildstabilität vor Pixelzahl. RDR2/GTA5 sind
  Qualitätsmaßstäbe, keine unbelegten Versprechen. Auflösung/Zielrate bleiben unabhängige Profile:
  480p, 1280×720, 1920×1080 und höher; 25/30/60 fps. Maße immer explizit Breite×Höhe.
  Hohe Bildqualität bei 1280×720/30 auf A18 Pro ist mein Ziel; 1280×720/60 bleibt Vergleichsmessprofil.
  RTX 3060 verwendet separat 1920×1080/60. Framebudgets: 1000/30 = 33,33 ms beziehungsweise
  1000/60 = 16,67 ms einschließlich p99; Speicher und CPU/GPU pro Gerät getrennt belegen.
  Das Ziel ersetzt keinen gemessenen Gerätenachweis.
- Alle acht Places aus [#5](https://github.com/Weichwolf/outshine/issues/5) sind visuelle Regressionen, mindestens einer gehört ins Gate.
  Tokyo und Central Park sind zusätzliche harte Dichtebenchmarks und gehören ins Render-Gate.
  Ich rendere über die öffentliche Client-API und öffne die Hash-PNGs in `build/shots/places/`.
  Vorher/Nachher und passende datierte Webcam-Referenzen vergleichen. In `shots/places/` bleibt
  je Place nur der letzte erfolgreich gespeicherte Shot; Referenzbilder und Pins bleiben erhalten.
  Die README-Galerie erhält dieselben aktuellen PNGs unter `doc/images/places/`, untereinander
  nur mit Ortsangabe und ohne Vergrößerungslink.
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
  glTF-/Animationskompatibilität und Khronos-Bildreferenzen bleiben separat ausführbar.
  Gemeinsame Shaderbindungen, CPU/GPU-Layouts und Ressourcenlebensdauer bleiben Pflicht.

## Code und Arbeit
- Ich nutze Command Line Tools und CLI-Profiler; die Xcode-App ist keine Voraussetzung.
- Fremdcode wird nicht im Repository gebündelt. Ich nutze eigene Implementierungen oder
  Standardbibliotheken aus Homebrew/apt; erforderliche Pakete nenne ich ausdrücklich.
  Wo immer möglich nutze ich bewährte Bibliotheken statt eigener Standardalgorithmen;
  Vertrag, Portabilität und gemessene Kosten müssen passen. Für Techniken und Algorithmen
  schlage ich immer zuerst in SIGGRAPH-Veröffentlichungen nach, ergänze
  passende Primärquellen und verankere Verfahren und Quellen im zuständigen Issue.
  PDFs prüfe ich immer auch optisch, insbesondere Abbildungen, Diagramme und Gleichungen.
- C++23, SDL3/SDL_GPU, GLSL; RAII, Composition, Zustandsautomaten. Runtime ohne Exceptions;
  behandelbare Fehler als `[[nodiscard]] std::expected`, geprüfte `noexcept`-Verträge.
  Warnings sind Fehler. CPU/GPU-Größe/Alignment/Offsets mit `static_assert` sichern; andere echte
  Typ-/Binärverträge erlaubt. `reaches` und die dokumentierte HTML/CSS/ECMAScript-Teilmenge erhalten.
  `src/` ohne Kommentare, `include/` nur hilfreiches API-Doxygen; Tests dürfen Kommentare haben.
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
- Auf `vegetation` wird jeder geprüfte Fortschritt committed, gepusht und in der lokalen
  Showcase unter `/tmp/outshine-vegetation-showcase` veröffentlicht. Kein automatisches
  Neuladen; neue Stände erscheinen beim Öffnen oder über den manuellen Aktualisieren-Knopf.
  Gestaltung/Bedienung folgen der Building-Showcase; drei typische Landschaften sind feste Bildszenarien.
  Vegetations-Showcase-Renderings mindestens 1920×1080, Standard 2560×1440 für erkennbare Details.
  Für Vegetation gilt SpeedTree als Qualitätsmaßstab; die Nutzerbildreferenz besitzt [#1](https://github.com/Weichwolf/outshine/issues/1).
- Pfade/Symbole zuerst mit `rg` suchen, relevante Historie/`make help` lesen. Gezielte Ausschnitte
  ≤160 Zeilen, Ausgabe gewöhnlich ≤2000 Tokens. Funktionen per clang-tidy auf 120 Zeilen begrenzen;
  keine Verdichtung/Suppression. Große Dateien nach Zuständigkeit beim betroffenen Ausbau teilen.
- Code: Format, betroffene Tests, fokussiertes clang-tidy samt abhängigen Units; Shaderverträge und
  betroffene Places prüfen. Volles Lint am Integrationsende und bei API-/Modul-/Build-/Prüfregeländerung,
  nicht je Kleincommit. Nur Docs: `make lint-docs`; das schließt keine offenen Engine-Gates.
  Während Python-Verfahrensexperimenten laufen deren lokale Prüfungen; native Gates folgen der Integration.
- Ein schwerer Lauf je physischem Host zugleich; Prozesse zuerst prüfen. Lange Gates auf eingefrorenem Commit in
  detached Worktree mit eigenem Build, `LINT_JOBS=2 make lint`. Kein geteilter Build/parallel spotless.
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
  Tests unter `test/outshine/{include,src,integration/places}`. Telemetrie nach Diagnosebedarf.
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
