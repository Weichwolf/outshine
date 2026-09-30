Type: feature
State: open
Architecture: planned
Priority: P0
Parent: 2166
Depends: 2334
Area: world, data, terrain, engine
Tags: copernicus, original-source, webcam

# Copernicus GLO-30 supplies terrain without a substitute elevation provider

## Ergebnis und Iststand
Alle natürlichen Places verwenden ausschließlich Copernicus GLO-30 als DEM.
Der bisherige `TerrariumDem`-Default liefert eine andere Quelle und erfüllt diesen
Auftrag nicht. Decoder und historische Prüfdateien bleiben erhalten; der Runtime-
Quellenpfad wird ersetzt. Alte Provider und unbenutzte Quellenpfade entfernen;
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
Die erste reale Bereichsantwort liefert 16384 von 32964108 Bytes mit starkem ETag;
Folgeblöcke müssen diese Objektidentität halten. Uploaddatum ist kein Aufnahmedatum.
`world/data/CopernicusDem` nutzt libtiff hinter privaten, begrenzten IO-Callbacks;
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
  Terrainfelder. Native 1°-Quelladressen bleiben von Mercator-Renderadressen getrennt;
  Datumsbezug, Resampling und fehlende Nachbarposts erhalten explizite Herkunft.
  Codec-/IO-Details enden am Adapter; Generatoren lesen native Meter.
- Räumlich benötigte Rasterblöcke gebündelt beschaffen. Bei Bereichsanfragen
  Antwortbereich und gemeinsame Dateirevision prüfen; keine gemischten Versionen.
  Nur empfangene Quellbytes persistent cachen. Reprojektion, Terrainfelder,
  Normalen und Höhenatlanten bleiben begrenzte RAM-/GPU-Produkte.
- Bestehende Terrain-Zertifikate erhalten Originalidentität, Transformation und
  Fehlerherkunft. Kein engerer Oberflächenfehler allein durch einen Quellenwechsel.
  Straßenprofile, Deformation und native Gebäudehöhen bleiben erhalten.
- Gebäude-/Vegetationsanteile im Oberflächenmodell und Küstenübergänge explizit
  behandeln. Ableitungen vom Terrain erhalten Herkunft und Unsicherheit; weder
  Städte noch reale steile Hänge pauschal glätten. Wasser besitzt eigene Pegel.
- IO, Decode und Upload haben begrenzte Arbeit, klare Besitzer und Abbruch.
  Kein Netz-/Dateizugriff im Framepfad; Drehung verwendet residente Terrainprodukte.

## Abnahme
Client-Provenienz nennt GLO-30 und geprüfte Originalrevision; kein Terrarium-Zugriff.
Flensburgs Küste, Rosenheims Stadt und ein steiler Landschafts-Place vergleichen
Quellsamples, Terrain, Straßenanschlüsse und das tatsächliche Bild. Fehlende Raster
bleiben rot. Alle acht Places, volle Sicht, 10-s-Preload und 720p60 bleiben verbindlich.
Format, fokussierte Terrain-/Quellenprüfungen und vollständiger Lint samt clang-tidy.
