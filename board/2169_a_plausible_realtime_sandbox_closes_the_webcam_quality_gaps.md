Type: feature
State: active
Architecture: planned
Priority: P0
Area: engine, world, render, simulation, audio
Tags: sandbox, visual, integration

# Outshine: eine glaubwürdige, spielbare Welt aus OSM und DEM

## Der fertige Zustand

Du stehst auf einer Straße in Wien. Um dich herum stehen vollständige Häuserzeilen mit
Eingängen, tiefen Fenstern, unterschiedlichen Baustoffen, Dachkanten und plausiblen Höfen.
Asphalt, Bordstein, Gehweg und Gebäudesockel treffen sich räumlich. Die Straße führt über
eine tragende Brücke weiter; darunter fließen Wasser und Verkehr ungehindert.
Du gehst, steigst in ein Fahrzeug und fährst aus der Stadt über Land bis in die Berge.
Es gibt keine Ladeunterbrechung, verschwundene Häuser oder springende Straßenkontakte.
Nähe, Stadt und Horizont gehören zur selben Welt; grobere Ferndarstellung bleibt kohärent.

OSM liefert Netze, Grundrisse, Nutzung und belegte Maße; DEM liefert die große Geländeform.
Deterministische Konstruktionsregeln ergänzen fehlende Gebäudeform, Details und Oberflächen.
Das Ergebnis ist eine plausible Spielwelt, keine behauptete fotografische Rekonstruktion.
Der Default ist Solarpunk 2050: elektrifizierte Mobilität, Energieanlagen und begrünte
Architektur mit materialgerechter Alterung. Szenarien bestimmen Epoche, Regeln und Besetzung;
belegte Formen und physikalische Anschlüsse haben Vorrang vor einer Stilannahme.

RDR2/GTA5 auf PS4 setzen den Maßstab für räumliche Dichte, Detailhierarchie, Licht,
Bewegung und zusammenhängende Bildwirkung. Outshine hat einen eigenen physikalisch
plausiblen Studio-Look. Nahansicht muss tragen: eine Fenstertextur auf einem Prisma reicht nicht.
Wolken, Atmosphäre, Sonne und lokale Lichter beleuchten dieselben Materialien. Regen verändert
Wasser, Glanz, Sicht und Geräusche. Menschen und Fahrzeuge bewegen sich nach Weltregeln.
Schritte, Reifen, Antriebe, Wetter und Umgebung sind räumlich hörbar, auch über Kopfhörer.

Der Spieler kann mit erklärten Objekten, Türen und Figuren interagieren, Aufgaben verfolgen,
Besitz verändern und den Zustand speichern. Eine Szenariodatei beschreibt solche Spiele;
Engine-Code enthält weder Wien-Sonderfälle noch fest verdrahtete Missionen.

## Lieferweg: jede Stufe erweitert dieselbe spielbare Welt

| Stufe | Konkretes Ergebnis | Träger-WIs | Wirkliche Voraussetzungen |
|---|---|---|---|
| 0 / P0 | Vorhandene Städte vollständig zeigen; Wien verliert keine Gebäude | 2319, 2224, 2243 | Verluststelle in Quelle, Bake, Residency oder Publikation beheben |
| 1 / P0 | Begeh- und befahrbarer Straßenraum mit Knoten, Brücken und Tunneln | 2133, 2281, 2121, 2175, 2257 | Semantisches Netz 2278 und gemeinsamer räumlicher Bezug |
| 2 / P1 | Glaubwürdige Nahansicht: Baustoffe, Gebäudemassen und räumliche Fassaden | 2171, 2173, 2138, 2168 | Vorhandener Materialpfad; Gebäudesemantik und zugängliche Straßenfront |
| 3 / P1 | Stadt, Wasser und Landschaft bilden ein kohärentes beleuchtetes Bild | 2166, 2145, 2129, 2167, 2128, 2155 | Gültige Oberflächen und Materialantwort; kein fertiger Wetterausbau nötig |
| 4 / P1 | Tatsächliches Fahren und Gehen mit Kontakten, Steuerung und Kamera | 2127, 2297, 2261 | Räumliche Straße, Kollisionsprodukte und fester Simulationstakt |
| 5 / P1 | Tag/Nacht, Regen und Wolken verändern Bild, Fahrbahn und Akustik konsistent | 2172, 2140, 2213, 2212 | Ein gemeinsamer Zeit-/Wetterzustand; Licht und Materialantwort |
| 6 / P1 | Verkehr und Fußgänger beleben den benutzbaren Straßenraum | 2136, 2130, 2133 | Navigation, Kontakte, Animation und begrenzte Verhaltensarbeit |
| 7 / P1 | Ein deklaratives Spiel mit Interaktion, Aufgabe, UI und speicherbarem Zustand | 2141, 2135, 2242, 2151 | Stabile Weltidentitäten, Simulation und versionierter Spielzustand |
| 8 / P2 zuletzt | Vegetation vervollständigt Stadt und Landschaft im selben Budget | 2111, 2176, 2282 | Tragfähige Szene, Standorte und gemeinsame Qualitäts-/Kostenstufen |

Stufen ordnen Lieferungen, keine monolithische Wasserfallentwicklung. Materialien können
während Straßenarbeit entstehen; Kontakt/Steuerung beginnt am ersten gültigen Straßenstück.
Hockenheim liefert den ersten integrierten Fahrfall. Wien und die anderen Places verhindern,
dass daraus eine Rennstrecken-Demo statt einer weltweit nutzbaren Sandbox wird.
Die kleine unmittelbar ausführbare Reserve und Modulbesitzer stehen in 2188.

## Architektur, die das ermöglicht

Provider → semantische Welt → native Generatoren → versionierte Weltprodukte.
Simulation besitzt Zustand; Renderer und Audio konsumieren konsistente Snapshots.
Logische Navigation, physischer Kontakt und Render-LOD sind getrennte Produkte mit denselben
Quellidentitäten und Raumreferenzen. Darstellungswechsel verändern keine Verkehrsverbindung.
Generatoren bauen Form; Materialien beschreiben Oberflächen; Beleuchtung macht sie sichtbar.
Ein Materialshader kaschiert weder falsche Geometrie noch fehlende Gebäude.

Streaming lädt vor der Bewegung, hält eine gültige Darstellung und ersetzt nur passende
Produkte. Sichtbarkeit, Instancing, Detailwahl, Uploads und Residency begrenzen die Kosten.
2298/2312 liefern nutzbares Gebäude-LOD; weitere isolierte Beweisverfeinerung ist kein Meilenstein.
Gemeinsames Ziel: A18 Pro, 8 GB, 720p60. Stadt und Wald halten dieselbe Zeitobergrenze.
OSM-Bauwerke, Terrain, Vegetation, Himmel und Wolken teilen Budget nach sichtbarem Nutzen;
keine festen Klassenquoten. Himmel/Wolken erhalten ihrem großen Bildanteil entsprechendes Gewicht.
2092/2228/2314 begleiten jede Stufe mit Laufzeit-, Speicher- und Qualitätsentscheidungen.

## Was heute fehlt

Wien/Graz/Olympiaturm erreichen keine vollständige Refined-Szene. Die geöffneten Bilder zeigen
fehlende Stadtteile, repetitive Fassaden, schwache Straßenräume, flaches Wasser, Terrainwände
und überglättete Berge. Der behauptete AAA-Maßstab ist damit noch nicht erreicht.
Die jetzigen Referenzen liegen unter build/shots/reference/6521238ba/; Historie und technische
Prüfprotokolle stehen in Git und System-Temp-Logs, nicht als Fortschrittstagebuch im Backlog.

## Fertige Lieferungen sind sichtbar und benutzbar

Jede Lieferung benennt ein vorher fehlendes Spielergebnis und zeigt es im Client in Nähe und
Bewegung. Alle Places bleiben dabei verbindlich: Wien, DarmstadtWest, Graz, Rosenheim, Husum,
Feldkirch, Malcesine, Koerbersee, Olympiaturm und Hockenheimring. Kein grüner Einzeltest ersetzt sie.
Tag/Nacht, Wetter, Kaltstart, Warmstand und Bewegung gehören zum Endzustand. Host-Messungen
beweisen keine A18-Leistung. Ein schönes Standbild beweist weder Kontakte noch Streaming.
