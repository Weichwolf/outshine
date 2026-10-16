# Kontinuierliche Böschungen am Wiener Treppenverbund (#434)

Der neue Versuch plant die umliegenden Bodenhöhen gemeinsam mit den ganzen
Brücken-Unterseiten. Die18Quellenkörper und sämtliche Verkehrsprofile bleiben
unverändert. Die harten Inseln der [Terrain-Gegenprobe](stair_terrain_comparison.md)
werden durch zusammenhängende Böschungen und örtliche konstruktive Stützflächen
ersetzt. Ganze Double-Kontakte und ein aus der tatsächlichen Float32-Rundung
hergeleitetes Höhenband werden getrennt geprüft. Übrige Quellen, fünf Außenprofile,
Fahr-/Gehhüllen, Statik und native Übernahme bleiben offen; #434 bleibt aktiv.

## Verfahren und räumlicher Vertrag

Eingang bleibt der [Außenversuch](stair_outer_comparison.md) mit ursprünglichen
OSM-/DEM-Hashes, Frame und Quell-IDs. Der neue Plan entsteht aus diesen versionierten
Eingängen; kein früherer privater Terrain-Ausgabebaum ist notwendig.
Ein vorläufiger Höhenfit ohne Brückenfreigabe bestimmt die zu groben Bodenfacetten.
Nur deren innere Boden-/Bodenkanten und Facetten erhalten freie Mittelpunkte.
Ganze ursprüngliche Boden-Kontaktfacetten, deren Endpunkte und ihre Höhen bleiben
fest. Danach werden beide Zielfunktionen auf derselben eingefrorenen Konnektivität
und mit denselben Qualitätsbedingungen geprüft.

Desbrun et al., SIGGRAPH1999, [Originalpaper](https://people.eecs.berkeley.edu/~jrs/meshpapers/DesbrunMeyerSchroderBarr.pdf),
[lokales PDF](../../../doc/references/transport/siggraph/1999-implicit-fairing.pdf),
S.2§2.1/Gl.2 und S.3§2.7 wurden optisch geprüft: Membranenergie sowie harte/weiche
Kontakte. Galin2010 Fig.20 liefert weiterhin die Gelände-/Profiltrennung.
Hier wird ein eigener skalarer Reparaturfit auf festen XY-Dreiecken verwendet:
`delta = h - DEM`, Energie `deltaᵀ M delta + lambda deltaᵀ K delta` mit
flächengewichteter Masse und `K = Gᵀ area G`.
Das ist weder der vollständige Krümmungsfluss noch eine volumenerhaltende Methode.

Erdböschungen haben weiterhin ganze Gradienten≤2/3. Nicht begehbare konstruktive
Stützflächen sind nur vollständig innerhalb5m der gewählten Bodenwege erlaubt.
Ihre zusätzliche Gradientenzulage wird flächengewichtet bestraft; sie ändern kein
Verkehrsprofil. Begrenzte konstruktive Varianten2 und10 werden getrennt geprüft.
Das sind neue Stützflächen, keine steileren als Erde ausgegebenen Böschungen.
Jede tatsächliche Klasse muss sowohl Double als auch Float32 bestehen.

Jeder positive Boden-/Brücken-Dreiecksüberlapp liefert Höhenbedingungen an allen
Schnittpolygonvertices. Der Boden bleibt unter der ganzen wirklichen Unterseite;
ein10cm-Abstand läuft innerhalb1m zum tatsächlichen Bodenkontakt auf0 aus.
Direkte unabhängige ganze Dreieckstests prüfen danach beide gespeicherten Exporte.
Dieser Bodenabstand ist keine Geh-/Fahrhülle; die bestehenden sechs Verkehrs-
Unterführungen behalten ihre bisherigen Bedingungen und Körper.

## Ganzes Kontaktband im tatsächlichen Export

Der Terrain-Cap ist eine einzige kontinuierliche Höhenfunktion auf einem konvexen
Rechteck. Doppelte XY mit verschiedenen Höhen, innere Sprünge, Löcher, umgedrehte
Facetten und offene/mehrdeutige Körper werden abgelehnt. Jede ursprüngliche
Bodenfacette besitzt vollständige metrische Abdeckung durch echte Kindfacetten;
deren ganzen affinen Elternhöhen werden geprüft.

Für den tatsächlichen Float32-Export werden die größten XY-/Z-Vertexbewegungen
von Terrain und Originalboden gemessen. Die maximale Double-Terrainnorm ist eine
globale LipschitzkonstanteL. Ein baryzentrischer Punkt bewegt sich höchstens wie
sein am stärksten gerundeter Eckpunkt. Deshalb gilt über dem ganzen ursprünglichen
Bodenprodukt das Höhenband
`E = e_Double + delta_z_T + delta_z_G + L * (delta_xy_T + delta_xy_G)`.
`e_Double` enthält die gemessene Elternhöhenabweichung und die bestehende metrische
Schnittauflösung. Die tatsächliche Float32-Fläche muss das komplette Bodenprodukt
abdecken; ganze positive Dreiecksüberlappungen müssen in beiden Richtungen innerhalbE
liegen. Keine bloße Stichprobe oder frei gewählte zusätzliche Kontakt-Toleranz.

Die frühere geschlossene Stützwandfläche aus #475 erfüllt die C0-Voraussetzung
ausdrücklich nicht und wird schon davor abgelehnt. Deren meterhoher Randabstand
wird durch diese Herleitung nicht freigegeben. Ein exaktes Nullvolumen überlappender
Float32-Solids wird weiterhin nicht behauptet; das endliche Kontaktband wird belegt.

## Reproduktion und verbleibende Abnahme

[Aufrufe und Pakete](README.md). Die lokalen Kontrollen enthalten echte Quellen,
Innenflächenkonflikte ohne Eckpunkttreffer, veränderte Materialrollen, den früheren
Sprung-Gegenfall und absichtlich unterschiedliche Float32-Triangulationen.
Der reine Datenfit und der Membranfit unterliegen denselben Exportbedingungen;
ein erfolgreicher Solverstatus darf einen ungültigen Export nicht freigeben.
Beide Kandidaten, auch ein abgelehnter Rohfit, werden mit ihren Ursachen erhalten.

Kein SDK, Manifold oder nativer Produktcode gehört zum neuen Verfahren.
Die vorhandenen Bibliotheksoperatoren bleiben unverändert. Neue räumliche/native
APIs oder eine globale Quellfreigabe werden nicht aus diesem örtlichen Versuch
abgeleitet. Materialparameter anderer Besitzer bleiben unberührt; die technische
Python-Darstellung nutzt vorhandene Erd-/Betonfarben.
