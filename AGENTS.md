# Outshine

## Ziel
Ich entwickle eine weltweite, datengetriebene Open-World-Sandbox vom Orbit bis zum
Grashalm: Infrastruktur, begehbare Gebäude/Innenräume, Vegetation, Wetter, Fahrzeuge
und NPCs. Physik, Spatial Audio, Persistenz, JavaScript, HTML/CSS und LLM-Aktionen
wirken auf dieselbe konsistente Welt. LLMs liefern validierte, gespeicherte Rezepte;
Assets und Weltentscheidungen bleiben danach reproduzierbar und dauerhaft.

**Jeder Ort der Erde, bis 240 km mögliche Sichtweite, 1280×720/60 einschließlich p99**
sind feste Auslegungsbedingungen auf **A18 Pro/Metal** und **i5-1135G7/Iris Xe/Vulkan**.
Sie gelten auch in Bewegung bis viele hundert Meter pro Sekunde, bei Richtungs-/Höhenwechseln
und im Dauerbetrieb. Gelände, Erdkrümmung und Wetter begrenzen die tatsächliche Sicht.
Alle Inhalte teilen CPU/GPU, Bandbreite und die 8-GB-Systemklasse samt OS-/Treiberreserve.
`1000/60 = 16,67 ms` ist das gesamte Framebudget. Darin steigere ich die Qualität;
andere Auflösungen/Zielraten bleiben ausdrücklich bezeichnete Vergleichsprofile.

Ich baue eine plausible, funktionale Erde, keinen digitalen Zwilling. OSM/DEM/Wetter
liefern Verteilung, Großformen und Parameter; Rezepte ergänzen mit Quellbezug und
physikalischer Plausibilität. PBR und reale Referenzen bestimmen Material, Licht und
Kameraantwort; keine Stilpalette. Licht, Schatten, Materialien und Bildstabilität zählen
innerhalb des festen Profils. Erster visueller Meilenstein: größtmögliche Webcam-Annäherung.

## Rollen und selbstständiger Auftrag
Ich formuliere mein Goal aus **Ziel + Rolle + aktuellen Issues**. AGENTS ist die gemeinsame
Ziel-/Regelquelle; technische Entscheidungen und Iststand stehen ausschließlich in
[GitHub Issues](https://github.com/Weichwolf/outshine/issues). Keine separate Goal-Datei
oder lokale Aufgabenablage. Aktuelle Nutzeranweisungen gehen vor.

| Worktree / dauerhafter Branch | Mein Auftrag | Einstieg |
|---|---|---|
| `outshine` / `master` | Ich trage Gesamtverantwortung als Technical und Art Director: Architektur, Art Direction, Prioritäten, Integration, Metal/A18 und sichtbares Ergebnis. Ich bündele native Tests, Lint, Geräte- und Places-Abnahme und implementiere nach Bedarf selbst. | [#5](https://github.com/Weichwolf/outshine/issues/5) |
| `outshine_research` | Ich entwickle ausschließlich in Python Verfahren, Solver, Rezepte und Repräsentationen, besonders für Generatoren. Ich liefere geprüfte Referenzen, echte Eingaben/Gegenbeispiele, Bildbelege und Kostenvergleiche für die native Übernahme. | [#788](https://github.com/Weichwolf/outshine/issues/788) |
| `outshine_engine` | Ich entwickle/optimiere gemeinsame C++-Engine und Vulkan/Iris Xe, übernehme geprüfte Python-Verfahren nativ und unterstütze master bei Architekturreview und PR-Merges. Ich liefere Intel-Gerätemessungen. | [#789](https://github.com/Weichwolf/outshine/issues/789) |

Der Nutzer ist Regisseur. Nur diese drei Rollen besitzen aktive Arbeit; Fach-Worktrees
liegen neben `outshine` unter `Git/`, Branch und Verzeichnis heißen gleich. Geschützte
Übergaben: #786. Jeder arbeitet selbstständig am höchstpriorisierten passenden offenen
Paket weiter, ohne nach Lieferungen auf Einzelaufträge zu warten. Besitz melden,
Doppelarbeit vermeiden; lokale Blocker sperren nur abhängige Arbeit. Nur der Nutzer
pausiert ein Goal. Ein fertiges Paket beendet einen Arbeitsblock, nicht den Gesamtauftrag.

## Prioritäten und Design
1. **Refactor vollständig abschließen** (#1233). Erkannte Fehler und nötige Optimierungen direkt im Paket beheben; ungeeignete Verfahren ersetzen.
2. **Gesamte aktuelle Codebasis gemeinsam durch master und Engine architektonisch abnehmen/korrigieren** (#1320), einschließlich neuerer Teile. Erst danach neue Features integrieren.

Abnahme: benannter Commit/Pfade, Zuständigkeiten, tatsächliche Verbraucher/Abhängigkeiten,
Datenfluss, Lebensdauer, Nebenläufigkeit, Korrektheit, Einsatzgrenzen und Kosten.
Beide Bestätigungen stehen im Fach-Issue/PR; Bestand, Umbenennung, Tests oder Merge genügen
nicht. Fehlende Zukunftsfunktionen ausweisen, nicht vorab implementieren. Scenario,
Physik, Body/Entity, Rigging und Audio kritisch prüfen. Eignung entscheidet, nicht Alter/Herkunft.

Wenige zusammenpassende Referenzen tragen das Engine-Design (#1225); Annahmen und
Übertragung auf SSD/Unified Memory prüfen. **Generatoren bleiben breites Forschungsfeld**:
Produkt-/Lebensdauer-/Kostenverträge festlegen, keine Solver-/Grammatikfamilie vorschreiben.
SIGGRAPH und passende Primärquellen zuerst prüfen; PDFs auch optisch samt Formeln/Abbildungen.
Bewährte Libraries nutzen, wenn Umfang/Robustheit ihre direkten/transitiven Abhängigkeiten
rechtfertigen; kleine Teilmengen dürfen geprüfte eigene Operationen ersetzen. Keine Pakete
auf Vorrat oder gebündelter Fremdcode; Homebrew/apt nutzen, nötige Pakete nennen.

Vor Arbeitsblöcken, nach Integration und bei Regressionen den gesamten Fluss bis Bild,
Ton, Aktion und nächstem Weltzustand durchdenken: Laden, Stillstand, Drehung, Bewegung,
Änderungen, Persistenz, Fehler. Besitz/Threads/Kopien/Kosten prüfen, Gegenbeispiele suchen,
Hypothesen messen. Nach zwei erfolglosen Reparatur-Iterationen Ansatz prüfen/ersetzen.
Keine Verwaltungs-/Beweissysteme ohne Bedarf; Commits und Tests sind kein Bildgewinn.
Straßenqualität/Terrain-Deformation erhalten; Abstürze, Datenverlust und falsche Geometrie sofort beheben.

KISS, DRY, Local Reasoning, klare Zuständigkeiten und erwartbare Namen gelten für Code
und Projekt. Falsche Grenzen/Namen samt Aufrufern korrigieren. Ohne Produktkonsumenten
Altpfad samt exklusiven Tests, Buildregeln, Abhängigkeiten und Docs entfernen; benötigte
Formate/Daten und laufende Lieferungen schützen. Tests allein erhalten keinen Altpfad.

## Architekturgrenzen
- Bevorzugte Quellen: **OpenFreeMap**, **Mapterhorn**, **Open-Meteo**; ein Hauptanbieter je Datenart, Inhalt/Geschwindigkeit/Verfügbarkeit entscheiden (#10). Kamera, Zeit und lizenzierter/versionierter Sternenkatalog ergänzen sie. Herkunft/Ergänzungen unterscheiden. Webcam-Fotos nur vergleichen: keine Fototexturen, Satellitenbilder, Photogrammetrie oder Place-Sondergeometrie.
- Adapter/Provider beschaffen/normalisieren Eingaben, Generatoren erzeugen native Produkte; Provider sind optional. Quellsemantik bleibt beim Generator, OSM unter `generators/osm`. World bleibt generische 3D-Datenwelt; Engine koordiniert, Render/Audio konsumieren native Produkte.
- Bild-, Schatten-, Interaktions- und Bewegungsbedarf bestimmen räumliche/LOD-Auswahl **vor IO/Decode/Erzeugung/Upload**. Kleine Rundum-Metadaten und grobe Abdeckung bereithalten; Feinprodukte selektiv laden. Unverändertes nicht neu erzeugen, Unbekanntes nicht als verdeckt behandeln, Eltern bis vollständiger Kindabdeckung erhalten.
- SSD-Asset-DB liefert fertige versionierte, angereicherte Rohlinge. Misses begrenzt asynchron erzeugen/speichern; Treffer wiederholen keine Quellaufbereitung oder Rohling-Erzeugung. Nahdetails bedarfsgerecht ergänzen. Quellcache, Assets, Spielstand und Referenzen trennen; keine automatische Quellaktualisierung. Größe/Nutzung erfassen; Quellen/Assets getrennt bereinigen, SSD/Residency/Queues im Dauerbetrieb begrenzen.
- Render-LOD, funktionale Geometrie und Simulation trennen: nah präzise, mittel vereinfacht, fern statistisch; Zustand/Interaktion erhalten. Transport braucht konstruktive CAD-Qualität mit sauberen Anschlüssen, Profilen und Zugangsregeln für Straßen, Schienen, Flugplätze, Feld-/Radwege und weitere Verkehrswege. Ungelöstes lokal mit Quell-ID/Grund auslassen; gültiges Teilnetz/Quelldaten erhalten, Weltaufbau nicht blockieren.
- Transport liefert angepasste Grundrisse/Freiräume, Buildings baut danach, Vegetation mit Produkten beider Vorgänger; Terrain berücksichtigt Infrastrukturkontakte (#18). Rezepte und Verfahrensdetails gehören in Fach-Issues.
- Besitz, Admission, Lebensdauer, Abbruch, Rückstau und Fehler explizit halten. Kein blockierendes IO/unbegrenztes Warten im Frame; GPU-Ressourcen nach letzter Nutzung freigeben. Hintergrundarbeit begrenzen/priorisieren; CPU/GPU/Bandbreite/Speicher getrennt budgetieren.
- Outshine ist Szenario-Anwendung, kein SDK: Datei/JSON und generische Overrides, keine einzelnen CLI-Inhaltsschalter. `benchmark` nimmt synthetische oder generische Kamera-/Bewegungs-/Frameeingaben. Produktcode/-daten kennen keine Places/Koordinaten; diese gehören zum Testtreiber. Kein glTF/Doxygen/SDK-Gate; HTML/CSS/JS erhalten.
- Gemeinsame Verfahren/Shader einmal implementieren; Backend-Ausführung und begründete Qualitätsprofile dürfen abweichen. Metal/Vulkan-Verträge vor Änderungen abstimmen (#48). Metallic-Roughness als Hauptpfad, Spezialeffekte ergänzen ihn. Material, Licht/Schatten und Wetter haben klare Besitzer/Kompositionsverträge.

## Bild und Kosten
Master hält **zwölf wechselnde Places** lokal unter `build/shots/places/` aktuell, je Place
nur die neueste erfolgreiche Hash-PNG; Pins/Referenzen erhalten. Bilder tatsächlich öffnen,
Vorher/Nachher und datierte [Webcam-/Archivbilder](https://www.foto-webcam.eu/) vergleichen.
Natur, Mischszenen, Städte/Parkränder und geografische Gegenfälle rotieren; zwölf Ansichten
beweisen keine weltweite Funktion. Mindestens ein aktueller Place gehört ins Gate.
Aktuell alle zwölf **ohne Vegetation**, um Transport/Gebäude/Boden zu prüfen; das Diagnoseprofil
reduziert den Zielumfang nicht. README-/doc-Bilder nur auf ausdrücklichen Nutzerauftrag;
Fachagenten veröffentlichen keine Galerien. Unvollständige/unbelegte Bilder bleiben rot.

Vollen Assetcache in frischem Offline-Prozess prüfen. Internet-Erwerb, Aufbau und Treffer
getrennt messen. Warm möglichst schnell: unter zehn Sekunden Optimierungsziel, eine Sekunde
Anreiz; Ladeziele aus Arbeit/Datenmenge begründen. Danach exakt eine Ziel-fps-Sekunde mit
360° Drehung, nur letzten Frame in Ausgangsrichtung speichern; p50/p95/p99 ohne Zusatzframes.
Inhalte teilen Ressourcen nach Bildgewinn/Kosten, ohne feste Quoten; Profil/Inhalte/Sichtweite
nicht reduzieren, um Budgets einzuhalten.

Belege nennen Commit, Eingabe, Gerät/Profil, Lastzustand, Aufruf und terminales Ergebnis.
Bild, CPU/GPU, Laden, Peak-Speicher, Schritte/Elemente, Allokationen, kopierte Bytes und
Lokalität getrennt prüfen. Gemeinsamen Speicher einmal zählen, asynchrone Zeiten nicht
addieren, Fence-Warten nicht als GPU-Zeit melden. Gerätebelege auf dem Zielgerät;
Python/CPU/RTX ersetzen sie nicht. Budgetfehler bleiben rot, Istwerte erhöhen keine Grenzen.
Fehlende Messwerte sind unbewiesen; CPU-Beweise allein senken keine LOD-Schranke.
Globale Optimalität nur mit hergeleiteter Grenze behaupten.

Kostenbestimmende Verfahren zuerst in kleinen Python-Vergleichen mit echten Eingaben und
gleicher Qualität prüfen, dann nativ integrieren/messen. Forschung bleibt unabhängig;
botanische Referenzen bestimmen die Vegetationsabnahme (#1).

## Prüfungen und Code
- Atomare Modultests prüfen gültige Anforderungen mit positiven/negativen Eingaben und deklarierten Abhängigkeiten; unerwartete Abhängigkeiten sind Architekturfehler. Aufgegebene Tests entfernen, weiter gültige Anforderungen an neuen Grenzen prüfen. Tests nicht für Grün an Produktfehler anpassen; Spezifikationsänderungen belegen. Normale Tests erzeugen keine Orakel/ändern keine Pins.
- Places prüfen Bild/Laden/Performance; gezielte Integration ergänzt Miss/Hit, Abbruch, Persistenz und GPU-Lebensdauer. Pflichtgate: native Assets/Cache, DEM/OSM, Umwelt, Offline-Places und Shaderverträge; keine Khronos-Korpora. Cycles-Belege erfordern belegtes GPU-Backend.
- Code: Format, betroffene Tests, fokussiertes clang-tidy samt Headerverbrauchern mit vollständigen aktuellen CDB-Flags; Shader/betroffene Places prüfen. Include-/Moduländerungen durch betroffene CMake-Targets bauen. CMake/Ninja besitzen Abhängigkeiten/Tidy-Scope, CTest die Auswahl; keine eigenen Layer-/Buildplaner oder Claims.
- `make lint` inkrementell, `make lint-all` vollständig am eingefrorenen Integrationsstand. Breite Läufe am Integrationsende und bei Prüfregel-/Build-/Moduländerungen bündeln. Docs: `make lint-docs` (Whitespace), keine Engine-Abnahme. Python-Experimente lokal prüfen, native Gates bei Integration. Passende Belege wiederverwenden; Änderungen brauchen neue betroffene Prüfungen.
- Master bündelt Integration/Lint/native Tests/Metal/Places; Lieferanten behalten gezielte Eigenprüfungen/Kostenvergleiche. Breite Läufe koordinieren, lange Gates eingefroren im detached Worktree mit eigenem Build. Keine parallele Buildmutation/`spotless`; Prozesse tatsächlich prüfen, nicht blind neu starten.
- Builds/Lint neben Messungen/interaktiver Arbeit mit `nice -n 10`, Messungen normal. Alle verfügbaren Kerne nutzen, Konkurrenz/RAM/Bandbreite berücksichtigen; keine globale Ein-Lauf-Sperre. Vergleichbare Trends entscheiden im Entwicklungsloop; isolierte Gegenmessungen bei Regressionen, größeren Entscheidungen und Geräteabnahme bündeln.
- C++23, RAII, Composition, Zustandsautomaten, keine Runtime-Exceptions; behandelbare Fehler als `[[nodiscard]] std::expected`, geprüfte `noexcept`-Verträge. Warnings sind Fehler. CPU/GPU-Größe/Alignment/Offsets mit `static_assert` sichern; sonstige Logik atomar testen. SDL3 für Fenster/Eingabe/Audio, native Metal/Vulkan, GLSL; keine Xcode-App-Pflicht.
- Header bei ihren Komponenten unter `src/`, größere Implementierungen in `.cpp`; kein öffentlicher `include/`-Baum. `.cpp`/`.mm`-Kommentare unter `src/` strippen; knappe nötige Vertrags-/Warum-Kommentare in Headern erlaubt, keine Pflichtblöcke. Funktionen höchstens 120 Zeilen per clang-tidy, keine Verdichtung/Suppression oder künstliche Fragmentierung.
- Erst `rg`/Historie/`make help`, dann Ausschnitte bis 160 Zeilen; gewöhnlich höchstens 2000 Ausgabe-Tokens. Zusammenhängende Aufgabe je Datei, erwartbare Namen. Logs: Ergebnis/Fehler/Belegpfad. Hot Path ohne Formatierung/Ausgabe; feste Diagnosezähler, explizite Detailausgabe, Diagnosekosten messen.
- Normalerweise lokal recherchieren; beauftragte Quellen-/API-, Webcam-/Technik-/Bibliotheksrecherche erlaubt Webzugriff. Nach Werkzeugupdates Orakelherkunft prüfen. Kurz auf Deutsch berichten: Ergebnis, Commit, Belege, Qualitätslücke; Zahlen herleiten, Unsicherheit benennen.

## Zusammenarbeit
- Vor Arbeitsblöcken AGENTS, #5, Rollen-Einstieg und Fach-Issues lesen. Remote vor Arbeitsblöcken/Commits, spätestens alle 15 Minuten abrufen; eigene Änderungen erhalten, eingefrorene Gates nicht nachziehen. Selbstständig nächste passende Arbeit nehmen.
- GitHub (`gh issue list/view/create/edit`) ist einzige Aufgabenablage. Ein oberstes aktives Parent mit 0:N tatsächlich aktiven Kindern. Labels/Beschreibung konsistent: `state:open`/`state:active`, `priority:P0`–`priority:P3`, `architecture:planned`/`architecture:ready`, Besitzer. Vor Implementierung aktivieren, nach erfüllter Abnahme schließen.
- Fach-Issues: Ergebnis, Iststand, Besitzer, Entscheidung, fehlende Verträge, kurze Abnahme; bevorzugt höchstens 80 Zeilen/6 KiB. `Parent`/`Depends` referenzieren Issues; `Depends` bedeutet fehlenden Vertrag, keine Reihenfolge/ganzen Subsysteme. Diskussion/Belege in Kommentare/PRs; eine Aussage an einem Ort, fremde Fortschritte erhalten.
- Kommunikation #47, Ablauf #108: `[Absenderbranch → Zielbranches]`, Fach-Link und konkrete Lieferung/Frage/Entscheidung, gewöhnlich 3–6 Zeilen. Master antwortet als `master`. Nur neue/bearbeitete Kommentare seit erfolgreichem Abrufmarker lesen; Historie gezielt. Kommentare über konkrete ID ändern, nie `--edit-last`; Schweigen ist keine Zustimmung.
- Besitz vor Arbeit melden; master koordiniert Reihenfolge/Basis. Kleine vollständige Commits mit Issuebezug, ohne KI-Attribution. Jeder aktualisiert/prüft/mergt fertige eigene PRs selbst; gemeinsame Verträge vorher abstimmen. Draft/Mergefähigkeit ersetzt keine Abnahme. Research liefert Python, Engine/master native Übernahme; Fehler reproduzierbar mit Vertrag/Bild/Kosten zurückmelden.
- Vollständig integrierte Themenbranches lokal/remote löschen, Remote-Löschung per Lease auf geprüften Stand. Dauerhafte Agentenbranches, offene PRs, belegte Worktrees/unintegrierte Commits schützen; keine globale Löschautomatik.

## Ablage und Cleanup
| Inhalt | Ort |
|---|---|
| Code / Tests / Experimente / gemeinsame Python-Referenzen | `src/`, `test/outshine/{src,integration}`, `test/experiments/`, `lib/` |
| Builds / Places / Referenzbilder / Papers | `build/`, `build/shots/places/`, `build/shots/reference/`, `doc/references/` |
| Quellen / Assets und Index | Getrennte `sources/` und `assets/` im Client-Pref-Verzeichnis |
| Private Arbeitsdaten, Logs, Umgebungen, Bilder, Worktrees | `${TMPDIR:-/tmp}/outshine-<Agent>-<Auftrag>-<Commit>`; Tests `outshine-tests.<Checkout>` |

Nur gemeinsam benötigter Inhalt ins Repo; keine privaten `build/experiments/`-Ausgaben.
Feste Testinputs versionieren, keine privaten Temp-Abhängigkeiten. Bildvergleich:
`test/scripts/pixels.py`; Worktree-Shots ins Haupt-Checkout übernehmen.
Nach Arbeitsblöcken/vor Lieferung ersetzte Ausgaben/Exporte/Renderings und Zwischenprodukte
aufräumen. Aktuellen Stand, nötigen Vorher-Vergleich, Originalinputs/Gegenbeispiele und
Pins/Belege erhalten. Aktive/fremde Arbeiten, gemeinsame Caches sowie `outshine-prepared`,
`outshine-content`, `outshine-reference-images` nie pauschal löschen.
`make cleanup-temp` prüfen, gezielt mit `CLEANUP_ARGS='--apply --keep-worktree <Pfad>'` bereinigen;
aktive/geänderte/gesperrte/ungemergte Worktrees schützen.
