Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2331
Depends:
Area: public-api, host, world, data
Tags: copernicus, cog, memory, io

# COG source blocks use bounded, revision-checked HTTP ranges

## Ergebnis und Iststand
Provider laden benötigte Originalbytebereiche statt vollständiger GLO-30-Dateien.
Der gemessene Flensburg-COG umfasst 32 964 108 Bytes; sein Header und einzelne
komprimierte Rasterblöcke benötigen nur einen Teil davon. Der gemeinsame Transport
unterstützt bisher ausschließlich vollständige Antworten. GLO-30-Decodierung und
Terrainintegration bleiben WI 2331; dieser Baustein ist deren Beschaffungsvertrag.

## Vertrag und Umsetzung
- `world/data/Transport.h` erhält einen öffentlichen Bytebereich aus Startoffset
  und positiver Länge sowie eine Bereichsanfrage mit optionalem starkem ETag-Pin.
  Ungültige Bereiche und übergroße Anfragen werden vor der Aufnahme verweigert.
  Transporte ohne Bereichsunterstützung verweigern die Erweiterung explizit.
- `host/Fetching` nutzt denselben libcurl-Multi-Arbeiter, begrenzte Anfrageplätze,
  Abbruch und Deadlines wie vollständige Antworten. Keine neue IO-Queue.
- Die Anfrage setzt einen einzelnen HTTP-Range, `Accept-Encoding: identity` und
  bei Folgeblöcken `If-Match`. HTTP-206-Erfolg verlangt passenden Content-Range,
  exakte Bodylänge, bekannte Dateigröße und starkes ETag; ein Pin muss übereinstimmen.
  Ignorierte Bereiche, falsche Offsets, Encoding und gemischte Revisionen sind Fehler.
  HTTP-Fehler behalten ihren Status; sie werden keine erfolgreichen Quellbytes.
- Empfangene Bytes sind bereits während des Downloads auf die Bereichslänge und
  das Transportbudget begrenzt. Antwortmetadaten besitzen Bereich, Dateigröße und
  ETag. Redirect-/Zwischenantworten dürfen keine alten Metadaten übernehmen.
- Provider besitzen Reihenfolge und Rohdaten-Cacheidentität ihrer Blöcke; dieser
  Transport cachet weder Bytes noch generierte Terrainprodukte selbst.

## Abnahme
Lokale HTTP-Gegenbeispiele belegen tatsächliche Range-/If-Match-Header, Bodygrenze,
Antwortbereich und Revisionsprüfung. Der reale Flensburg-COG liefert seinen Header
über denselben Transport ohne vollständigen Download. Vollständige Antworten,
Abbruch und begrenzte Queue bleiben funktionsfähig. Das ersetzt kein Place-Gate.
