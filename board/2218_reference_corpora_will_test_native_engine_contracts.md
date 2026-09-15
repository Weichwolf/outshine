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

## Widerlegte Belichtungsannahme im Sonnen-Audit
ScoreWhichWaysTheSunMovesTheGround verlangt für 5° nach 75° ein anderes Bild und
rechtfertigt das mit zeitlicher Belichtungsanpassung. Messung über Engine::inspect:
ExposureApplied bleibt bei 5/5/30/75/5° exakt 5,20833346e-5. Herleitung im Live-Pfad:
2,5 / (1,2 × 40000 Lux) = 5,20833333e-5, Rundung auf Float erklärt die Differenz.
Explizite halbe/doppelte Belichtung kommt korrekt an. Der Verlust von 37,022 auf
1,140 mittlere Bodenhelligkeit ist damit keine Belichtungsanpassung. Temporäre
Messinstrumentierung entfernt; unveränderte Assertions weiterhin reproduzierbar.
Ungleichheits-Assertion durch exakte Pixelgleichheit ersetzt; auch die direkte
Wiederholung vergleicht sämtliche Pixel statt nur die mittlere Bodenhelligkeit.
Vor Rendereränderung Schatten, atmosphärische LUTs, temporale Historie und Geometrie
bei A/B/A isolieren. Unabhängigen kleinen Fall über outshine-client aufbauen und
PNGs vergleichen; Hang-/Schattenszenen beweisen kein allgemeines sin(elevation).

Zusätzliche Gegenprobe: 32 statt mindestens 2 Renderframes liefert dieselben
Bodenhelligkeiten 37,022/35,774/71,616/1,140. Längeres Render-Settling allein behebt
A/B/A nicht. Temporäre Änderung entfernt; Terrain-Invalidierung weiter in 2105.

## Air-Aufnahmephase und implizite Draws
Air bleibt nach Gebäuderevisionsfix rot. Zwei explizite render-Aufrufe statt einem
lassen alle vier Assertions bestehen. Achtung: readPixels rendert intern erneut
(Framing.cpp), ebenso saveScreenshot; damit wurden tatsächlich drei statt zwei
Frames erzeugt. Die bisherigen Framebezeichnungen waren unvollständig.
Belichtung, GPU-Irradianz, Sonnentransmission, Bodenlicht und Schatten-Extrema sind
in vier inspizierten Aufnahmen gleich. Screenshot-Versuch liefert gleiche PNGs,
fügt aber selbst Frames hinzu und isoliert daher die betroffenen Ausgaben nicht.
Temporäre Instrumentierung entfernt. Folgemessung muss die bereits ausgelesenen
RGBA-Daten speichern, ohne Screenshot-Aufruf oder weitere Draws.
Readback-API auditieren: explizites Rendern und Kopieren eines fertigen Frames
trennen; dokumentierte bisherige Implizit-Draw-Aufrufer vollständig migrieren (2195).
Keine pauschale Erhöhung von settleFrames als Reparatur. Der Frame muss seinen
Vertrag erfüllen oder eine spezifizierte temporale Vorgeschichte benötigen.

Direkter Dump bereits ausgelesener RGBA-Daten ohne weitere Draws: 150/57600 Pixel
verschieden, maximal 6/255 je Kanal, Bounding-Box x=1..316/y=62..177 bei 320×180.
Dumps im System-Temp outshine-air-raw-{0,1}.rgba; Instrumentierung zurückgenommen.
Die Abweichung ist damit belegt, aber weder allgemeine Belichtungsänderung noch
lokaler einzelner Objektfehler bewiesen. Implizite Draws erschweren gleiche-Frame-
Farb-/Tiefenvergleiche; Readback-Vertrag zuerst unter 2195 konsolidieren.

Readbacks desselben Frames nach API-Korrektur: Tiefe 0/57600 Unterschiede;
ShadingNormal und SurfaceIdentity je 0/230400. SceneLinear: 916/230400 Kanäle,
maximal 198 HDR-Einheiten. RGBA: 256 Kanäle, maximal 6/255. Ausgaben als zusätzliche
Planattachments angefordert; temporäre Instrumentierung entfernt. Weitere Isolation:
Schattenatlas texelweise statt nur Extrema, dann Beleuchtung/Filterauswertung.
