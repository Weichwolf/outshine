# Installierbare metrische Gleiskörper auf gewählten Ebenen (#376)

## Ergebnis und Vertrag

Die bewährte Anpassung aus #340/#352 ist jetzt additiv öffentlich:
`rail_hardware.build_planar_rail_hardware`, `PlanarRailHardware`, `rail_bed_frame`.
Die Boden-/Deckexperimente importieren denselben Bibliothekskern; eigene
Konstruktionskopie entfernt. Das alte horizontale API bleibt unverändert.
[Signatur, Besitz, Einheiten, Budgets und Grenzen](../../../lib/rail_hardware.md#explicit-affine-bed-planes-376).

Explizites endliches Dreieck wählt eine unendliche affine Bettebene, keinen
automatischen Boden-/Deck-/höchsten Treffer. Ein orthonormaler rechtshändiger
Rahmen erhält metrische Spurweite, Querschnitte und Schwellen. Ein rein vertikales
Anheben würde die Spurweite bei Querneigung verfälschen; Gegenfall bleibt geprüft.
Die Bibliothek liefert eigene unveränderliche Mesh-/Instanz-/Kontakt-/Rahmenarrays
und ganze XY-Projektionen inklusive oberer Schwellenkanten. `sleeper_bottoms` ist
jetzt ein eigenes unveränderlichesN×4×3-Array, kein Tupel einzelner Arrayviews.
Der separate Archivleser benutzt weiterhin sein reines Exportrecord ohne Builder.

RailRecipe/RailBudget bleiben gemeinsam. Stations-/Vertexbudget wird vor
XY-Konversion/-Projektion geprüft, lokale Gründe wie Schwellenbudget, Knick,
Faltung und unzulässiger Ring bleiben erhalten. Ungültige/nicht darstellbare
Ebenen oder Koordinaten sind ValueError; falsche Rezept-/Budgettypen TypeError.
Die öffentlichen Records prüfen Struktur/Besitz, nicht alle örtlichen Konflikte.

Endliche Flächenwahl, vollständiger tatsächlicher Kontakt, Ursprung/Kind/Quell-IDs,
Zugang und Konfliktprüfung bleiben beim Aufrufer. Keine Weichen/C0-Knickanschlüsse,
vollständige Zugfahrbarkeit, neue Plan-/Reservation-/Verbraucherfreigabe, native,
LOD-, Statik- oder Geräteabnahme. Fußquerungen/Gebäudezugänge unter #20 bleiben offen.

## Quellen und feste reale Eingaben

Zuerst Chen et al., SIGGRAPH2008 S.2/Fig2 (Quellgraph/Form trennen), danach
Galin et al., Eurographics2010 S.8/Fig17–18 (Querschnitt/Bauwerkssegmente),
beide vorhandenen Original-PDF-Seiten erneut tatsächlich optisch geprüft.
Keine kopierte externe Implementierung, SDK- oder neue Paketabhängigkeit.
Der bestehende horizontale Kern und die gemessene batchweise starre Transformation
werden verwendet; keine neue optimale Netzplanung behauptet.

`lib/tests/fixtures/planar-rail-vienna.json` enthält alle bereits zugelassenen
77 unteren und56 oberen Wiener Stücke: expliziteXY-Achsen/Dreiecke/Spurweiten,
Original-IDs, Kind und alte Körperpins. Zusammen9.624 Schienenvertices und20.077
Schwellen. Dies ist ausschließlich der gewählte zulässige Teilbestand, kein
neuer Quellnutzungsbeleg. Vollständige Quellattribute, unverwendete Kandidaten und
ursprüngliche Netze bleiben in den alten Originalarchiven erhalten.
ENU inMetern, Ursprung48,21°N/16,37°E; Provider-/DEM-/Ebenenpins übernommen.
`capture_planar_rail_cases.py` erzeugt diese Referenz ausschließlich auf expliziten
Aufruf/frischem Ziel nach Pinprüfung; normale Tests schreiben keine Orakel.

## Source und frisches installiertes Wheel

Produzent/Leser/Tests/Renderer eingefroren auf
`4544b88ddcf6cb4585e3ed41f6739539d98ba374`. Danach nur Docs und fremde unabhängige
Master-Lieferungen integriert; betroffene Implementierung unverändert.
Wheel mit vorhandenem setuptools84.0.0, `pip wheel --no-deps --no-build-isolation`
aus genau diesem eingefrorenen lib gebaut; frisches externes `pip install --no-deps --target`.
Das Transport-venv enthält selbst kein setuptools; Paketbau benutzte daher den
vorhandenen separaten Python-Interpreter, ohne dessen Umgebung zu verändern.

Für Wheel-Prozesse ausschließlich externes Installationsziel plus lib/tests und
Experimentpfad in PYTHONPATH, kein lib/src. Tatsächlich geladener Modulpfad und
SHA stehen im Bericht. Source und Wheel `_planar.py` identisch:
`9641f9365c801699b82fff0bd11a995f7118fc3c860cc99bdb87c9e23977b52f`.
Alle133 neu gespeicherten Schienen-/Index-/Instanz-/Rahmen-/lokalen Arrays sind
exakt gleich den tatsächlichen alten Exports; beide gepackten Hardwaregruppen ebenso.
Kein lediglich erfolgreicher Prozess oder Arraycount als Konformitätsbeleg.

Unabhängiger Leser konsumiert neue gespeicherte Körper ohne Rekonstruktion durch
den Factory-Kern. Ganze ursprüngliche feine Boden-/Deckkontakte, vollständige
physische Footprints, Spurweite, Querschnitt, Maße, geschlossene Volumen und
gegenseitige Konfliktfreiheit je Ebene bestehen. Maximalfehler: Spurweite7,306e-13m,
Schwellenmaß8,882e-16m, Profilbreite8,205e-13m, Kontakt3,187e-14m;
Schienenvolumen7,667e-11m³, Schwellenvolumen8,096e-10m³.
Untere und obere Netze werden getrennt auf XY-Konflikte geprüft, nicht falsche
ebenenübergreifende Projektion als physische Kollision interpretiert.

Beide neu erzeugten Hardwaregruppen ersetzen nach unabhängiger Bereichsprüfung
genau ihre bisherigen Bereiche in der vollständigen #372-Szene. Sämtliche
433.422Vertices/782.840Faces inklusive alter Körper/Beläge/Normalen/Borde/Gehwege
bleiben als ganze Arrays exakt gleich. Kein Nachbauen aus dem Bild oder alter
Render als Ersatz für aktuelle Bibliotheksgeometrie.

49 Kontrollen je Source und installiertem Wheel, terminal0 (0,133/0,137s).
Enthalten sieben neue öffentliche Maß-/Projektions-/Besitz-/Budget-/Ebenengegenfälle,
alte horizontale sowie geneigte/obere Kontakte und Quellen-/Graph-/Zulassungskontrollen.
16 tatsächliche Blenderkontrollen terminal0,7,995s. Ruff/Format/Funktionslängen,
`git diff --check` und `make lint-docs` bestanden. Keine nativen Weltgates.

## Normale Kosten und bestehender Alternativenbeleg

Linux/Python3.13.5, NumPy2.2.4, Shapely2.2.0/GEOS3.14.1, ein BLAS-Thread;
parallele Agentenlast möglich. Konstruktion133 bereits dekodierter Fälle:
Source1,14564s, Wheel1,08902s. Gesamter Erzeugungs-/Export-/Vergleichsprozess
4,65020/4,85254s; Peak231.636/231.500KiB. Keine Beschleunigung aus diesen zwei
Einzelläufen behauptet. Auswahl alter zulässiger Stücke, unabhängiger großer
Kontaktleser und Rendering sind keine Teilmenge dieser Konstruktionszeit.

Eigene aktuelle Ergebnis-/lokale Arraybytes6.693.312B; gemeinsamer Einheitsschweller
separat einmal336B. Dazu kommen Shapely-Geometrie, Pythonobjekte und temporäre
Arbeit; Arraybytes sind kein Prozesspeak und keine vollständige interne Kopierzahl.
Neue Record-Eigentumskopien umfassen Instanz-/Unterkanten-/Rahmenarrays; vollständige
Allokationsereignisse dieses Gesamtbuilders nicht zusätzlich gemessen.

Der bereits verifizierte [Skalar-/Batchvergleich aus #340](tilted_rail_comparison.md#fair-transformation-costs)
bleibt einschlägig für genau denselben Transformationsoperator:4.800 lokale
Vertices/77 Frames, identische körperliche Ausgabe und86.400 logische skalare
Operationen;77 statt4.800 Python-Dispatches. Normale Mediane0,462/21,593ms,
Memray1.20.0 separat:941/43.392 Allokationsereignisse,504.488/4.315.427 kumulativeB,
Peak129.440/136.320B, je115.200 eigene Ausgabebytes. Originaltraces/Inputs unter
`/tmp/outshine-transport-tilted-340-cost` erhalten. Dieser historische Teilbeleg
ersetzt keine frische Gesamtbuilder-Allokationsmessung, keine native/Geräteprüfung
und keinen gleichermaßen großen Zeitfaktor für Auswahl/Kontakte/Packen/Rendern.

## Tatsächliche regionale Bilder

Frische Source-/Wheel1280×720-Bilder und lokale Wheel1920×1080-Prüfung tatsächlich
geöffnet. Gleiche regionale Kamera aus #372 mit1100m Bildspanne, gleiche Objekt-ID,
Geometrie, Quellfarbe, Tageslicht/AgX und16Samples; Blender5.2.2EEVEE/Mesa
llvmpipeLLVM19.1.7. Keine Nahansicht, Überhöhung, neue Material-/Lichtparameter.
Source/Wheel0 von921.600Pixeln verschieden. PNG-Datei-SHAs können wegen Metadaten
abweichen; verglichen wurden tatsächliche RGB-Pixel. Kein neuer Bildgewinn behauptet:
breite Ränder/Brücken bleiben erhalten, Gesamtbild weiter blass/schematisch.
Offline-Render Source720p22,053s/Peak2.417.508KiB, Wheel720p22,427s/2.354.264KiB,
lokal1080p33,601s/2.610.144KiB. Kein GPU-/Framebudgetbeleg.

## Pins und Originale

| Artefakt | SHA256 |
|---|---|
| Feste133 Fälle | `6a17241e93231f34afafe7a610c497ca352bbef0f0e82389b98971cf619154d1` |
| Frisches Wheel | `08d3f7832b003cf4513e8d93a9fe932447765d9eafc340bdb66a106e5caa735a` |
| Sourcebericht | `06abc1dd8c7a054121b29918141a61ca7e9d5c2a819eb17e6162a8b23947b692` |
| Wheelbericht | `12aff5614c13cb42bb5c72fcee086009cb44e20f5df4a953843f18900a311c2b` |
| Ganze ursprüngliche Szene | `3e272f12fe5ca9f9e6bd8026761e7320280c678e6838a0f4dde83cb372d15b31` |

Aktuelle Source-/Wheelprodukte und Bilder: `/tmp/outshine-transport-planar-376-{source,installed}-4544b88dd`;
Wheel/Installation `/tmp/outshine-transport-planar-376-wheel-4544b88dd`.
Je Vergleichsbericht, neue tatsächliche Stücke/Packs, unabhängige verification.json,
Szene/Renderreceipts und tatsächlich geöffnete Bilder. Terminale Phasenlogs
`/tmp/outshine-transport-planar-376-*-4544b88dd.log`; Wheelbuild-v2/install separat.
Originale #340/#352/#372 sowie Netz-/Bauwerksarchive bleiben benötigte Referenzen.
[CLI-Reproduktion](README.md#installierbare-geneigte-gleiskörper-376) verwendet frische
Tempziele und expliziten tatsächlich geladenen Source-/Wheelpfad. Alle Bilder lokal,
keine doc-Publikation durch Fachagenten. Native Übernahme bleibt beim master/#48.
