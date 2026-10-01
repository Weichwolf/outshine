Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2166
Depends: 2334
Area: world, data, terrain, engine
Tags: copernicus, original-source, webcam

# Copernicus GLO-30 supplies terrain without a substitute elevation provider

## Ergebnis und Iststand
Ziel: Alle natürlichen Places verwenden ausschließlich Copernicus GLO-30 als DEM.
Der native COG-Decoder liest Metadaten und einzelne Höhenblöcke aus gelieferten
Originalbereichen; Datei, Länge und Revision bleiben gekoppelt. Öffentliche Quelladressen
unterscheiden native 1°-Zellen, Mercator-Kacheln und Produktindizes; Provider, Lieferung
und Rohdaten-Cache erhalten diese Identität. `CopernicusDem` beschafft Originalbereiche
über `SourceSet`; Receipts und Bytes bleiben beim erneuten Öffnen des Caches gekoppelt.
`CopernicusTerrain` resampelt native Zellen und Übersichten auf dem gemeinsamen
Terrain-Worker; Float-Meter, Zellgrenzen, fehlende Posts und Originalrevisionen
bleiben erhalten. IO verwendet denselben Providervertrag für eingebaute und externe
Quellen. Der eingebaute Terrain-Default ist GLO-30; reale Place-Bilder und Budgets
sind noch nicht abgenommen. Native Zertifikate bleiben unbewiesen.
Decoder und historische Prüfdateien bleiben erhalten. Alte Provider und unbenutzte Quellenpfade entfernen;
gemeinsam genutzte Terrain-/Straßenlogik erhalten. Kamera, Zeit, offizielle OSM-Daten und Open-Meteo bleiben
die übrigen erlaubten Eingaben. Fehlende GLO-30-Daten sind kein Meeresspiegelwert.

## Belegter Rastervertrag und Decoderwahl
Der GLO-30-COG für N54/E009 ist über Bytebereiche erreichbar. Sein Header deklariert
2400×3600 Float32-Samples, Deflate mit Float-Predictor und 1024×1024-Blöcke;
EPSG:4326, PixelIsPoint und eingebettete Übersichtsstufen. Georeferenz übernehmen,
keine quadratischen Samples oder feste Längenauflösung annehmen.
Das [Copernicus-Produkthandbuch 5.0 vom 29.11.2022](https://dataspace.copernicus.eu/sites/default/files/media/files/2024-06/geo1988-copernicusdem-spe-002_producthandbook_i5.0.pdf)
belegt orthometrische Meter über EGM2008 (EPSG:3855), keine Ellipsoidhöhen.
Die Spezifikation nennt NoData -32767 ausdrücklich für EEA-10; diesen Wert nicht
ungeprüft auf GLO-30 übertragen. Raster-NoData-Metadaten und nichtendliche Samples
beachten; fehlende Dateien bleiben fehlende Quellen. Das Bucket-Readme beschreibt
entfernte Ost-/Südrandposts und gemittelte Übersichten, keine Grenzpost-Duplikation.
`world/data/CopernicusRaster` liest unveränderliche Originalbereiche mit deklarierter
Dateilänge über begrenzte libtiff-Callbacks. Fehlende Bytes liefern einen Bereichsbedarf,
kein IO auf Compute. Metadaten und ausgewählte Blöcke sind getrennte endliche Aufträge.
PixelIsPoint und gemittelte Übersichten behalten ihren jeweiligen Sample-Ursprung;
gesonderte X-/Y-Abstände und NoData bleiben erhalten. libtiff begrenzt Allokationen;
kein eigener TIFF-Decoder oder allgemeiner GIS-Stack. Originalblöcke/Übersichten
liefern native Meter an bestehende Terrainfelder; das Providerformat endet dort.

## Architektur und konkrete Umsetzung
- Die öffentliche Providerregistrierung aus 2332 ist verfügbar; die übrige
  Erweiterungsarbeit dort blockiert den GLO-30-Adapter nicht. GLO-30 nutzt denselben Transport,
  Rohdaten-Cache und Fehlerfluss wie ein Provider eines Bibliotheksnutzers. Kein
  paralleler privater Copernicus-Pfad. Der öffentliche S3-Bucket verwendet HTTPS.
- `world/data` besitzt GLO-30-Beschaffung, Produktidentität und Originalbytes.
  Vor Implementierung offiziellen Bezugsweg, Raster-/Höhendatum, NoData und
  Revisions-/Byte-Pins anhand der Produktmetadaten festlegen. Keine behauptete
  Gleichheit von DSM-Oberfläche, nacktem Boden und lokalem Wasserstand.
- Bounded libtiff-Decode liefert georeferenzierte Höhensamples an bestehende
  Terrainfelder. `Address::AtCell` trennt native 1°-Quellzellen von `Address::At`-Renderkacheln;
  Datumsbezug, Resampling und fehlende Nachbarposts erhalten explizite Herkunft.
  Codec-/IO-Details enden am Adapter; Generatoren lesen native Meter.
- Räumlich benötigte Rasterblöcke gebündelt beschaffen. `Fetch` trägt Bereich und ETag-Pin;
  SourceSet prüft Antwortbereich, Dateilänge und Revision vor Lieferung und Cache.
  Atomare Rohdateneinträge koppeln Originalbytes und Receipt; HTTP 412 meldet Quellenänderung.
  Reprojektion, Terrainfelder,
  Normalen und Höhenatlanten bleiben begrenzte RAM-/GPU-Produkte.
- Bestehende Terrain-Zertifikate erhalten Originalidentität, Transformation und
  Fehlerherkunft. Kein engerer Oberflächenfehler allein durch einen Quellenwechsel.
  Straßenprofile, Deformation und native Gebäudehöhen bleiben erhalten.
- Gebäude-/Vegetationsanteile im Oberflächenmodell und Küstenübergänge explizit
  behandeln. Ableitungen vom Terrain erhalten Herkunft und Unsicherheit; weder
  Städte noch reale steile Hänge pauschal glätten. Wasser besitzt eigene Pegel.
- IO, Decode und Upload haben begrenzte Arbeit, klare Besitzer und Abbruch.
  Downloads laufen gebündelt parallel über libcurl-Multi; Decode/Resampling verwendet
  einen Compute-Worker. Render-/Audioarbeit und IO blockieren diesen nicht.
  Der gemeinsame Terrain-Executor ist verfügbar; die übrigen Umstellungen in 2335
  blockieren den DSM-Adapter nicht mehr.
  Kein Netz-/Dateizugriff im Framepfad; Drehung verwendet residente Terrainprodukte.

## Abnahme
Client-Provenienz nennt GLO-30 und geprüfte Originalrevision; kein Terrarium-Zugriff.
Flensburgs Küste, Rosenheims Stadt und ein steiler Landschafts-Place vergleichen
Quellsamples, Terrain, Straßenanschlüsse und das tatsächliche Bild. Fehlende Raster
bleiben rot. Alle acht Places, volle Sicht, 10-s-Preload und 720p60 bleiben verbindlich.
Format, fokussierte Terrain-/Quellenprüfungen und vollständiger Lint samt clang-tidy.
