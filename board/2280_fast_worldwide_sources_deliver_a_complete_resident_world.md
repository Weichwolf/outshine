Type: feature
State: active
Architecture: planned
Priority: P0
Parent: 2169
Depends:
Area: generators, data, engine, client
Tags: sources, loading, residency

# Fast worldwide sources deliver a complete resident world

## Ergebnis und vorhandene Fähigkeit
Wien und die übrigen Places laden eine vollständige, resident renderbereite Welt über
öffentliche Provider-/Generatorverträge. Ein weltweiter Hauptanbieter je Datenart liefert
OSM-basierte Vektoren, Höhen und Wetter. Geschwindigkeit, Verfügbarkeit und Bildqualität
entscheiden; Original-OSM und vollständige Tags sind keine Pflicht. Quellenwechsel darf
Straßen, Terrainkontakt und Wassergeometrie nicht verschlechtern. Keine Place-Sonderdaten.
Original-XML, MVT, GLO-30, paralleles IO, verifizierte Netzwerkbytes/Receipts und native
Gebäude-/Terrainprodukte bestehen. Der Client erzwingt bisher den ungeeigneten OSM-
Editing-API-Katalog. Vollständige aktuelle Place-Bilder und native Straßen/Wasser bleiben offen.

## Entscheidung und nächste Lieferung
- Empfehlung nach lokalen Parallelproben: OpenFreeMap/OpenMapTiles für OSM, Mapterhorn/
  Terrarium-WebP für Höhen, Open-Meteo für Live- und Archivwetter. Auswahl ist noch keine
  integrierte oder visuell abgenommene Runtime. Öffentliche Dienste liefern keinen
  bewiesenen Langzeit-Verfügbarkeitsnachweis; Wetter-Free-Tier zeigte einzelne HTTP 503.
- OpenFreeMap erlaubt kommerzielle Tile-Nutzung ohne numerisches Requestlimit, ohne SLA.
  Automatisierte Bulk-/Offline-Erwerbsrechte der AGB vor vollständigem Flächenpreload klären;
  veröffentlichte Planet-Downloads sind ein gesonderter dokumentierter Lieferweg.
- Mapterhorn liefert globale Grundauflösung und regionale feinere Terrarium-Kacheln;
  Quellenlizenz/Datum und tatsächliche Auflösung erhalten. Globales GLO-30 bleibt DSM.
  Open-Meteo-Free ist nichtkommerziell; kommerzieller Betrieb braucht passenden Plan,
  Archivzugriff im Kundentarif Professional oder höher. Kundenendpoint ist ungemessen.
- Zuerst Anbieter-/Schema-Konfiguration bis Providerregistrierung und Wien integrieren;
  danach Central Park/Tokyo als dichte Erwerbslast und alle acht Webcam-Places prüfen.
  Erwerbsdurchsatz ist kein Weltaufbau- oder Bildnachweis. 240-km-Abdeckung vollständig
  planen; Nahdetail und Fernaggregation aus 2336 begrenzen Arbeit vor Geometrieerzeugung.
- OSM-Editing-API ist kein primärer Bulk-Lieferweg: Betreiber erlaubt höchstens zwei
  Downloadthreads und verweist Lese-/Großlast auf andere Dienste. Kein weiterer Quotenlauf.
  Vorhandene Original-Importer bleiben Bibliotheksfähigkeit; kein paralleler Client-Fallback.

## Besitzer und Datenfluss
`generators/osm` besitzt Erwerb, MVT-/XML-Decode, Schemaadapter und native Erzeugung.
Höhen-/Wettererweiterungen besitzen ihre jeweiligen Provider. Gemeinsame IO-/Cache-/Job-
Dienste kennen nur Adressen, Bytes, Versionen und Aufträge; `world` nur native Produkte.
Anbieter-API-Konfiguration → HTTP → gemeinsamer Formatdecoder → Schema-/Einheitenadapter
→ Provider-Inputs → Generator → native Welt. Keine zweite Engine oder Generator-Sonderroute.
2188 liefert den allgemeinen Lebenszyklus; vorhandene native Produkte bleiben erhalten.
IO ist begrenzt parallel, Decode/Compute gebündelt auf dem gemeinsamen Worker;
Render/Audio getrennt. Begrenzte Queues, Rückstau, Revision und Abbruch gelten durchgängig.

## Verträge und Umsetzung
- URL/Auth/XYZ-Reihenfolge/TileJSON, Hostparallelität und Cachepolicy sind Konfiguration.
  MVT teilt seinen Decoder; Shortbread/OpenMapTiles benötigen verschiedene Schemaadapter.
  Gebäuderinge einschließlich MultiPolygon/Höfen und Parts korrekt konsumieren; Zahl der
  MVT-Features ist keine Gebäudezahl. Höhen, Straßenklassen, Brücken/Tunnel und Gewässer
  normalisieren, soweit geliefert. Fehlende Semantik bleibt explizit; keine erfundenen Tags.
- Terrarium-PNG/WebP teilt Höhenformel nach Bilddecode; Tilegröße/Zoom/Datum sind explizit.
  Terrain-RGB und COG brauchen eigene Decoder. Kein RGB-Farbverlust bei Höhendecode.
  Fehlende Höhen sind kein Nullboden; native Meter und Herkunft bis Terrainkontakt erhalten.
- HTTP prüft Endstatus und Payloadformat, nicht nur HTTP 200. Authfehler terminal;
  temporäre Fehler begrenzt mit Backoff/Retry-After und gemeinsamer Ursprungssperre.
  Ausgeschöpfte Frist bleibt fehlende Abdeckung. Niemals unbegrenzt erneut anfordern.
- Cache speichert ausschließlich Netzwerkbytes mit Anbieter/Dataset/Adresse/Version/Digest.
  Vorhandene Bytes, Receipt und Teilbestände bleiben erhalten. Formatgleichheit ist keine
  Datasetgleichheit; Mirrors brauchen belegte Revision/Digest vor gemeinsamen Range-Reads.
  Verschiedene Anbieterstände nicht innerhalb derselben Region blind zusammenwürfeln.
- Tile-Ränder, Overlaps und IDs besitzen konsistente Ownership. Quellenupdates invalidieren
  abhängige Produkte gezielt; alte Abdeckung bleibt bis atomarer Publikation erhalten.
  Blickdrehung lädt/generiert keine vorhandenen Produkte neu. Residentes RAM/GPU und
  Parse-Scratch getrennt begrenzen; SSD-Cache ist keine Renderbereitschaft.
- Netzwerkvorbereitung ist getrennt vom Warmaufbau. Client, Prepare und Shots verwenden
  denselben persistenten SDL-Nutzerspeicher und dieselbe Registrierung. Keine generierten
  Geometrie-/Atlas-Diskcaches. Vollständigkeit folgt Weltbedarf statt beendetem Einzelrequest.

## Weitere Integration und Abnahme
2281/2145 erhalten Straßenanschlüsse, Brücken und Wasser; 2173 die Gebäudequalität.
2336 liefert distanzabhängigen Bedarf bis Orbit. Keine pauschale Abhängigkeit vom gesamten
Sandbox-Ausbau. Warmstart ohne Netzwerk: vollständiger gewählter Bedarf in höchstens
zehn Sekunden, danach eine Sekunde 360° im festen Profil mit tatsächlichem Bild und p99.
Alle Places bleiben Pflicht; ein echter Place im Gate. Datenmenge, Acquire/Decode/Build,
RAM/GPU und Bildqualität getrennt belegen. Durchsatz an Stichproben beweist weder
vollständige 240-km-Datenmenge noch A18-Pro-Budget oder erhaltene Bildqualität.
