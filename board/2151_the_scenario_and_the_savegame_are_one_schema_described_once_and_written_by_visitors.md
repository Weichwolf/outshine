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

Szenariobeschreibung und Laufzeit-Snapshot sind getrennte Verträge mit teilbarer Serialisierung.
Save/Restore besitzt bereits einen begrenzten Trait-Pfad (WI 2210); vollständige
Snapshots brauchen explizite Abdeckung aller wiederherzustellenden Systeme. Event-
Replay erst nach nachgewiesener Deterministik einschließlich externer Eingaben.

## Nachgewiesene Lücken
Asset-Writer erhält jetzt die vom Reader unterstützten Metadaten, Animation/Clip und
Surface-Selektoren/Materialparameter. Weitere native Materialfelder sind kein XML-Vertrag.
Identität, Render- und Lichtfelder werden jetzt vollständig gemäß Reader erhalten;
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


Asset-Abnahme: vier Animationsmodi mit unabhängigen Feldwerten vor/nach Write/Read
grün; Altwriter scheitert ohne Buildfehler. Parser-Erhaltung und Zeichenreferenzen grün.
Szenario bleibt eigenständiges Importformat; keine glTF-Erweiterung für Welt-/Spielregeln.

## Identität, Render und Beleuchtung
Identität (name/version/active/epoch/decay), RenderPlan und Lighting vollständig
entsprechend dem Reader serialisiert, jeweils in eigener Writer-Phase;
keine Änderungen an ungeprüften Reader-Tokens oder Runtime-Fallbacks in diesem Schritt.
Render: Frame/Fps/Fill/Audits/Orbit/Transfer/Exposure/Precision, geordnete Outputs/Stages.
Lighting: Key, IndirectLight und ShadowRadiusM. Native Picture ist bislang kein XML-Feld.
Output/keep sind Reader-Aliasse; Writer verwendet kanonisch output. Keine doppelte Ausgabe.
Unabhängige XML-Fixtures mit nicht-default Werten und Zeichenreferenzen vor und nach
Write/Read prüfen, inklusive leeren Listen und nicht deklarierten Render-/Lichtsektionen.
Altwriter scheitert an verlorenen Werten ohne Buildfehler; neuer Writer besteht
die Feld-Oracles und den Asset-Roundtrip. Kein bloßer Text-Fixpunkt als Oracle.

## Geprüfte Grenzen der Inventur
Grammar/Writer ist eine statische Literal-Inventur, kein Beweis für Attribute oder
semantische Erhaltung. Tokenbasierter Scanner prüft Kommentare, verkettete/Raw-Tags
und kaputte Eingaben; elf Tests grün, Altprüfer verletzt sechs Negativkontrollen.

Asset-Roundtrip, strikter Clip-Token und gemeinsamer Playback-Validator sind implementiert;
negative Kontrollen und API-Erhaltung/Retry grün. Welt-/Relief-/OSM-Writer separat;
OSM-Koordinaten, Physik, Generatorparameter und Assets durch Roundtrip-Fixtures geprüft.

API-Audit: Identity, Patch, RenderPlan und Lighting dokumentieren Besitz, Einheiten,
Default-/Auswahlverhalten und aktuelle Grenzen anhand der Consumer. Epoch/Decay sind
nur Metadaten ohne Runtime-Semantik. Fps beeinflusst den bisherigen Animationspfad;
Orbit ist updateabhängig. Gemeinsame finite Bereichsvalidierung bleibt offen, ebenso
zeitbasierte Animation/Orbit statt Framekopplung. Dokumentation ist keine Abnahme dieser Lücken.

## Native View-Verträge
ViewBook prüft jetzt den geschlossenen CameraPlacement-Modus und einen endlichen
positiven TimeScale-Faktor vor Veröffentlichung. Der Altstand nahm drei ungültige
Modi und positive Unendlichkeit an. Public-API-Negativfälle erhalten Deklaration/
Katalog; alle drei Modi und ein positiver Bruchfaktor bestehen, fünf Regressionen grün.
View-Felder sind nach Verwendung dokumentiert: In, Viewport, PitchLimitDeg und TimeScale
haben derzeit keine Kamerawirkung; Person validiert nur das Label, DistanceM steuert
den Verfolgungsabstand. Keine implementierte Szenenwahl, Viewports oder Zeitdilatation
behaupten. Diese ungenutzten Konfigurationen fachlich implementieren oder mit
expliziter Importdiagnose aus der minimalen API entfernen; nicht still verwerfen.

## Layer-Vertrag
ReadScenario validiert vor MergeLayer; keine zweite Dokumentkopie nötig. Auswahl,
Override-Reihenfolge, Nested-Ablehnung und Fehlererhaltung geprüft; öffentliche Layer
dokumentiert. Test grün, entfernte Nested-Prüfung rot.
Offen: native Windows-Pfadauflösung statt Slash-Erkennung; übrige Merge-Semantik auditieren.

## Grundlegende Weltparameter
Georeferenz, Gravitation, Luftdichte, Streamingreichweite/-geduld gemeinsam auditieren,
dokumentieren und verlustfrei exportieren. radiusM ist Generator-Extent, kein verwendeter
Erdradius; Luftdichte schaltet bisher nur den Himmel. Nullgravitation wählt bislang
Standardgravitation: bekannte Semantiklücke, nicht als Schwerelosigkeit dokumentieren.
Gemeinsame finite Werteprüfung für Import/API/Export; Latitude [-90,90], Longitude
endlich, übrige Größen nichtnegativ. Patience über geprüfte Millisekunden-Pollzahl
begrenzen und auch GroundPoolConfig vor int-Cast prüfen; keine Doppelkonstante.
Altcode verletzt API-/Roundtrip-Kontrollen; elf Tests einschließlich Fehlererhaltung,
Defaults, Layern und exakten Pollbudget-Grenzen bestehen.
