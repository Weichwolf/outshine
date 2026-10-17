# Testkorpora

Referenzen prüfen konkrete Engine-Verträge. Ein vorhandenes Manifest beweist weder
Ausführung noch Korrektheit. Status unten beschreibt die Anbindung, keinen grünen Lauf.

| Verwendet | Warum | Wie |
|---|---|---|
| WPT CSS | Unterstützte Layoutregeln | Manifeste unter `test/wpt/css/`, numerischer Scorer unter `test/harness/wpt/css/`. Keine Behauptung vollständiger Browserkonformität. |
| test262 | Unterstützte Skriptsemantik | Manifeste unter `test/test262/`, Scorer unter `test/harness/test262/js/`. ECMAScript-Teilmenge ausdrücklich begrenzen. |
| GeographicLib GeodTest | Geodätische Distanz und Richtung gegen präzisere Referenzwerte | Gepinnte Tabellen über `test/harness/geographiclib/geodesic/`; prüfen `GeodesicOn`. Das beweist weder ECEF/ENU noch Web-Mercator. |

Vorbereitet, aber noch kein belastbarer Engine-Nachweis:

| Referenz | Zweck und nächster Schritt |
|---|---|
| Clear-sky / Egbert und ASTM G173 | Manifest und Erwerb von Messdaten für Atmosphärenbeleuchtung; noch kein nativer Vergleich. Messgrößen und Randbedingungen besitzt [#7](https://github.com/Weichwolf/outshine/issues/7). |
| OpenDRIVE / A9 | Manifest und Erwerb von Straßenprofilen/Verkehrsnetzen; noch kein nativer Vergleich. Anbindung an Netze/Geometrie besitzt [#11](https://github.com/Weichwolf/outshine/issues/11); keine Rekonstruktion der echten Straße verlangen. |

`make corpus-prepare MANIFEST=...` bereitet deklarierte Daten vor.
Das Pflichtgate `make test` prüft Weltverträge und Offline-Places.
`make test-all` führt alle deklarierten Suiten aus; `make suite SUITE=...` wählt
einen betroffenen Vertrag. HTML/CSS/JS-Korpora bleiben erhalten.

Pins, Herkunft, Lizenz und erwartete Werte gehören in die Fallmanifeste; große Daten
und Bilder in den Cache. Normale Tests verändern keine Referenzen. Fehlende Eingaben,
übersprungene Fälle und Toolfehler sind kein Erfolg. Referenzcode lokal neben diesem Checkout unter `/Users/cosmo/Git/`
recherchieren; keine Websuche.

Analytische Lösungen, hochpräzise Rechnungen und unabhängige Implementierungen sind
zulässige Orakel. Eingabesammlungen allein prüfen Robustheit. Bildmetriken messen
Abweichung, keine Schönheit; PNGs visuell beurteilen. Neue Referenzen und Prioritäten
stehen beim zuständigen GitHub-Issue, nicht in einem zweiten Backlog hier. Lizenzen je Quelle und
Verwendung prüfen; keine pauschalen Ausschlüsse aus dem alten Survey übernehmen.
