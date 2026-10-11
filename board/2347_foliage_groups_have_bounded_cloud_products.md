Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2111
Depends:
Area: generators, render
Tags: vegetation, python, groups, lod

# Foliage groups have bounded cloud products

## Ergebnis und Besitzer
2346s räumliche Blattgruppen werden als günstigere Produkte für denselben Pflanzenplan
darstellbar. `test/experiments/vegetation` besitzt eigene Verfahren, Vergleiche und Bilder;
2111 besitzt vollständige Wälder, Artenvielfalt und die native Integration.
Die Laubbaumprobe benötigt noch 2,65 Mio. Dreiecke. Einzelblattkarten verlieren bei
flachen Richtungen Deckung; gruppierte räumliche Ebenen sind der nächste Vergleich.

## Quellen und Verfahren
[Billboard Clouds, SIGGRAPH 2003](../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf):
fehlerbegrenzte Ebenen und projizierte unbeleuchtete Attribute; PDF-Seite 4 optisch betrachtet.
Drei Achsen mit Tiefenschichten verlieren im 96-Richtungs-Vergleich 33–46 % mittlere Deckung.
`group_cloud.py`: Blatt-PCA liefert Kandidatenebenen; Greedy wählt jeweils die meisten
noch nicht zugeordneten Organe innerhalb 15 mm und 0,26 rad Normalenabweichung.
Alle Organe genau einmal; Quads aus allen projizierten Ecken. Keine globale Minimalität.
Maximal 256 Organe je Gruppenanfrage; unzureichendes Budget erzeugt keine Platzierungen.
Aus dem gleichen 16×2-Blattmesh selbst Farbe, Normalen und Alpha rasterisieren;
vormultiplizierte Filterung. Keine fremden Assets, Fototexturen oder gespeicherte Beleuchtung.
Ebene/Bounds aus dem ganzen zugeordneten Mesh ableiten; maximalen Projektionsversatz an
allen Dreiecksecken prüfen. Diese Schranke belegt weder Deckung noch Licht-/Schattenfehler.
Kamerafreier Plan/IDs und Gruppenanker bleiben erhalten. Karten sind Darstellung, keine neuen Blattpläne.
Gegen dieselben räumlichen Gruppen: feste orthographische Richtungen, Deckung und
qualitative zweiseitige Lichtantwort; danach tatsächlich betrachtete 3D-Baum-/Bestandsbilder.
2348s Sichtbarkeits-/Größenrang und Blattvergrößerung begründen einen eigenen Vergleich:
stabile Organ-IDs, gleiches Blattbudget; Zufall, sichtbare Fläche und räumlich verteilte Auswahl.
Eigene Sichtflächen direkt aus begrenzten Tiefenrastern bestimmen; kein Modeler-Feld kopiert.
Vergrößerung um Organanker mit Quadratwurzel des Flächenverhältnisses nur als Hypothese;
Blattdeckung, Normalen-/Schattenantwort und Silhouette unabhängig messen. Keine automatische LOD-Freigabe.
Gegen gröbere fehlerbegrenzte Kartengruppen auf identischen Ausschnitten vergleichen.
2348s Ansichten-Atlas begründet eine weitere Alternative: unbeleuchtete Rundum-Ansichten
je stabiler Gruppe vorhalten, nur die nächstgelegene Karte beim Draw wählen. Kamera erzeugt
keinen neuen Plan; Licht-/Schattenansichten brauchen dieselbe getrennte Auswahl.
Eigene gleichmäßig verteilte Kugelrichtungen statt eines übernommenen Modeler-Gitters;
Deckung auf unabhängigen Richtungen prüfen. Tiefenparallaxe und Übergänge bleiben offen.
`cloud_product.py` hält vollständige Atlas-/Meshprodukte; Kamera-Snapshots sind abgeleitet.
`view_snapshot` wählt getrennt nach Perspektivauge oder orthographischer Richtung, auch für
Schattenbedarf. Kamerageometrie allein verliert im Baumvergleich den Großteil des Schattens.
`shadow_product.py` leitet Kamera-, Sonnen- und Fülllicht-Snapshots aus demselben Cache ab;
EEVEE-Schattenverknüpfung und Light-Path-Materialmasken trennen ihre Verwendung.
[Blender 4.3](https://www.blender.org/download/releases/4-3/) belegt den Backendpfad;
Alpha-/Schattenbilder gegen identische Lichtparameter der räumlichen Referenz prüfen.
Karten je Licht unabhängig gegen eine Schleife geprüft; deklarierte/gespeicherte Dreieckzahlen gleich.
Vorläufige Vorschau erhält wieder einen kräftigen Baumschatten. Eigener nativer Schattenpass bleibt offen.

## Datenfluss und Kosten
Teilgruppe aus stabilen Astpfaden anfordern; vollständiges angefordertes Produkt erzeugen.
Rohplan, Chartprodukt und Kamerabedarf getrennt. Drehung nutzt vorhandene räumliche Produkte;
Bewegung fordert Details desselben Plans an. Persistenter Cache und Windintegration bleiben offen.
Textur-/Geometriebytes, Chartaufbau und Rasterkosten getrennt von Blender/Runtime messen.
Auswertung einzelner Gruppen entscheidet vor einem teuren vollständigen Baumaufbau.
Ein Atlas bündelt Charts mit expliziten Rändern; Mip-/Overdraw-Kosten bleiben Teil der Prüfung.
Der vollständige Prototypaufbau expandiert alle angeforderten Gruppen einmal; kein direkter
Fernwaldaufbau ohne Organarbeit belegt. Wiederverwendbare Produkte verhindern diesen Aufbau bei Draws.

## Abnahme und offene Lücke
Identische Quellorgan-IDs/Anker bei wiederholten und umgeordneten Anfragen; gültige Geometrie.
Alle Organe genau einem Chart zugeordnet; Alpha/Attribute endlich, Filtermittel erhalten.
`cloud_experiment.py`: vier 60-Blatt-Gruppen × 24 Richtungen auf einem gemeinsamen 8×8-Raster.
Viewport aus allen Vergleichsprodukten einmal ableiten; identische Referenzdeckung unabhängig prüfen.
Der vorherige Paarvergleich variierte Ausschnitte; seine Fehlermittel sind als Verfahrensranking ersetzt.
Gemeinsamer Ausschnitt: 8×8-Charts 7,5 % mittlere/11,7 % maximale Deckungsabweichung;
diffuse Proxyantwort maximal 8,2 %. 16×16: 6,1 %/10,4 %; kein allgemeines LOD-Gate.
`raster_batch.py`: begrenzte Dreieck-/Pixelpaare, Tiefen-Maximum und stabile letzte Gleichstände.
Gleiche Coverage/Attribute gegen Referenz; 16 Prüfungen. Lokale CPU-Mediane aus fünf gleichen Eingaben:
64×64-Raster von 3.960 Dreiecken: rund 520 ms einzeln gegen 4,8 ms in Batches; kein GPU-Nachweis.
`cloud_product.py`: 630 Gruppen/37.800 Organe → 32.059 Karten/64.118 Laubdreiecke statt 2,49 Mio.
Holz unverändert; zwei Bündel statt neun im Blender-Vergleich. 2880×2864-Atlas, vier Randtexel.
RGBA8 Farbe/Normalen 62,9 MiB; theoretisch 83,9 MiB samt Mips, GPU-Format/Overdraw ungemessen.
Pflanzenrezept versioniert speichern; frischer Plan stellt identische angeforderte Details her.
Nahe Geometrie und Karten haben dieselbe Identität; Quellaufbereitung bleibt außerhalb des Produkts.
Showcase zeigt Deckung/Kartenbedarf, gleiche CPU-Rastereingaben und unveränderte Bildausschnitte.
`clouds/clouds-build.json` hält den geprüften vollständigen Produktaufbau; aktuelle Gruppenprüfung
mit `--study-only` erhält dessen unveränderte Produkte und die geprüften Renderings.
Keine allgemeine LOD-Qualität aus kleinen Positionsfehlern oder wenigen Bildern ableiten.
Vollständige Fernwaldaggregation, natürliche Arten-/Bestandsvielfalt, Wind, native GPU-Kosten
und A18-Pro-Budgets bleiben offen. CPU-/Blender-Proben schließen kein Runtime-Gate.
`selection_experiment.py`: 13 Verfahren × vier Gruppen × 24 Richtungen, gemeinsamer Ausschnitt.
Halbe Blattzahl mit Flächenausgleich: 31–38 % mittlere Deckungsabweichung; keine allgemeine LOD-Lösung.
64 Ansichten bei 16×16: rund 15 % mittlere/26 % maximale Deckungsabweichung, eine Karte je Draw.
Mittlere Normalen verlieren Lichtantwort: Proxyabweichung bis 52 %; vollständige BRDF bleibt offen.
Normalsignale in eine gemeinsame Hemisphäre falten hilft der Einzelprobe nicht zuverlässig;
Normalenverteilung vor einer Material-/LOD-Freigabe erhalten statt nur eine gemittelte Richtung.
Vollständige Baumprobe: 630 Gruppen × 64 Ansichten, 37.800 Organe; 1.260 Laubdreiecke je Kamera-Snapshot.
4824×4824-Atlas, RGBA8-Paar 177,5 MiB vor Mips. Gefrorener Aufbau 251 s, Prozesspeak 469 MiB.
`view_cache_experiment.py`: frischer Prozess, kompletter Cachedecode rund eine Sekunde; keine Generierung.
30 Auswahlen einer Rundumdrehung erhalten Gruppen/IDs und stimmen mit umgeordneten Anfragen überein.
Gemessene Einzelgruppen-Schleife durch NumPy-Batches ersetzt; identische Indizes, getrennte Kosten.
Indizes: Mediane 4,80 ms Schleife/1,79 ms Batch; Snapshot samt Meshkopie p50 1,96/p99 3,21 ms.
Frischer vollständiger CPU-Cachedecode 1,08 s, Peak 498 MiB; kein natives Frame-/GPU-/A18-Pro-Gate.
Atlas-/Mip-/GPU-Kosten, bewegte Übergänge und eigenes Schatten-LOD verhindern noch eine Qualitätsfreigabe.
