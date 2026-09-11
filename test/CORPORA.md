# Testkorpora

Referenzen prüfen konkrete Engine-Verträge. Ein vorhandenes Manifest beweist weder
Ausführung noch Korrektheit. Status unten beschreibt die Anbindung, keinen grünen Lauf.

| Verwendet | Warum | Wie |
|---|---|---|
| Khronos glTF-Sample-Assets / Asset-Generator | Import, Materialien, Geometrie und Animation | Deklarative Fälle unter `test/khronos/`; Rendervergleich über outshine-client. Cycles ausschließlich auf GPU erzeugt separat gepinnte Referenzbilder. Flache Emissionsreferenzen prüfen keine PBR-Beleuchtung. Ausbau: WI 2195 / 2218. |
| Khronos glTF-Validator | Fehlerhafte Assets und Importdiagnosen | Vendor-Eingaben und erwartete Reports über `test/harness/khronos/validator/`. Nur die tatsächlich implementierten Importverträge beanspruchen. |
| WPT CSS | Unterstützte Layoutregeln | Manifeste unter `test/wpt/css/`, numerischer Scorer unter `test/harness/wpt/css/`. Keine Behauptung vollständiger Browserkonformität. |
| test262 | Unterstützte Skriptsemantik | Manifeste unter `test/test262/`, Scorer unter `test/harness/test262/js/`. ECMAScript-Teilmenge ausdrücklich begrenzen. |
| GeographicLib GeodTest | Geodätische Distanz und Richtung gegen präzisere Referenzwerte | Gepinnte Tabellen über `test/harness/geographiclib/geodesic/`; prüfen `GeodesicOn`. Das beweist weder ECEF/ENU noch Web-Mercator. |

Vorbereitet, aber noch kein belastbarer Engine-Nachweis:

| Referenz | Zweck und nächster Schritt |
|---|---|
| Clear-sky / Egbert und ASTM G173 | Messdaten für Atmosphärenbeleuchtung. Alter Scorer ist schemawidrig und prüft keine Engine. Vergleich mit passenden Messgrößen und Randbedingungen aufbauen: WI 2167. |
| OpenDRIVE / A9 | Straßenprofile und Verkehrsnetze. Vorhandenen Stationsscorer fachlich prüfen und an native Netze/Geometrie anbinden; keine Rekonstruktion der echten Straße verlangen: WI 2218 / 2175. |

`make corpus-prepare MANIFEST=...` bereitet deklarierte Daten vor.
`make corpus-reference CASES=...` erzeugt Referenzpins ausdrücklich neu;
`make corpus-render CASES=...` vergleicht Bilder. Numerische Suiten laufen über
`make test` beziehungsweise `make suite SUITE=...`. Das alte `test/gate.sh` ist nicht
`make test`; seine Konsolidierung steht in WI 2218.

Pins, Herkunft, Lizenz und erwartete Werte gehören in die Fallmanifeste; große Daten
und Bilder in den Cache. Normale Tests verändern keine Referenzen. Fehlende Eingaben,
übersprungene Fälle und Toolfehler sind kein Erfolg. Referenzcode lokal neben diesem Checkout unter `/Users/cosmo/Git/`
recherchieren; keine Websuche.

Analytische Lösungen, hochpräzise Rechnungen und unabhängige Implementierungen sind
zulässige Orakel. Eingabesammlungen allein prüfen Robustheit. Bildmetriken messen
Abweichung, keine Schönheit; PNGs visuell beurteilen. Neue Referenzen und Prioritäten
stehen in WI 2218, nicht in einem zweiten Backlog hier. Lizenzen je Quelle und
Verwendung prüfen; keine pauschalen Ausschlüsse aus dem alten Survey übernehmen.
