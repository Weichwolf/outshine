Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends: 2336
Area: generators, data, engine, client
Tags: loading, cache, residency

# Cached source data becomes a complete resident place without wasted work

## Ergebnis und Ist
Wien erreicht aus vollständigem Quellcache die fertige Rundumwelt und den Screenshot im
AGENTS-Budget. MVT/Terrarium, paralleler Erwerb, Quellcache und native Produkte bestehen.
Zellvorbereitung endet mit lokalen Höheneingaben oder einem expliziten Fehler. Wien erreicht
den Screenshot, doch Metal bricht Renderaufträge wegen Speicherüberlastung ab. Gebäude-
Zusatzgeometrie entfernen und Ferncluster wiederherstellen (2336); danach verbleibende
Lade-/Framekosten optimieren. Abnahme ist die vollständig sichtbare Welt,
nicht ein interner Ready-Zustand oder die Anzahl erzeugter Dreiecke.

## Besitzer und Grenzen
SourceSet/ContentStore/TilePool besitzen gemeinsame Bytes/Jobs; Erweiterungen ihre Formate.
HeightSheets, SourcedTerrainFields, StructureSourcePreparation/BuildQueue und EnginePreload
werden bis zur vollständigen Publikation integriert. 2336 besitzt räumliche Detailplanung,
2188 den gemeinsamen öffentlichen Lebenszyklus. Depends 2336 verlangt den ausführbaren
Nah-/Fernbedarf und das vollständige Quellmanifest für die Budgetabnahme, nicht den Orbit-Ausbau.
Cache-/Stillstandsreparatur beginnt unabhängig davon. Keine neue parallele Ladepipeline.

## Datenfluss und Umsetzung
Kamera/AGL/Sichtweite → Rundumbedarf mit Entfernungsdetail → Quellcache oder paralleler Erwerb
→ kompakte gemeinsame Eingaben → benötigte native Geometrie → resident halten und rendern.
Drehung ändert nur die Sichtauswahl; Bewegung ergänzt Bedarf. Alte Verwaltung ohne notwendigen
Beitrag zu diesem Ablauf entfernen; Tests gegen diesen fachlichen Vertrag prüfen.

1. Jeder Zellauftrag erhält nur seine benötigten Raster samt Nachbarn. Übergröße zerlegt
   den Auftrag oder ergibt einen benannten Fehler; kein dauerhaftes Deferred ohne ausführbare
   Fortsetzung. Diagnose nennt fehlende Eingabe, laufenden Job oder konkrete Budgetverletzung.
2. Einmal dekodierte Raster unveränderlich teilen. Byte-/DecodedCache, HeightSheets und
   HeightField dürfen für denselben unveränderten Input keine erneuten Vollkopien/Hashes
   verlangen. Kachelzugriff indizieren; Quellarchive nach Übernahme kompakter Inputs freigeben.
3. Quellen-Zertifikate, TerrainRevisionIndex und Metadatenreservierungen entfernen.
   Schlüssel aus Anbieter, Anfrageparametern und Formatversion; Treffer liefern gespeicherte
   Bytes ohne Aktualitätsprüfung. Identität der Inputs/Generatorparameter einmal bestimmen.
   Expliziter Welt-/Quellenwechsel verwirft alte Jobs; Terrainänderungen erneuern ihre Produkte.
   Die bestehende Eingaberevision bindet Jobs und Produkte an denselben Terrainstand; keine
   zweite Gültigkeitshierarchie. Identische Wiederlieferung allein entwertet keine Geometrie.
   Unversionierte Eingaben brauchen Inhaltsvergleich; zwei unbekannte Revisionen sind kein Beweis.
   Vollständige Raster/Nachbarn, echte Fehler und geometrische LOD-Schranken bleiben verbindlich.
4. Gebäudepläne/Kontakte vom Mesh trennen (2336). Keine volle Quellgeometrie nur als Vorstufe
   derselben Zellgeometrie erzeugen. Snapshot-/Publikationskopien auf veränderte Produkte
   begrenzen; gemeinsame Inputs bis zur letzten Nutzung halten. GPU-Lebensdauer erhalten.
5. Fortschritt ereignisgesteuert wecken; kein Polling auf unmöglich gewordene Zustände.
   Unabhängige Vorbereitungen dürfen vorlaufen: Baumprototypen brauchen keine fertigen Gebäude.
   Finales Ready verlangt weiterhin alle deklarierten Inhalte. Ein gemeinsamer Compute-Worker,
   begrenzter paralleler Erwerb; kein IO-Warten im Compute-/Renderpfad.
6. Tote Runtime-Diskcache-Zweige für Gebäudemeshes/Atlanten entfernen, vorhandene Dateien
   erhalten. Referenzwerkzeuge/Spielstände sind getrennte Zwecke. Gemeinsam belegten RAM,
   Scratch, publizierte Produkte und GPU-Ressourcen nach Besitzer statt mehrfach zählen.

## Quellenvertrag
- OpenFreeMap: MVT/OpenMapTiles; Mapterhorn: Terrarium-WebP; Open-Meteo: JSON.
  Ein Anbieter je Datenart, Endpunkte/Auth Konfiguration. Gleicher Formatdecoder erlaubt
  Adapterwechsel, nicht ungeprüftes Mischen. Byte-Range-Teile benötigen konsistente Archividentität.
- HTTP-Endstatus/Payload prüfen; Auth terminal, temporäre Fehler begrenzt mit Backoff/Retry-After,
  Frist/Abbruch/Rückstau. Bestätigte Höhenlücke darf ein Elternraster desselben Anbieters liefern;
  Transportfehler sind kein NoData. Gelieferte Auflösung/Adresse/Datum erhalten.
- Bestehende XML/MVT/COG-Reader nur als tatsächlich genutzte Provider-/Importfähigkeit erhalten;
  kein zweiter Weltgenerator oder automatischer Altanbieter-Fallback. Decoder gehören zur Erweiterung.
- OpenFreeMap-Bulk/Offline-Nutzung und Mapterhorn-Attribution klären; Stichproben sind kein SLA.
  Open-Meteo-Free ist für nichtkommerzielle Evaluation, kommerzielles Archiv benötigt passenden Tarif.
  Anbieteranalyse bleibt Hintergrund, keine Pflicht zum Ausbau unbenutzter Adapter/Container.
- 2336 bestimmt vollständige Nah-/Fernquellen. 49 geladene Nahkacheln beweisen keine 240-km-Welt.
  Prepare und Shots verwenden denselben Bedarf, Quellcache und erforderlichen Produktstand.

## Abnahme
Wien: vollständiges Offline-Bild, keine verlorenen Gebäude/Straßen/Gewässer und kein unbegründet
wartender Auftrag. Warmaufbau, Dekodierungen/Kopien, CPU/GPU und Speicher getrennt belegen;
Limits nicht erhöhen, Sichtweite/Inhalte nicht kürzen. Danach Central Park/Tokyo als Lastfälle
und alle acht Pflicht-Places. Konkrete Zahlen/Logs bleiben außerhalb des WI.
