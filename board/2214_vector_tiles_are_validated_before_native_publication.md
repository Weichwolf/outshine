Type: defect
State: active
Parent: 2188
Area: import, world
Tags: correctness, memory, format, bounded

# Vector tiles are validated before native publication

## Ziel
Formatdaten vollständig validieren, begrenzt dekodieren und erst danach als besitzende
native Kachelprodukte veröffentlichen. Fehler erhalten bereits veröffentlichte Daten.
Importer liefern native Geometrie; Reader und Eingabepuffer bleiben an der Formatgrenze.

## Implementierte Verträge
- Begrenzte Little-Endian-Fixed32/64- und Varint-Lesezugriffe; korrektes int64/sint64
  Wire-Decoding. Ein Value-Typ; wiederholte singuläre Felder: letztes gewinnt.
- Feature-Felder vollständig lesen; uint32-Wörter packed/unpacked/segmentiert,
  Scratch-Kapazitäten pro Ebene wiederverwenden.
- MVT-2-Geometriekommandos prüfen: Counts, Folgen, Nullsegmente, int32-Cursorgrenzen.
  ClosePath erhält Cursor; Punkt/Multipart und Ring-Winding analytisch geprüft.
- Name, Version und Extent vorhanden; Version 2 unterstützt. Extent positiv und im
  nativen int-Raum; Tag-Paare und Wörterbuchindizes gültig, Tagmenge uint32-begrenzt.
- Parse mit Span/StringView und expected<void, ParseError>. MissingLayer erst nach
  gültigem äußerem Framing; InvalidTile und UnsupportedVersion getrennt. Parserzustand
  erst nach erfolgreichem Kandidatendecode ersetzen; Fehler erhalten Daten und Views.
- Alle angeforderten Ebenen vor nativer Übernahme dekodieren. Beschädigte spätere Ebene
  veröffentlicht weder frühere Ebenen noch Kacheleintrag/Settled. Fehlende Ebenen zulässig.
  Diagnosezähler getrennt von Weltdaten; Accept/AddTile/Build propagieren Fehler.
  Bereits erfolgreich angenommene andere Kacheln bleiben; kein Gesamt-Build-Rollback.
- Accept prüft Zoom 0..31 und x/y im Raster vor Decode und Mutation.
- Inverse Mercator-Breite ohne sinh-Überlauf bei großen endlichen Pufferkoordinaten.
  Ab |a| >= log(4/epsilon) ist 2*atan(exp(-|a|)) <= epsilon/2; Ergebnis rundet auf
  den Pol. Normale Koordinaten werden unverändert berechnet. Keine MVT-Pufferbeschneidung.

## Offene Abnahme
- [ ] Gemeinsame Kapazitätsverträge für importierte und deklarierte Daten. MVT-Accept
  prüft kumulative Indexräume; deklarierte Pfade und Container-max_size bleiben offen.
- [ ] Explizite Byte-/Decode-/Allokationsbudgets, Abbruch und inkrementelles Decode.
  Kandidaten erhöhen den Spitzenbedarf; Allokationsfehlervertrag mit WI 2194 abstimmen.
- [ ] Vollständige Ringtopologie: Selbstschnitt, Selbstberührung, Lochzuordnung/-schnitt.
- [ ] Alle Formatpflichtfelder, Feldnummern und sonstigen Versions-/Layerverträge;
  vollständige Kachelvalidierung gegenüber nur angeforderten Ebenen präzisieren.
- [ ] Native Zahlentypen erhalten Integer oberhalb 2^53, statt Double als Universalspeicher.
- [ ] Expliziter nativer Vertrag für ungewickelte Längengrade/Datumsgrenzen-Geometrie.
- [ ] Wiederannahme, Revision und Eviction mit WI 2104; Generation/Invalidierung prüfen.
- [ ] Etablierten Protobuf/MVT-Reader für vollständigen Ersatz bewerten: No-Exceptions,
  Ressourcenbudgets; keine zweite unbewiesene Universal-Protobuf-Implementierung.
- [ ] Durchgängige unabhängige Fixtures und direkte Instrumentierung aller Decoderpfade.

## Referenzen
Lokale Klone: ../vector-tile-spec (21ff2cb), 2.1/README.md und vector_tile.proto;
../protobuf-docs (4b88f52), content/programming-guides/encoding.md.
MVT-Spezifikation ist Formatvertrag; native Speichergrenzen zusätzlich ausdrücklich prüfen.

## Nachweis
14 ausgewählte MVT-/OSM-Prüfungen grün, MVT einschließlich ASan/UBSan. Alte Fixed32-Fassung verursacht heap-buffer-overflow;
alte Geometriefassung scheitert mit Sanitizer-Abbruch. Falsche MissingLayer-Klassifikation
wird durch Mutation erkannt. Native Teilveröffentlichung scheitert alt ohne Buildfehler.
OSM-Positions-/Annahmeprüfungen grün; ungültige Rasteradressen erhalten Daten/Settled.
Große finite Mercator-Ordinaten, Äquator und Kachelrand analytisch geprüft, einschließlich
FE_INVALID/FE_OVERFLOW. Alte Projektionsfassung scheitert; nach Integervergleichskorrektur
betroffenen Test erneut bestanden. Normale OSM-Integration nicht als ASan-Nachweis ausgeben.
Abschluss-Lint: 183 tidy, 330 Dokumentationsdiagnosen, 32 Repository-Tests grün;
drei rote Gruppen. Decode-Komplexität 59 (Grenze 25); Accept-Komplexitätswarnung behoben.
Wien-PNG geöffnet, 0/921600 Pixelabweichung. Einzelbelege und frühere Schritte in Git.

## Kumulative Indexkapazität
Vor Annahme den bestehenden Poolbestand mit allen geplanten Ebenenzuwächsen prüfen.
Features/Tiles höchstens INT_MAX wegen Rückgaben/TileIndex, übrige Indexräume UINT32_MAX;
Punktpaare zusätzlich durch SIZE_MAX/2 begrenzt. Addition als Restkapazitätsprüfung,
kein Überlauf vor Vergleich. Werte/Keys/Strings konservativ höchstens ein Eintrag pro
Tag-Paar; unbekannte Interning-Treffer nicht vorwegnehmen. Keine Mutation bei Ablehnung.
Kapazitätsarithmetik separat mit analytischen Grenzfällen ohne Milliardenallokationen
prüfen; Negativmutation muss scheitern. Native Annahme-/Positionsregression und Lint.
Das ist ein Indexvertrag, kein RAM-/Container-max_size-/Decodezeitbudget.
Kapazitätsabnahme: analytische Grenzfälle normal/sanitisiert grün; Mutation erlaubt
je einen überzähligen Eintrag und scheitert in beiden Varianten ohne Buildfehler.
Native Annahme-/Positionsregression grün, Wien visuell geprüft und pixelgleich.
Abschluss-Lint unverändert 183/330, 32 Repository-Tests grün, drei rote Gruppen.

## Decoder-Zuständigkeiten
Decode sucht/validiert den Ebenenheader und das äußere Framing. ReadLayerTables baut
besitzende Wörterbücher und liefert geliehene Feature-Byte-Spans; DecodeFeatures prüft
und übernimmt diese danach mit wiederverwendetem Scratch. So bleiben Feldreihenfolge,
Input-Lebensdauer und native Publikation ausdrücklich getrennt. Keine neue Speicher-
oder Fehlersemantik. Bestehende unabhängige MVT-Fixtures und Negativmutation prüfen,
dass ein Fehler der Feature-Phase nicht als Erfolg zurückkehrt; Lint muss die verbleibende
Decode-Komplexitätswarnung ohne Unterdrückung beseitigen.

## Vorläufige Ressourcenbudgets (Schätzung, nicht gemessen)
OSM-resident 256 MiB; gemeinsamer transienter Pool 64 MiB für Rohdaten, Decode-Scratch
und native Kandidaten aller Jobs. Summe 320/8192 MiB = 3,90625 % des 8-GiB-Ziels;
kein Gesamtbudget der Engine. Rohkachel maximal 8 MiB, höchstens zwei aktive Decode-Jobs;
deren Einzelmaxima dürfen den gemeinsamen 64-MiB-Pool nicht überbuchen.
Main-Thread-Publikation 0,5 ms/Frame = 3 % von 1000/60 ms; Worker-Abschnitte 2 ms,
danach Abbruch-/Yield-Punkt. Atomare Publikation darf nicht monolithisches Decode bedeuten.
Noch nicht enforced. Später konfigurierbar machen und durch Budget-/Überlasttests sowie
Benchmarks auf Zielhardware absichern: verschieben, freigeben, Detail reduzieren oder
expliziter Fehler; keine unbemerkte Teilveröffentlichung. Mit Messdaten kalibrieren.
