Type: defect
State: active
Parent: 2188
Area: import, world
Tags: correctness, memory, format, bounded

# Vector tiles are validated before native publication

## Befund
Der ursprüngliche OsmVector::Parse (Komplexität 176) las Float/Double ohne Restlängenprüfung.
int64 wird als unsigned, sint64 über einen 32-Bit-Decoder interpretiert. Fehler
verschachtelter Value-Reader werden ignoriert. Varint-Byte 10 kann überlaufen.
OsmField::Accept (Komplexität 31) publiziert vor vollständiger Prüfung; Größen
werden ungeprüft nach int/uint32/uint16 verengt. Geometrie-Deltas können int32
überlaufen; Version, Extent, Tags und Geometriekommandos sind nicht vollständig geprüft.

## Vertrag und Lösung
- Importformat bleibt an der Grenze. Native Kachelprodukte enthalten keine Reader-
  oder Eingabepufferreferenzen; Veröffentlichung erst nach erfolgreicher Validierung.
- Fehler unterscheiden von fehlender Ebene; expected mit begrenztem Decode-/Bytebudget.
- Typen/Einheiten/Indexräume ausdrücklich; Zahlen nicht durch Double als universellen
  Integer-Speicher schleusen. Strings besitzen, vorübergehende Bytes als Span leihen.
- Gesamten Decoder in prüfbare Zuständigkeiten zerlegen. Für den vollständigen Ersatz
  etablierte Protobuf/MVT-Reader gegen No-Exceptions und Ressourcenbudgets bewerten;
  keine zweite unbewiesene Universal-Protobuf-Implementierung aufbauen.

## Implementierte Verträge
- Begrenzte Little-Endian-Fixed32/64- und Varint-Lesezugriffe; vollständiges int64/sint64
  Wire-Decoding. Ein Value-Typ; wiederholte singuläre Felder: letztes gewinnt.
- Feature-Felder vollständig prüfen; uint32-Wörter in packed/unpacked/segmentierter
  Form, Scratch-Kapazitäten pro Ebene wiederverwenden.
- Geometriekommandos nach MVT 2.1: Counts, Folgen, Nullsegmente, int32-Cursorgrenzen.
  ClosePath erhält Cursor; Punkt/Multipart und Ring-Winding analytisch geprüft.
- Parser ersetzt seinen Zustand erst nach erfolgreichem Kandidatendecode; Fehler
  erhalten Daten und Views. Äußere Feldrahmen bis Ende lesen, gewählte Ebene eindeutig.

## Referenzen
Lokale Klone: ../vector-tile-spec (21ff2cb), 2.1/README.md und vector_tile.proto;
../protobuf-docs (4b88f52), content/programming-guides/encoding.md.
MVT-Spezifikation ist Formatvertrag; native Speichergrenzen zusätzlich ausdrücklich prüfen.

## Ebenen und Wörterbücher
Ebenenheader vor Nutzdaten auslesen: Name, Version und Extent vorhanden; Version 2
unterstützen, andere Versionen für die gewählte Ebene ablehnen. Extent positiv und
im nativen int-Raum, vor jeder Verengung prüfen. Headerprüfung aus Decode extrahieren.
Tag-Paare vollständig, Schlüssel-/Wertindizes gültig; Feature-Tagmenge und kumulative
Tagablage vor uint32-Verengung prüfen. Keine still verlorenen Metadaten.
Unabhängige Wirefixtures für fehlende/überlaufende Header, unbekannte Version,
ungerade Tags und Indexgrenzen; spätere Wörterbücher und gepufferte Koordinaten gültig.
Das Geometriefixture ohne Extent ist nach §4.1 falsch spezifiziert und erhält diesen
Pflichtwert; seine bisherigen Geometrieassertionen bleiben erhalten. Negativkontrolle,
normal/sanitisiert, Lint und Wien-Pixelvergleich mit visueller Prüfung erforderlich.

## Offene Abnahme
- [ ] Alle Bytezugriffe, Feldnummern, Längen, Index- und Mengengrenzen geprüft.
- [ ] Versions-/Extent-/Tagverträge; vollständige Ringtopologie, Löcher und Multipart.
- [ ] Fehlerhafte Kachel ersetzt keinen gültigen nativen Zustand; fehlend/beschädigt
  ausdrücklich unterscheidbar. OsmField::Accept publiziert und settled noch zu früh.
- [ ] Native Datentypen erhalten Integerwerte oberhalb 2^53, nicht nur Double.
- [ ] Byte-/Decodebudgets, Abbruch und inkrementelles Decode. Kandidat erhöht Spitzenbedarf.
- [ ] Etablierten Protobuf/MVT-Reader für vollständigen Ersatz bewerten (No-Exceptions,
  Ressourcenbudgets); keine zweite unbewiesene Universal-Protobuf-Implementierung.
- [ ] Durchgängige unabhängige Formatfixtures und direkt instrumentierter Decoder.

## Aktueller Nachweis
Zehn MVT-Läufe normal/sanitisiert grün. Header-/Tag-Negativkontrolle alt in beiden
Varianten rot ohne Buildfehler. Alte Fassungen scheitern ohne Buildfehler;
Fixed32-Abschneidepuffer verursachen unter ASan einen heap-buffer-overflow, geometrische
Überläufe einen Sanitizer-Abbruch. Parser-Rollback scheitert alt normal/sanitisiert.
Lint: 184 tidy, 330 Dokumentationsdiagnosen, 32 Repository-Tests grün; drei rote Gruppen.
Headertrennung reduziert Decode-Komplexität auf 58 (Grenze 25); weitere Zerlegung erforderlich.
Wien visuell geöffnet und 0/921600 Pixelabweichung. Frühere Einzelbelege in Git.
