# Nativer Wolkenhimmel bis zur physischen Strahlgrenze

#389 ergänzt den tatsächlichen Scene/Depth-Verbrauch aus PR #360 um einen
ausdrücklich gewählten privaten Boundary-Sky-Modus. Die native Probe zeichnet
Wolken und klare Luft auch bei depth0; sie ist noch kein produktiver SkyStage-
oder Place-Verbraucher. Gemeinsame Stage-/Ressourcenintegration bleibt #328/#266.

## Eingabe und Komposition

| Eingabe | Unveränderter Standard | Boundary-Sky-Modus |
|---|---|---|
| Positive Tiefe | Lineare Oberflächenradiance | Derselbe Vertrag |
| depth0 | Bereits integrierte Augenradiance, unverändert | Lineare Radiance an der Top-/Planetgrenze |

Der Aufrufer wählt den Modus ausdrücklich in Host und Shader. Ein bereits
integriertes Sky-LUT-Bild ist keine gültige Boundary-Eingabe. Keine Subtraktion
einer anders integrierten klaren LUT: `C = Boundary*T_joint + Lux*L_joint`.
Die Probe verwendet homogene, ozonfreie Luft und konstante kontrollierte
Sonnen-/Psi-Tabellen; dies ist keine vollständige natürliche Himmelsbeleuchtung.

MediumView224 liefert Richtung, Jitter, Viewport und Ortho-Ursprungsversatz.
Bei depth0 ersetzt die physische Top-/Planetgrenze die rekonstruierte
Oberflächendistanz. Kamerafar begrenzt diese Strecke nicht. Der bereits vorhandene
metrische Wetteranker plus Ortho-Offset bleibt am Auge; Vakuum vor dem
Atmosphäre-Eintritt erzeugt weder Radiance noch Extinktion.

Nur die private Sky-Grenzrechnung verwendet eine kompensierte Kugelkonstante
`dot(origin,origin)+2*R*origin.y-h*(2*R+h)` mit dem vorhandenen stabilen
`airSolveQuadratic`. Ursprung, R und h werden dafür in km skaliert, Produkte
mit `fma` berechnet. Der große Radius wird nicht zuerst zur kleinen Kamerahöhe
addiert. Positive Tiefen und der gemeinsame MediumView-/AirPath-Vertrag bleiben
erhalten. Allgemeine höhenabhängige Quellen-/Schattenfehler sind damit nicht
abgenommen; Light besitzt deren Integration.

## Reproduktion

Erforderlich: NumPy, Matplotlib, C++23, glslangValidator, reguläres Enginearchiv
und die Produktions-Linkabhängigkeiten. Private Ausgaben ausschließlich im
System-Tempverzeichnis. Der Builder verwendet alle Produktionsheader aus dem
angegebenen sauberen Archiv-Checkout, einschließlich dessen nativer Device-API;
aktuelle Experimentquellen werden gegen dieses passende Paar gebaut.

```sh
python3 test/experiments/weather/cloud_view_native.py \
  --archive-root /path/to/regular/archive-checkout \
  --output /tmp/outshine-sky-legacy
python3 test/experiments/weather/cloud_view_native.py \
  --archive-root /path/to/regular/archive-checkout --boundary-sky \
  --output /tmp/outshine-sky-native
nice -n 10 python3 test/experiments/weather/cloud_sky_raster.py \
  --native-receipt /tmp/outshine-sky-native/receipt.json \
  --legacy-captures /tmp/outshine-sky-legacy \
  --output /tmp/outshine-sky-checked
```

Mit `--width 1280 --height 720 --scenario sky-broken` wird ein vollständiges
Bild geprüft; `--legacy-captures` entfällt bei einem einzelnen Fall. Der Host
zeichnet jeden Zustand zweimal, prüft Wiederholbarkeit und speichert Rohwerte,
unabhängige Kamerastrahlen und tatsächliche Wetterparameter. Die26 kleinen Fälle
prüfen Klar/Bedeckt/Aufgebrochen, Wind, Translation, Ortho, Jitter, Offset, Schicht
innen/oben, Orbit und Vakuum. Float64 integriert die kontrollierten lokalen
Koeffizienten unabhängig; RGB/L/T behalten1e-4. Die neue volle planetare
Endpunktkontrolle hat1m; die separate vorhandene Receiverkontrolle bleibt2mm.
Diese endliche Kohorte begründet keine globale Genauigkeit an Tangenten.

Far50/6000/600000m, unveränderte opake Rohbytes, Vakuumidentität und lineare
Boundary-Antwort werden separat geprüft. Diagnose-PDFs verwenden dieselbe
Displayfunktion4x/(1+4x). Aufgebrochene Wolken und ihre60s-Advektion sind echte
native Bilder; keine TAA-/History- oder Bewegungsstabilitätsabnahme daraus.

## Grundlage und Grenzen

Hillaire, EGSR2020, PDF-Seite3/Gleichungen1–4 optisch geprüft:
`doc/references/atmosphere/egsr/2020-production-ready-atmosphere.pdf`.
Schneider/Vos, SIGGRAPH2015, PDF-Seite54/Beer-Abbildung optisch geprüft:
`doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf`.
Bestehende Dichte, CloudPrefix und gemeinsame Zellintegration werden konsumiert.
Keine neuen Bindings, MRTs oder produktiven Backend-Aufrufe. Zähler und gehaltene
Diagnosespeicher/Kopien werden ausgewiesen. Echte GPU-Zeit, vollständiger Frame,
Solarabschattung/Psi, Thin/History, Horizontqualität #264, Places und A18-/RTX-
Budgetabnahme bleiben offen. Nur Master veröffentlicht Place-/Galeriebilder.
