# Positiver SITE-Kontrollfall unter unverändertem Versuchslimit

#777 beantwortet die konkrete Vegetationsfrage aus #770. Die frische vollständige
102er Welt liefert mit der bisherigen Vorschlagsreihenfolge null Pflanzen nach
128 Versuchen. Das ist ein korrekt erklärtes begrenztes Teilergebnis; ein freier
Wurzelpunkt wurde nicht ungeprüft als freie Krone übernommen.

Der Versuch findet vier vollständig geprüfte Pflanzen nach vier Versuchen in
derselben ursprünglichen Vorschlagsmenge. Er ändert ausschließlich deren
Abarbeitungspriorität. Keine neue globale Pflanzenverteilung oder Artvorgabe.
Die räumliche Übergabe der größeren 105er Welt bleibt bei #770 offen.

## Ursache, Eingaben und Entscheidung

`stair_plant_sites.proposals` erzeugt höchstens 1024 Habitatvorschläge und sortiert
aufsteigend nach XY-Abstand zu aktuellen Volumina. Damit kommen zuerst Punkte
mit Abstand null in die begrenzte Prüfung; viele Hüllen kollidieren tatsächlich.
Es fehlen nicht einfach nur mehr Versuche. Eine geänderte Priorität kann den
vorhandenen Aufwand auf andere bereits erzeugte Vorschläge richten.

Das Beispiel verwendet dieselben vollständigen 8113 physischen/logischen
Transportreservierungen und 65 tatsächlichen Gebäudekörper, zusammen **8178**
Volumina. Alle 394 ursprünglichen Habitat-Ledgereinträge bleiben erhalten.
Das bestehende 8192-Limit wird nicht erhöht, der Bestand nicht beschnitten.
Die verifizierten Produkte stammen aus der frischen Rekonstruktion in
[PR #773](https://github.com/Weichwolf/outshine/pull/773); die ganze Rezeptfolge,
Originaleingaben und Wiederherstellung beschreibt der
[Transportbericht](../transport/stair_prepared_growth_comparison.md).

Der vorhandene junge *Pinus sylvestris*-Prototyp (Seed 718) bleibt unverändert:
289.094 Vertices, 549.948 Dreiecke, Höhe 3,274447 m. Vertex-/Dreiecksarrays
belegen 13.537.632 B; Leafmasken, weitere Prototypdateien und Metadaten kommen
hinzu. Alle vier Instanzen verwenden dieselbe vollständige Form, nicht eine
kleinere Krone oder vereinfachte Kollisionshülle.

## Priorität ist keine Freigabe

`site_order.envelope_radius` berechnet aus den relativen vollständigen
Prototypgrenzen einen nach außen gerundeten XY-Umkreis. In diesem Beispiel
beträgt er **1,490277 m**. Die alternative Reihenfolge bevorzugt zunächst
vorhandene Vorschläge knapp oberhalb dieses Abstands; danach folgen die
übrigen nach abnehmendem Abstand. Source-Part und Site-ID lösen Gleichstände
deterministisch. Alle Tupel/Koordinaten/IDs bleiben vorhanden; Nullabstand wird
nicht herausgefiltert. Es wird weder neue Geometrie noch eine neue Position erfunden.

Die Zahl dient nur zur Priorisierung. Jeder Vorschlag durchläuft weiterhin die
unveränderten aktuellen Terrainkontakte, tatsächlichen Höhenbänder, Root-Support-
Schnitte, die ganze gemessene Hülle und den ursprünglichen SITE-Graphen.
Das Verfahren garantiert weder vier Treffer bei beliebigen Eingaben noch eine
globale maximale Bepflanzung. Bei Rotation/Bewegung müsste der Aufrufer passende
vollständige Grenzen liefern; hier gelten die ursprünglichen statischen Posen.

Die Hauptprogramme sind reine Experimentadapter. Sie rufen den bestehenden
Transportproduzenten und seine Reader unverändert auf und ersetzen nur innerhalb
eines frischen Einzelprozesses die Vorschlagspriorität. Keine Änderung an
Transportdateien oder Library-/Native-Verträgen. Die zusätzliche Ordnungsdiagnose
liegt neben dem ursprünglichen Produktverzeichnis; dessen Artefaktschema bleibt.

Die lokale SIGGRAPH-Veröffentlichung
[Realistic Modeling and Rendering of Plant Ecosystems, 1998](../../../doc/references/vegetation/siggraph/1998-plant-ecosystems.pdf)
trennt Pflanzenverteilung, Formaufbau und Instanzierung (PDF-Seite 4, Abbildung 1,
optisch geöffnet). Das ist die Einordnung, kein Beleg für diese eigene Heuristik.
Hier wird ein positiver Übergabe-Kontrollfall untersucht, kein Ökosystemmodell
und keine neue Dichteverteilung. Keine Websuche oder SpeedTree-Gleichwertigkeit.

## Tatsächlicher ganzer Vergleich

| Gleiche Welt, Form, Seed und Kapazität | Bisher: nächste zuerst | Nahe Umkreisfreiheit zuerst |
|---|---:|---:|
| Vorhandene Vorschläge | 1024 | 1024 |
| Erlaubte Versuche / Pflanzen | 128 / 4 | 128 / 4 |
| Tatsächliche Versuche / angenommene Pflanzen | 128 / 0 | 4 / 4 |
| Konfliktabfragen | 241 | 8 |
| Besuchte XY-Kandidaten / Höhenkandidaten | 4592 / 4438 | 4 / 4 |
| Positive Konfliktschnitte | 4429 | 4 |

Die vier positiven Schnitte gehören zu den Root-Support-Abfragen; die tatsächlich
angenommenen vollständigen Körper bleiben frei. Die angenommenen Wurzeln haben
Abstände von 1,5002/1,5003/1,5146/1,5435 m zu den nächsten tatsächlichen
Transport- **oder** Gebäudevolumina. Das sind keine gemessenen Kronenrandabstände.
Das Ergebnis ändert die vier ausgewählten Positionen; nur die ursprüngliche
Vorschlagsmenge und deren Identitäten bleiben gleich. Nicht als allgemeine
Platzierungsstrategie übernehmen, die Pflanzen an Reservierungsrändern häuft.

Die Konfliktzähler enthalten nicht die gemeinsamen Nearest-Distanzabfragen der
Habitat-/Vorschlagsvorbereitung. Diese Arbeit und die gesamte Proposalmenge
bleiben bestehen; weniger Konfliktaufrufe bedeuten nicht denselben Faktor für
den vollständigen Suchlauf.

Der vollständige Vorschlagssatz hat in beiden Abläufen den kanonischen
Positions-/Schlüsselfingerprint
`04b9bd4cd0b52a98998f995e771175238936a6b02851112023422a55ed428cac`.
Der unabhängige Vergleich erzeugt ihn erneut aus den tatsächlichen Welteingaben,
berechnet die Referenzpriorität unabhängig und prüft sämtliche versuchten IDs,
Koordinaten, die vollständige Präfixmenge, Reihenfolge je Quellteil und Reihenfolge
aller angenommenen Pflanzen. Prototypmetadaten und sämtliche Prototyparrays sind
zwischen den Abläufen exakt gleich. Alle ursprünglichen Source- und Rootdaten
werden durch die vorhandenen vollständigen Reader einschließlich Scan aller
Transport-/Gebäudevolumina geprüft.

Beide kompletten Abläufe samt Originalreader und Prioritätsvergleich bestehen
auch mit frisch gebauter/installierter Referenzbibliothek ohne `lib/src` im
Importpfad. Wheel-SHA256:
`b2ce435a5163106fa70bc41227849075151e73581509ab303a3a36918c1763c2`.
Source/Installed: Prototypdateihashes, vollständige Ledgers, Ordnungsdiagnosen und
alle semantischen SITE-Felder exakt. Nur die beiden ausdrücklich gemessenen
Query-`seconds` werden beim Vergleich der Pflanzenrecords ausgenommen.
Keine anderen Attribute oder Datums-/Posefelder werden entfernt.

23 Source- und 23 Installed-Kontrollen bestehen: fünf neue Prüfungen sowie
18 bestehende Pflanzen-/Payload-/Retentionkontrollen. Sie enthalten Gegenfälle
für freie Wurzel mit kollidierender Krone, gemeinsames Headroom, veränderte
Terrain-/Form-/Frameidentität, Nullbedarf ohne neue Vorschläge und geänderte
Payloads. Neue Präfixprüfungen lehnen fehlende/doppelte/unbekannte Versuche,
verschobene Punkte und vertauschte Quellreihenfolgen ab.

## Kosten und Datenhaltung

Zwei einzelne uninstrumentierte frische Produzentenläufe auf gemeinsam genutztem
Host: Datenladen 3,666/3,616 s, ganzer Prototypaufbau 2,093/2,061 s,
Pflanzsuche einschließlich Ordnungsdiagnose 1,936/1,521 s. Prozess-Peak-RSS
251.621.376/251.940.864 B. Das sind Beobachtungen unterschiedlicher Ergebnisse,
kein belegter Geräte- oder gleichwertiger Laufzeitfaktor.

Separates Memray 1.20.0, ganze tatsächliche Suche nach vorgeladenen Daten/Form/
STRtree, einschließlich ursprünglicher Vorschläge und aller Prüfungen:

| Verfolgte Suche | Nächste zuerst, 0 Pflanzen | Umkreispriorität, 4 Pflanzen |
|---|---:|---:|
| Allokationsanfragen | 8.674.031 | 7.172.366 |
| Kumulativ angeforderte Bytes | 1.168.472.167 | 925.102.605 |
| Verfolgter Heap-Peak | 26.810.981 B | 26.810.981 B |

IO, Prototyp-Geometrieaufbau und zusätzliche Ordnungsdatei liegen außerhalb
dieses Profilbereichs. Instrumentierte Resultate stimmen semantisch exakt mit
den ganzen Produzentenresultaten überein. Peak unverändert: die ursprüngliche
Vorschlags-/Habitatvorbereitung bleibt bestehen. Kumulative Anforderungen sind
weder lebender Speicher noch gemessene Speicherbus-/Kopierbytes; Heap/RSS nicht
addieren. Vollständige interne Kopier-/Zugriffszähler sind nicht gemessen.

Die Priorität selbst kopiert nur Listenreferenzen, keine Prototyp-, Polygon- oder
Positionsarrays. Bei 1024 Einträgen sind das auf 64-Bit-Python 8192 B Referenzpayload
zuzüglich Python-/Schlüsselobjekten. Die diagnostischen JSON-Ansichten erzeugen
zusätzliche kleine Kopien und gehören nicht in einen nativen Framepfad.
Kein globaler Nearest-Tree-/Listenvertrag wird für #770 vorgeschrieben.

Python 3.13.5, NumPy 2.5.4, SciPy 1.18.1, Shapely 2.2.0/GEOS 3.14.1;
vorhandene `lib`-Zusätze für Transport/Places/Solids, optional Memray/bpy.
Keine neue Produktabhängigkeit. Normale Prüf-/Bildläufe nice10, keine native
A18-/RTX-, Lade-, Welt- oder FPS-Abnahme.

## Tatsächliches Bild und offene Qualität

Ein tatsächlich geöffnetes 2560×1440-Porträt lädt die vollständigen gespeicherten
289.094 Vertices/549.948 Dreiecke einer der vier Instanzen. Kamera wird am
verifizierten Weltwurzelpunkt rebasiert; keine Weltposition wurde verschoben.
Alle vier besitzen denselben geprüften Prototyp, deshalb kein redundantes
Vierfachporträt. Es zeigt die vorhandene noch dünne, schematische junge Kiefer.
Konstante PBR-Farben und Anzeigenormalen sind Geometriediagnose; weder neue
Pflanzenform noch botanische/SpeedTree-/Materialqualität werden daraus freigegeben.
Keine ganze Terrainfuß-/Kronenpackungs-/Wind-/Wachstumsfreigabe.
Blender 5.2.2 LTS verwendet llvmpipe/LLVM19.1.7. Bilder bleiben nur `/tmp`.

## Reproduktion

Zuerst aktuelle ganze Transport-/Gebäudeprodukte und deren Reader nach dem
verlinkten Transportbericht rekonstruieren. `.venv` mit Bibliotheksabhängigkeiten;
`PYTHONPATH` enthält `lib/src`, `lib/tests` und die Experimentverzeichnisse
`transport`, `buildings`, `vegetation`, `assets`, `render`.
`TMPDIR=/tmp PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`
setzen; private Outputs und XDG-/Mesa-Caches ausschließlich nach `/tmp` legen.

```sh
.venv/bin/python test/experiments/vegetation/site_order_experiment.py /tmp/current/transport --buildings /tmp/current/buildings --building-receipt /tmp/current/buildings-reader.json --order nearest --output /tmp/outshine-vegetation-sites/nearest
.venv/bin/python test/experiments/transport/stair_plant_verify.py /tmp/outshine-vegetation-sites/nearest --output /tmp/outshine-vegetation-sites/nearest-reader.json
```

Für den zweiten Ablauf `--order boundary` und neue Zielpfade verwenden, dann
beide Produkte mit `site_order_verify.py` samt `--reference-receipt`,
`--trial-receipt`, `--output` vergleichen. `site_order_cost.py` liest ein
verifiziertes Produkt und profiliert ausschließlich dessen Suchablauf.
`render_site_order.py` erhält das positive Produkt, `--receipt`, `--output`;
hier `EGL_PLATFORM=surfaceless DISPLAY= LIBGL_ALWAYS_SOFTWARE=1`.
Alle Ausgabeziele müssen frisch sein. Keine Originaldaten oder fremden
Privatprodukte ändern; keine neue Galerie oder Bildveröffentlichung.
