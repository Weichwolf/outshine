# Südlicher Radweg und wirkliche Außenprofile (#434)

Dieselbe begrenzte Versuchsfolge verarbeitet die dritte feste
[Planungseingabe](fixtures/stair-cycle-south-vienna.json). Sie ergänzt den ganzen
südlichen Radweg `39881120:51` (534,861 m) zum
[20-Quellenstand](stair_cycle_north_comparison.md). Bisherige Achsen, Priors,
Originalgeometrien, Eigenschaften und IDs bleiben erhalten; gemeinsame Höhen
und Terrain werden neu geplant. Keine neue Formulierung oder SDK-Abhängigkeit.

## Tatsächliche Kontakte und Datenbereich

Der wirkliche alte Endknoten 3198/3435 gehört zu `39881120:28`; sein ganzer
Querschnitt ist jetzt gebunden. Der weitere gemeinsame interne Quellvertex
2973/3073 ist kein Endpunkt des neuen Quellteils. Ein ausdrücklich getesteter
falscher Endanschluss bleibt abgelehnt. Gemeinsame zusätzliche Bandflächen
bleiben Planentscheidungen. Sieben wirkliche Außenprofile bleiben offen:
`4420992360:6` am Anfang, `32651280:3`, `39881120:23` und `39881120:51` beidseitig.
Kein gemessener Straßenknoten wird aus bloßer Nähe erfunden.

Der komplette rechteckige Terrainbereich braucht vier wirkliche Mapterhorn-Kacheln,
obwohl die ausgewählten Achsen allein die südöstliche Ecke nicht erreichen.
Die Aufnahme akzeptiert ein bis vier ausdrücklich benannte eindeutige Fenster
im begrenzten 2×2-Bereich des Quellframes. Zwei Reihen bekommen Dateinamen mit
X und Y; bisherige Zweifenster-Eingaben und ihre Hashes bleiben unverändert.
Doppelte/fremde Kacheln und fehlende benötigte Höhen bleiben Fehler.

Neue Rohbytes: `14/8937/5679`, 60344 B,
SHA256 `fdcdbcdc1a23efbf059e49c7b6877d360af14e7c4c18ea78c50d32ea45b7a800`;
`14/8938/5679`, 53812 B,
SHA256 `b17173dba20466cc1475ed8c267300ae15f0b5325a428f47c3c1320d20ad237d`.
Provider-URLs und feste Crop-Hashes stehen in der Eingabe. Original-PBF und beide
nördlichen DEM-Rohstände bleiben erhalten. Kein Erwerb bei normaler Reproduktion,
keine Extrapolation und keine vollständige kachelübergreifende OSM-Abnahme.

## Reproduktion und Abgrenzung

[Aufrufe](README.md). Dieselbe Kontaktband-Zerlegung, selektive Float32-Reserve,
Membranenergie und feste Höhenelimination wie im Nordvergleich; dort stehen
SIGGRAPH-Primärquellen, eigene Ableitung und die bisherigen visuellen Gegenfälle.
Der unabhängige Leser prüft gespeicherte tatsächliche Körper und beide Präzisionen.
Solverstatus allein ist keine Abnahme. Kostenvergleiche verwenden gleiche feste
Vernetzung, Ziele, Intervalle, Qualitätsgrenzen und bisherige Ressourcenlimits.

Keine gemeinsame/lib/native/Reservations-API geändert. Der #483-Pilot bleibt
separat; eine weitere Straßeninstanz erzeugt keinen Katalogeintrag.
`admitted_source_ids=[]`. Übrige Quellen/Ebenen, Fahr-/Gehhüllen, Knickradien,
Handläufe/Statik, allgemeine Selbstschnittfreiheit und native Übernahme bleiben
offen. #434 und der vollständige Transportauftrag bleiben aktiv.
