# Backendfreier Wolkenkern

[#193](https://github.com/Weichwolf/outshine/issues/193) besitzt die deklarierte
Wolkenschicht, [#239](https://github.com/Weichwolf/outshine/issues/239) ihre endlichen
Extinktionsmomente. Dieser Kern verwendet ausschließlich C++/NumPy und native
metrische Ost/Oben/Süd-Werte. Renderpässe, HTTP/Cache, Wetterbeobachtungen und
Engine-Verkabelung sind eigene Integrationsschritte.

`CloudLayer`, `CloudShell` und `CloudExtinction` definieren ein weltverankertes,
windadvectiertes Dichtefeld in einer gekrümmten 600-m-Schicht. Zwei glatte
Rauschbänder und ein parabolisches Höhenprofil bilden ein begrenztes prozedurales
Stratusrezept. CloudCover steuert die Rezeptschwelle, keine exakt gemessene
Flächenbedeckung. Gültige endliche interne Parameter, positive Dicke/Radius,
nichtnegative Extinktion und normierte Richtungen sind Voraussetzung.

`CloudOpticalDepth` und `TraceCloudMoments` teilen dieselben 16 Dichtezellen pro
sichtbarem Schichtintervall. Der Kugelplanet schneidet die verborgene Halbkugel ab;
Empfängertiefe begrenzt die Integration zusätzlich. Es gibt höchstens zwei
Intervalle, keine IO, Allokation oder implizite Weltaktualisierung. Meteorologischer
Wind-from wird in Ost/Oben/Süd umgerechnet; die Zeitphase liefert der Aufrufer.

`TraceCloudMoments` liefert M0=`Weight` und M1=`WeightedDistanceM` in Metern.
Konstante Zellen verwenden den analytischen Beer-Schwerpunkt, eine Reihe um null
vermeidet Auslöschung. Beide Momente gemeinsam filtern und erst danach dividieren;
0/0 bleibt der klare Fall. Die analytische Zellintegration ersetzt keine
Verfeinerung der innerhalb der Zelle variierenden Dichte.

## Reproduktion und Grenzen

Erforderlich: NumPy, C++23. Private Ergebnisse ins System-Tempverzeichnis legen.

```sh
python3 -W error test/experiments/weather/cloud_moments.py \
  --output "${TMPDIR:-/tmp}/outshine-cloud-moments"
sh test/run.sh outshine/src/world/weather/CloudLayer
```

Die Python-Referenz prüft 128 logarithmische Tiefenfälle gegen 90-stellige
Decimal-Rechnung und vergleicht 512/1024 Zellen im deklarierten Feld. Rohe
M1-Meterwerte überlaufen FP16; Kilometerdarstellung ist nur ein Gegenfall,
kein festgelegter GPU-Packingvertrag: Bei sehr geringer Extinktion rundet FP16
M0 auf null, während M1 in Kilometern noch positiv bleibt. Ein einzelner Mittelabstand liefert bei
zwei getrennten Tiefen außerdem keinen exakten Bewegungsmittelwert. Deshalb
belegen diese CPU-Prüfungen weder eine TAA-/Ghostingabnahme noch Placebilder oder
Framebudgets. Renderanbindung folgt dem gemeinsamen Metal-/Vulkan-Vertrag #48.

Der [native GPU-Momentverbrauch](cloud_moments_native.md) ergänzt dieselben rohen
Momente mit unabhängigen Decimal-Kontrollen und tatsächlichem Scene/Depth-Raster.
Er wählt weder Produktionspacking noch eine allgemeine Wolken-History.

Verfahrensgrundlage: Schneider/Vos, *The Real-time Volumetric Cloudscapes of
Horizon Zero Dawn*, SIGGRAPH 2015, PDF-Seiten31/33,54/56,76,84/86 optisch geprüft:
`doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf`.
Yang/Liu/Salvi, *A Survey of Temporal Antialiasing Techniques*, CGF2020,
§3.2/Gleichung1, PDF-Seiten3/4 als Bilder geprüft:
`doc/references/presentation/cgf/2020-temporal-antialiasing-survey.pdf`.

## Nativer gemeinsamer Wolken-/Luftpräfix

Der [begrenzte native Teilstreckenverbraucher](cloud_air_prefix.md) ergänzt #193/#266.
Er konsumiert den integrierten klaren Kern aus #233 und kombiniert die lokalen
Wolken-/Luftkoeffizienten auf einer festen gemeinsamen Partition. Referenzen,
Reproduktion und offene Bild-/Integrationsgrenzen stehen beim verlinkten Verfahren.

Der [native Boundary-Sky-Verbraucher](cloud_sky_raster.md) integriert dieselben
Medien bis zur physischen Himmelsgrenze. Sein ausdrücklich gewählter
Grenzradiance-Vertrag erhält den bisherigen bereits integrierten Eye-Sky-Modus.

Das [versionierte Körperrezept](cloud_shape.md) ergänzt ausdrücklich wählbare
Kumulusformen und den Übergang zur geschlossenen Decke. Der Stratus-Default bleibt
erhalten; native Werte, Bilder und Kosten werden getrennt geprüft.

## Native Wetterdaten

Der [native Open-Meteo-Decoder](native_forecast.md) überträgt die bestätigten
Felder aus #253 und prüft echte Antwortbytes sowie exakte UTC-Auswahl gegen die
Python-Referenz. Öffentliche Snapshot-Publikation und sichtbarer Wetterverbrauch
bleiben eigene Integrationsschritte.
