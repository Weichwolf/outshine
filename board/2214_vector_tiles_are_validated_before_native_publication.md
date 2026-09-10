Type: defect
State: active
Parent: 2188
Area: import, world
Tags: correctness, memory, format, bounded

# Vector tiles are validated before native publication

## Befund
OsmVector::Parse (Komplexität 176) liest Float/Double ohne Restlängenprüfung.
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

## Aktiver Schritt: Value-Binärvertrag
Bestehenden Reader unmittelbar absichern: begrenztes Fixed32/64-Lesen in Little Endian,
Varint auf 64 Bit begrenzen; Value-Decoder extrahieren. int64 korrekt als Zweierkomplement,
sint64 mit 64-Bit-ZigZag. Genau ein typisiertes Value-Feld nach Schema. Fehler an Parse
weitergeben; kein Value aus abgeschnittenen Bytes. Numerische Speicherung bleibt vorerst
Double und ist oberhalb ihrer exakten Integer-Präzision noch verlustbehaftet.
Analytische Bytes für alle sieben Typen, sämtliche Float-/Double-Abschneidepositionen,
negative und breite Integer, Varint-Überlauf, mehrere/fehlende Value-Typen testen.
Alte Implementierung muss scheitern; zulässige Kacheln/Wien unverändert, PNG öffnen.
Dies beweist weder sichere Geometriekommandos noch transaktionale Gesamtkachelannahme.

## Referenzen
- https://protobuf.dev/programming-guides/encoding/
- https://github.com/mapbox/vector-tile-spec/blob/master/2.1/vector_tile.proto

## Offene Abnahme
- [ ] Alle Bytezugriffe, Varints, Feldnummern, Längen, Index- und Mengengrenzen geprüft.
- [ ] Versions-/Extent-/Tag-/Geometrieverträge einschließlich Löchern und Multipart.
- [ ] Fehlerhafte Kachel ersetzt keinen gültigen Zustand; fehlend und beschädigt unterscheidbar.
- [ ] Native Datentypen erhalten Integerwerte; Budgets, Abbruch und inkrementelles Decode.
- [ ] Unabhängige Formatfixtures, Negativkontrollen und direkt instrumentierter Decoder.
