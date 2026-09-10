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

## Value-Binärvertrag
Bestehenden Reader unmittelbar absichern: begrenztes Fixed32/64-Lesen in Little Endian,
Varint auf 64 Bit begrenzen; Value-Decoder extrahieren. int64 korrekt als Zweierkomplement,
sint64 mit 64-Bit-ZigZag. Genau ein Value-Typ nach Schema, gleiche singuläre Felder: letztes gewinnt. Fehler an Parse
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

## Nachweis dieses Schritts
FieldHeader ersetzt vertauschbare Feldnummer-/Wire-Ausgaben. Value-Decodierung separat.
Alte Fassung scheitert an 22/41 analytischen Checks. MVT-Suite kompiliert OsmVector.cpp
selbst mit ASan/UBSan; exakt große Abschneidepuffer lösen im alten Decoder einen
ASan heap-buffer-overflow (Read 4) aus, ohne Buildfehler. Korrigiert normal/sanitisiert
grün. OSM-Positionsregression grün; Wien c307cab8 bytegleich, PNG geöffnet.
Abschluss: 184 tidy statt 185, 330 Dokumentationsdiagnosen, 32 Repository-Tests grün;
drei Gruppen bleiben rot. Pixelvergleich Wien: 0 geänderte Pixel, RGB-Maximum/Mittel 0.

## Aktiver Schritt: Feature-Wirevertrag
Feature-Tags und Geometriewörter werden vollständig in ein lokales Formatprodukt
geparst; fehlerhafte Varints, uint32-Überläufe und abgeschnittene Felder lehnen die
Ebene ab, statt partielle Features als Erfolg zu melden. Gemeinsamer uint32-Reader,
packed und unpacked sowie mehrere Segmente desselben repeated Felds unterstützen.
Featuretyp nur im definierten Enum 0..3. Readerfehler auf Ebene/Feature weiterreichen.
Die eigentliche Geometrieinterpretation bleibt getrennt und vorerst unverändert;
Tagreferenzen, Extent/Version, Geometrieregeln und Gesamtkachel-Rollback bleiben offen.
Tests mit expliziten Bytes: spätes defektes Feature, fehlende Payload, uint32-Überlauf,
Enumgrenzen, äquivalente packed/unpacked/segmentierte Streams. Alte Fassung muss
scheitern, neue normal und direkt mit ASan/UBSan grün; Wien-Pixelvergleich und Lint.
