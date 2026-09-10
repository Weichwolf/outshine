Type: debt
State: active
Parent: 2188
Area: scenario, import, engine
Tags: architecture, validation, persistence
Depends:

# Scenario imports preserve declarations through the common native API

## Vertrag
Szenarien deklarieren statische Inhalte und Konfiguration, die auch Code über die
öffentliche API aufbauen kann. glTF ist ein weiterer Importadapter mit geringerem
Ausdrucksumfang, kein Runtime-Schema. Formatprüfung und Konvertierung im Adapter;
fachliche Validierung nativer Daten gemeinsam für beide Importer und direkte Aufrufer.
Vor Veröffentlichung vollständig vorbereiten und validieren; Fehler erhalten die
aktive Welt. Integration/Client-Abnahme in WI 2195, Zustandsübergänge in WI 2191.

Szenariobeschreibung und Laufzeit-Snapshot sind getrennte fachliche Verträge. Gemeinsame
Serialisierungsbausteine sind möglich, ein einziges Schema ist keine Voraussetzung.
Save/Restore besitzt bereits einen begrenzten Trait-Pfad (WI 2210); vollständige
Snapshots brauchen explizite Abdeckung aller wiederherzustellenden Systeme. Event-
Replay erst nach nachgewiesener Deterministik einschließlich externer Eingaben.
Keine unbelegte plattformübergreifende oder bildweise Bitidentität versprechen.

## Nachgewiesene Lücken
ScenarioWrite.cpp schreibt bei Assets nur Uri/Kind. ReadAssets liest zusätzlich
Digest, Variant, Animation, Clip und Surfaces einschließlich Materialparametern.
WriteScenario verliert außerdem Szenarionamen und zahlreiche weitere Sektionen.
Der Grammar/Writer-Guard erkennt Elementnamen, keine verlorenen Attribute. Zweimaliges
Write/Read kann auf einem bereits reduzierten Dokument stabil sein und ist kein Beweis.
Xml::Ref::Num/Int akzeptieren Zahlenpräfixe; ungültige Werte fallen auf Defaults zurück.
ReadAssets verengt Clip von double auf int ohne expliziten Wertebereichsvertrag.
Vorhanden: typisierte Scenario::Document-Daten, Reader, Writer, öffentliche Engine-
Einstiege, XML-Zeichenreferenztests und transaktionaler Szenario-Parser.

## Umsetzung
1. Asset-Deklarationen vollständig erhalten: Metadaten, vier Animationsmodi, Clip,
   Materialüberschreibungen und Selektoren. Aus unabhängigen XML-Fixtures erwartete
   typisierte Werte prüfen, anschließend nach Write/Read erneut dieselben Werte.
2. Unterstützte statische Felder/Sektionen inventarisieren und vollständig migrieren.
   Gemeinsame Feld-/Enum-Beschreibungen dort einsetzen, wo sie Regeln wirklich teilen;
   kein Reflection-Gerüst mit paralleler unkontrollierter Feldliste als Vollständigkeitsbeweis.
3. Syntax, typisierte Beschreibung, fachliche Validierung und Veröffentlichung trennen.
   Runtime-Datentypen ohne Formatkonventionen; Adapter schreiben keine Interna direkt.
4. Vorhandene ungültige Attribute ablehnen: vollständige Tokens, Bereich, Endlichkeit,
   Enums und Booleans prüfen; Diagnose mit Feld/Position. Defaults nur bei Abwesenheit.
   Fehlertransport mit WI 2194 abstimmen, keine stillen Ersatzwerte.
5. Versionierung und stabile Datenreferenzen definieren. Keine Laufzeithandles persistieren.
   Binärformat erst bei belegtem Bedarf; XML bleibt der vorhandene unterstützte Eingang.

## Abnahme
- [ ] Asset-Fixtures behalten alle unterstützten Felder; Altwriter verletzt das Oracle.
- [ ] Jede unterstützte statische Sektion besitzt unabhängige Erhaltungsfälle.
- [ ] Alle unterstützten Deklarationen roundtrip-fähig; Grammar/Writer-Guard grün.
- [ ] Code, Szenario und glTF nutzen dieselbe native Validierung; äquivalente Inhalte
      und identische native Fehler über alle drei Pfade geprüft (WI 2195).
- [ ] Späte Validierungs-/Aufbaufehler erhalten aktive Welt (WI 2191).
- [ ] Restzeichen, Überlauf, NaN/Inf und ungültige Boolean-/Enum-Tokens werden abgelehnt;
      valide Randwerte erhalten. Kein Test lockert fachliche Grenzen.
- [ ] Lint/clang-tidy und passende Regressionen; PNG-Prüfung bei Bildänderung.

Historische Fremdengine-/Determinismusbehauptungen sind keine Abnahmegrundlage.
Der konkrete lokale Reader/Writer-Datenverlust begründet diesen Auftrag unabhängig davon.
