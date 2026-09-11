Type: debt
State: open
Area: test, import, simulation
Tags: reference, corpus, audit
Parent: 2094
Depends:

# Reference corpora will test native engine contracts

## Entscheidung
Quellaudit 2026-09-11: CORPORA.md enthielt veraltete Fallzahlen, gelöschte Pfade,
pauschale Lizenzurteile und unbelegte Ausschlüsse analytischer/programmatischer Orakel.
Kein neuer Gesamtlauf; angebunden bedeutet nicht bestanden. Kurzübersicht in test/CORPORA.md.
Bestehende Runner/Manifeste/Referenzcache nutzen, keinen parallelen Harness bauen.
Keine neue öffentliche Engine-API nur für einen Korpus; interne Mathematik direkt
prüfen, Import-/Render-Integration über öffentliche API bzw. outshine-client.

## Reihenfolge und Zuständigkeit
1. Runner konsolidieren: README nennt test/gate.sh, make test verwendet test/run.sh.
   Alten Gatepfad migrieren; sämtliche sinnvollen Prüfungen zuordnen. Clear-sky ist
   dort kaputt: scorer erwartet files statt subjects[].files und prüft keine Engine.
   Keine falsche Prüfung als grüne Referenz weiterführen; Umsetzung Atmosphäre in 2167.
2. Khronos behalten: Validator-Reports prüfen Import, Sample-Assets/Generator prüfen
   generische Inhalte. render_corpus.py benutzt teils Emissionsmaterialien: solche
   Bilder beweisen Layout/UV/Abdeckung, keine Metallic-Roughness-BRDF. Materialerhalt,
   getrennte PBR-/Geometrieorakel und Animationszeitpunkte mit GPU-Cycles in 2195/2152.
3. Parser: JSONTestSuite und geeignete W3C-XML-Fälle für vorhandene Parser anbinden
   (2151/2194). Unterstützte XML-Teilmenge und implementation-defined JSON-Fälle
   ausdrücklich festlegen. Valide XML-Datei ist nicht automatisch valides Szenario.
4. Geometrie/OSM: robuste Orientierungsprädikate mit unabhängig bekannten Vorzeichen;
   ausgewählte JTS/GEOS-Overlayfälle für tatsächlich angebotene Operationen (2214/2175).
   Native Tests erhalten kleine Gegenbeispiele, Degenerationen und analytische Invarianten.
   Keine neue Polygonfunktion nur wegen eines Vendor-Falls implementieren.
5. Straßen: OpenDRIVE-A9-Manifeste/Stationsscorer prüfen, an native Ausgaben anbinden.
   Eindeutige Zuordnung, fehlende Abschnitte, Richtung, Einheiten und Datum berücksichtigen.
   A9 misst Ähnlichkeit, keine einzig richtige OSM-Geometrie. Netze/Brücken/Tunnel mit
   synthetischen expliziten Lösungen und Referenzsolver prüfen (2133/2175).
6. Geodesie: vorhandenen GeographicLib-Scorer erhalten; ECEF/ENU/Web-Mercator separat
   analytisch bzw. gegen passende PROJ-Referenzen prüfen. TMcoords ist Transverse
   Mercator und kein Web-Mercator-Orakel. Nur benötigte Projektionen anbinden.
7. Sonne/Mond: ausgewählte SOFA-/JPL-Werte für unterstützte Ephemeris-Verträge; Zeitskalen,
   Beobachterbezug, Refraktion und Genauigkeitsbudget definieren (2167), keine vollständige
   astronomische Bibliothek für einen Spielhimmel verlangen.
8. Audio: unabhängige Impuls-/Sweep-/Sinusorakel für Gain, Filter, Resampling, Latenz;
   HRTF-Daten sind Filterinputs, kein Klangqualitätsbeweis. Pegelgleiche Hörabnahme in 2212.
   Codec-Vektoren nur für tatsächlich unterstützte Decoder, keine Codec-Neuentwicklung.
9. WPT/test262 behalten; Unicode-Grenz-/Bidi-Fälle erst an implementierte Textfunktionen
   anbinden. PRNG-Vektoren aus gepinnter Referenz für den gewählten Algorithmus (2098).

## Nicht übernehmen
Keine Großkorpora ABC/Thingi10K/Moana/Bewegungsdaten ohne konkreten Fehlernutzen.
Keine vollständige Driver-CTS, keine stundenlangen Statistiktests im schnellen Gate.
FLIP/SSIM sind optionale Metriken, keine Wahrheit; Cycles ersetzt kein Farbraum- oder
BRDF-Vertragsorakel. Referenzimplementierungen und analytische Tests sind zulässig.
Frühere Lizenzangaben gelten als ungeprüft; lokal gepinnte Quelle und tatsächliche
Datennutzungsbedingungen prüfen, Code- und Datenlizenz unterscheiden.

## Abnahme
Jeder neue Anschluss hat Input, unabhängige Erwartung, Einheiten, Toleranzherkunft,
Negativkontrolle und echten Engine-Aufruf. Fehlende Daten sind unvorbereitet/rot.
Kleine schnelle Auswahl regelmäßig, große Audits benannt; Laufzeiten messen.
Keine Engine-Sonderfälle nach Testname/Hash, keine Normalisierung fehlerhafter Ergebnisse.
Erledigte Teilaufträge hier verdichten; Implementierungen in ihren Fach-WIs abschließen.
