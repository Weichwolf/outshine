# Place-Szenarien

Jede `.scenario`-Datei ist ein normales Outshine-Szenario. Der Dateistamm ist der Name.
`outshine-client --places <Verzeichnis> places|shots|roundtrip` lädt den Katalog;
Standard ist `src/assets/places`. `run <Datei>` verwendet dieselbe öffentliche Szenario-API.
Neue Places benötigen keine C++-Änderung und keinen Rebuild. Ausgabe von `places` ist TSV:
Name, Breitengrad, Längengrad, Höhe in m, Bearing in Grad, Pitch in Grad, vertikaler FOV
in Grad, Breite/Höhe in px, UTC-Zeit. Namen: ASCII-Buchstaben, Ziffern, `_` und `-`.

Der Place-Katalog verlangt Welt, positive Rendergröße, feste Uhr und eine geodätische
Kamera mit expliziter Höhe. Das Verzeichnis darf höchstens 4096 Einträge enthalten;
je Szenario höchstens 1 MiB. Dies sind gesetzte IO-Grenzen, keine Engine-Weltgrenzen.
Fehlende, leere oder ungültige Kataloge liefern einen Fehler; kein eingebauter Ersatz.

Die neun ausgelagerten Kameras stammen unverändert aus Commit 631d20ce und seinen
Nachfolgern. Höhe und Pitch sind Schätzungen. Die ursprüngliche Höhe war als ASL
bezeichnet, wurde aber ohne Datumkonvertierung übergeben. WI 2170 muss Datum, Pose,
FOV und Bildausschnitt kalibrieren; die Migration behauptet keine neue Genauigkeit.
1280×720, haze=0 und die übrigen Szenenwerte erhalten den bisherigen Renderzustand.
Die Webcam-JPGs haben andere Seitenverhältnisse; deren Anpassung gehört ebenfalls in 2170.

Bilder dienen der Qualitätsprüfung nach WI 2169 und dessen Place-Abnahmen. Keine
Foto-Rekonstruktion und keine pro Place kodierte Geometrie. Wetter und Beleuchtung
müssen als deklarierte Eingänge ausgebaut werden (WI 2172).
