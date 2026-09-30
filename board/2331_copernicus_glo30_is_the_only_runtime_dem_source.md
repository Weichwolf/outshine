Type: feature
State: open
Architecture: planned
Priority: P0
Parent: 2166
Depends:
Area: world, data, terrain, engine
Tags: copernicus, original-source, webcam

# Copernicus GLO-30 supplies terrain without a substitute elevation provider

## Ergebnis und Iststand
Alle natürlichen Places verwenden ausschließlich Copernicus GLO-30 als DEM.
Der bisherige `TerrariumDem`-Default liefert eine andere Quelle und erfüllt diesen
Auftrag nicht. Decoder und historische Prüfdateien bleiben erhalten; der Runtime-
Quellenpfad wird ersetzt. Kamera, Zeit, offizielle OSM-Daten und Open-Meteo bleiben
die übrigen erlaubten Eingaben. Fehlende GLO-30-Daten sind kein Meeresspiegelwert.

## Architektur und konkrete Umsetzung
- `world/data` besitzt GLO-30-Beschaffung, Produktidentität und Originalbytes.
  Vor Implementierung offiziellen Bezugsweg, Raster-/Höhendatum, NoData und
  Revisions-/Byte-Pins anhand der Produktmetadaten festlegen. Keine behauptete
  Gleichheit von DSM-Oberfläche, nacktem Boden und lokalem Wasserstand.
- Bounded TIFF/COG-Decode liefert georeferenzierte Höhensamples an bestehende
  Terrainfelder. Codec-/IO-Details enden am Adapter; Generatoren lesen native Meter.
  Einen vorhandenen kleinen TIFF-Codec prüfen, keinen allgemeinen GIS-Stack einbauen.
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
