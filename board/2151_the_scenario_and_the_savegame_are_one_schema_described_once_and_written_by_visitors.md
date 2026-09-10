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
Asset-Writer erhält jetzt die vom Reader unterstützten Metadaten, Animation/Clip und
Surface-Selektoren/Materialparameter. Weitere native Materialfelder sind kein XML-Vertrag.
WriteScenario verliert außerdem Szenarionamen und zahlreiche weitere Sektionen.
Der Grammar/Writer-Guard erkennt Elementnamen, keine verlorenen Attribute. Zweimaliges
Write/Read kann auf einem bereits reduzierten Dokument stabil sein und ist kein Beweis.
Xml::Ref::Num/Int akzeptieren Zahlenpräfixe; ungültige Werte fallen auf Defaults zurück.
ReadAssets prüft Clip jetzt vor double/int-Konvertierung auf vollständigen Integerwert
in [0, INT_MAX]; andere numerische Attribute bleiben zu auditieren.
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
- [x] Asset-Fixtures behalten alle unterstützten Felder; Altwriter verletzt das Oracle.
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

Asset-Abnahme: vier Animationsmodi mit unabhängigen Feldwerten vor/nach Write/Read
grün; Altwriter scheitert ohne Buildfehler. Parser-Erhaltung und Zeichenreferenzen grün.
Lint: 181 tidy, 282 Dokumentationsdiagnosen, 32 Repository-Tests grün; drei rote Gruppen.
Szenario bleibt eigenständiges Importformat; keine glTF-Erweiterung für Welt-/Spielregeln.

Nächster Grenzfix: Asset-Clip als vollständige endliche Dezimalzahl mit ganzzahligem
Wert in [0, INT_MAX] prüfen, erst danach verengen. Fehlendes Attribut bedeutet 0;
leeres/ungültiges Attribut ist Fehler. IntegralDecimal wie beim Catch-up-Limit nutzen,
um gerundete Bruchteile nicht als Integer anzunehmen. Negative/Überlauf/Restzeichen/
NaN/Inf/Bruchteile und gültige Dezimal-/Exponentformen prüfen; Fehler erhält Dokument.

Clip-Abnahme: 28 Checks sowie Asset-Roundtrip und Parser-Erhaltung grün; Altcode
scheitert ohne Buildfehler. Lint 181 tidy/282 Doxygen, 32 Repository-Tests grün, drei
rote Gruppen. Vorhandener Clip und direkte API-Validierung bleiben eigene Verträge.

Gemeinsame Asset-Validierung: Clip >= 0 und definierter Animationsmodus für XML und
direktes Engine::declare, ohne IO/Mutation. Negativer Clip/ungültiges Enum müssen auch
am direkten Einstieg scheitern und die vorherige Deklaration erhalten. XML-Syntax und
Indexexistenz im geladenen Asset bleiben separate Prüfungen. Kein vollständiger
Weltvalidator behauptet; zunächst gemeinsame fachliche Grenze für Playback-Metadaten.
