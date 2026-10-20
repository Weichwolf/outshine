# Positive Restterm-Spannen begrenzen die Arbeit, lösen den Filter aber nicht

Owner #948, Parent #422/#6. Ein neuer adaptiver Ansatz wird am unveränderten
`sheared-corner-0.003` begrenzt verworfen. Die tatsächliche Güte verbessert sich
stark, besteht aber fünf der zwölf ursprünglichen RMS-Grenzen nicht. Kein Port,
keine weitere Blatt-/Gruppen-/Momentordnungsstudie und kein veränderter Guard.

## Ableitung und Auswahl

Für Steigung q, `S=1+q.q`, `C=Hz-Hxy.q`, `B=S+(a²−1)C²` und `a²=r⁴` ist
der vorhandene positive Hilfskern `W=a²/(πB²)`. Die ursprüngliche BRDF verwendet
`nl=max(n.l,0)`, `nv=max(n.v,1e−6)` und `nh=max(n.h,0)`. Daher gilt ohne
Division durch einen kleinen Kern:

```text
response/W = Fresnel * T(nl,nv) * (S² wenn C>=0, sonst B²)
T(x,y) = 0.5*x / [x*sqrt((1−a²)y²+a²) + y*sqrt((1−a²)x²+a²)]
```

Für x>0 lässt sich T als `0.5/[sqrt((1−a²)y²+a²)+y*sqrt(1−a²+a²/x²)]`
schreiben: wachsend mit x≥0, fallend mit y>0; der stetige Grenzwert bei x=0 ist 0.
Lineare Dot-Numerator-Extrema liegen an Polygonecken. Positive Normgrenzen folgen
aus der q-BBox. Daraus entstehen nl/nv-Intervalle, monotone T-Endpunkte und
positive Polynomfaktor-Spannen. Bei einem nh-Guardwechsel beide Zweige einhüllen;
negative n.v bleiben im Modell. Intervallabhängigkeiten machen die Spannen bewusst breit.

Auf jedem exakt lichtbeschnittenen Polygon begrenzen `Dichte*Kernmasse*[Rmin,Rmax]`
das Integral in reeller Arithmetik. Konstante Normalanteile werden unverändert addiert.
Nach größter gewichteter Spannenbreite halbieren; gleiche Dichte, Elternfläche und
Kernmasse erhalten. Die größte q-BBox-Achse bestimmt die Schnittrichtung.
Der Ausgabewert ist die Mitte der gesamten positiven Spanne. Die Stopregel
`Spannenradius <= 0,004*untere_Integralschranke` verwendet kein Referenzorakel.
Sie wäre eine hinreichende punktweise Genauigkeit unter den bisherigen Kurvengrenzen,
keine notwendige Bedingung für deren Bestehen. Höchstens 64 aktive Blätter pro Abfrage.

**Rundungsgrenze:** Diese Restterm-Einhüllung gilt algebraisch. Float64-Operationen,
GEOS-Clipping und vorhandene analytische Kernmassen sind nicht mit gerichteter
Rundung eingeschlossen. `model_tolerance_met` bedeutet daher ausdrücklich keinen
rigorosen Floating-point-Fehlernachweis oder eine Runtime-/Filterfreigabe.

## Gleiche Eingabe, gleiche Grenzen

Alle 24 ursprünglichen Footprints bleiben im Referenzmanifest; hier werden nur
die zwölf Kurven dieses bekannten Gegenfalls ausgewertet: r=0,2/0,5/0,9, Sicht 0°/50°,
Azimut0°/90°, jeweils 65 Lichter. Originalarchiv SHA256
`d7b305c0cbac4ca0a6b7a8ebc7c935c2c3931d79e91db19bdec22b5f365d9397`.
Source-Hashes und der unabhängige konvergierte Referenzbericht werden geprüft.
Die Grenzen bleiben 2% maximal und 0,5% RMS, jeweils relativ zum eigenen Referenzpeak.

| Modell auf diesen 12 Kurven | Falsche Kurven | Größtes max/Peak | Größtes RMS/Peak | Negative Werte |
|---|---:|---:|---:|---:|
| Ursprünglicher Schwerpunkt | 9 | 22,1870% | 17,4093% | 0 |
| Adaptive positive Spanne, <=64 Blätter | 5 | 1,56864% | 1,08078% | 0 |

Alle Maximalgrenzen bestehen; RMS scheitert in Kurven 3/7/8/9/11, davon drei matte.
42/780 Abfragen erfüllen die neue Spannenregel; 738 enden ohne diesen Nachweis am
Budget. Kein Referenzwert liegt außerhalb der Spanne in der separaten 1e−11-
Numerikkontrolle. Diese breite Einhüllung beweist keine ausreichende Bildgüte.

Am bekannten vollständig beleuchteten Glanzpunkt (r=0,2, Sicht 50°, Azimut 90°,
Licht−35°): Referenz 0,000140760803, ursprünglicher Schwerpunkt 0,0001010162,
adaptiv 0,000143539414. Punktfehler 1,97399%, Spanne
[0,000126735161;0,000160343667], Radius/Untergrenze 13,2593% statt Ziel 0,4%.
Der Punktfehler benutzt seinen tatsächlichen Wert, die Kurvengrenzen ihren Peak.

## Arbeit, Bytes und Prüfungen

Dasselbe 144B-Startmodell und 2340 konstante BRDF-Auswertungen. Statt 780 ursprünglichen
Kern-/Schwerpunktabfragen mit 3120 Kantenbesuchen entstehen 94702 Kern-/Schwerpunkt-
abfragen (121,4fach), 378808 Kantenbesuche,378808 zusätzliche Smith-Wurzeln,
46961 Schnitte/93922 Polygonintersektionen und3084412 Summandenbesuche.
Auch der unbenutzte Schwerpunkt wird durch die wiederverwendete Kernroutine berechnet.
Keine Arbeitsersparnis durch die fehlenden residualen Punkt-BRDF-Aufrufe behaupten.

Explizite neue Split-Koordinatenpayloads 6011008B plus 94702 finale gewichtete
Zweierintervalle 1515232B; das sind bekannte Allokationspayloads, kein vollständiger
Kopier-/Allocatornachweis. Maximal 5120B aktive numerische Zellarrays pro Query,
40268B Ergebnisarrays einschließlich Diagnoseverlauf. Der separat bytegleiche
64-Blatt-Pilot erreicht 39272B Python-Tracing-Peak. GEOS-/native Peaks, tatsächliche
Gesamtallokationen und Kopierbytes bleiben ungemessen. Linux/i5-1135G7 unter gleichzeitiger interaktiver
Last: 45,04s für die neue Abfragefolge, 0,0206s für den ursprünglichen vektorisierten
Vergleich; kein Gerätebeleg. Der vollständige ursprüngliche
Kernvergleich ist vektorisiert; einzelne Pythonzeiten sind keine nativen Speedups.

Neun strikte Kontrollen prüfen algebraische Einzelpunktparität einschließlich
negativer View-/Half-Guards, Bereiche über ursprünglichen BRDF-Punkten, monotones T,
Achsenpermutation, Flächen-/Kernmassenerhalt, unabhängige positive 32/64-Quadratur,
exakte konstante Verteilungen, kleines Support und lokale Eingabebudgets.
Die restlichen 276 Kurven und native/GPU-/Weltprüfungen folgen diesem verworfenen
Kandidaten nicht. Keine größere adaptive Obergrenze nach diesem Befund.

## Reproduktion und Quellen

```sh
PYTHONPATH=lib/src:test/experiments/render OPENBLAS_NUM_THREADS=1 .venv/bin/python -B -W error -m unittest test_relief_residual_range
PYTHONPATH=lib/src:test/experiments/render OPENBLAS_NUM_THREADS=1 .venv/bin/python -B -W error test/experiments/render/compare_relief_range.py --reference /tmp/outshine-original-relief-reference --output /tmp/outshine-relief-range-proof
PYTHONPATH=lib/src:test/experiments/render .venv/bin/python test/experiments/render/plot_relief_range.py --output /tmp/outshine-relief-range-proof
```

Vorhandene NumPy/Shapely/Matplotlib-Umgebung, keine neue Abhängigkeit. Die unveränderte
Originalreferenz stammt aus `relief_shading.py`; normale Kontrollen erzeugen keine Pins.
SIGGRAPH zuerst: Han et al. 2007, vorhandenes PDF S. 3/Gl. 4–5 optisch geprüft:
effektive BRDF über die Normalenverteilung. Heitz et al. 2016,
[LTC](https://eheitzresearch.wordpress.com/415-2/), vorhandenes PDF S. 4/Abb. 5/Gl. 11
optisch geprüft: positive Polygon-Kernintegration. Keines der Papers liefert die
hier gesondert hergeleitete Restterm-Spanne. Keine fremden Fit-Tabellen/Codekopien.
Diagnosen bleiben lokal und werden geöffnet; vollständiges #422 bleibt offen.
