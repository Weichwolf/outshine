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
OsmField::Accept publizierte vor vollständiger Prüfung; Größen
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
  ausdrücklich unterscheidbar. Decode-Fehler vor Veröffentlichung abgefangen; native
  Mengen-/Projektions-/Allokationsfehler müssen noch vollständig abgesichert werden.
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
Decode-Komplexität 59 (Grenze 25); weitere Zerlegung erforderlich.
Wien visuell geöffnet und 0/921600 Pixelabweichung. Frühere Einzelbelege in Git.

## Ergebnisvertrag
Parse nimmt Span/StringView und liefert expected<void, ParseError>; kein bool plus
verwechselbarer Present-Ausgabe. MissingLayer nur nach gültigem äußeren Framing,
InvalidTile bei beschädigten Bytes, UnsupportedVersion bei vorhandener unbekannter
Version. Fehlender Versionswert bleibt InvalidTile. Kandidatenpublikation unverändert.
OsmField verwendet diese Fehler direkt und übergibt die originale Spangröße ohne
size_t→int→size_t-Verengung. Native Kacheltransaktion bleibt der anschließende Schritt.
Tests unterscheiden leere/fehlende Ebenen, frühe/späte Schäden und unbekannte Version;
Mutant InvalidTile→MissingLayer muss scheitern. Bestehende Ablehnungsassertionen bleiben.
Ergebnisvertrag geprüft: zehn MVT-Läufe normal/sanitisiert grün. Mutant, der alle
Fehler als MissingLayer meldet, scheitert in beiden Varianten ohne Buildfehler.
Lint unverändert 184/330, 32 Repository-Tests grün; drei rote Gruppen. Keine neuen
Bildregeln; frühere Wien-Abnahme bleibt Regression des vorherigen Header-Schritts.

## Native Annahme
Alle angeforderten Ebenen zunächst als besitzende Formatprodukte dekodieren. Eine
beschädigte spätere Ebene verwirft die Kachel, bevor native Pools oder Settled mutieren.
Fehlende Ebenen sind zulässig; Diagnosezähler getrennt von veröffentlichten Weltdaten.
Native Ebenenübernahme als eigene Zuständigkeit extrahieren; Accept/AddTile liefern
expected und Build propagiert Fehler, statt fehlerhafte Kacheln als erledigt zu markieren.
Bereits vorher erfolgreich angenommene Kacheln bleiben erhalten; kein Gesamt-Build-Rollback.
Direkte Accept-Fixtures mit gültiger erster/defekter zweiter Ebene, Wiederholung nach
Korrektur, fehlenden Ebenen und bestehenden Daten müssen alt scheitern und neu bestehen.
Kumulative Native-Indexgrenzen, Projektionsgrenzen und Allokationsbudgets bleiben separat offen.
Native Annahme geprüft: zwölf MVT-/OSM-Läufe grün; Teilveröffentlichungsfixture alt
rot ohne Buildfehler. Nach Lint-Korrektur beide OSM-Fälle erneut grün. Lint 183 statt
184 tidy (Accept-Komplexitätswarnung entfällt), 330 Dokumentationsdiagnosen, 32
Repository-Tests grün; drei rote Gruppen. Wien-PNG geöffnet, 0/921600 Pixelabweichung.
