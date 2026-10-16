# Höhenabhängige native Luftkontrolle (#353)

`cloud_air_depth_native.py --profiled` verwendet die native `kEarthAir`-Deklaration:
exponentielle Rayleigh-/Mie-Profile und das dreieckige Ozonprofil. Die sechs Bindings,
Medium80 und der tatsächliche Weather-Sun-T6-/CloudPrefix-Verbrauch bleiben erhalten.
Sun-/Psi-LUTs enthalten ausdrücklich konstante Kontrollwerte; dies belegt weder eine
konvergierte Sonnen-LUT noch Nacht, diffuse Wolkenantwort oder Runtime-/Placebilder.

Der Kandidat nutzt den vorhandenen quadratischen Quellen-/Extinktionsfit und
`mediumCellIntegrate`. Physische Knoten `expm1(i/64)` km wachsen von beiden Enden
bis zur Mitte der vollständigen klaren Strecke. Ozon-/Schattenkanten kommen wie
bisher hinzu. Die Partition hängt von der physischen Strecke ab, nicht von Queries.
Einseitige Knoten verschwenden beim Orbitabstieg ihre dichten Proben im oberen
Bereich; am Boden unterschätzen sie die schnelle Mie-Variation.

`AIR_PREFIX_ACCUMULATE_DEPTH=1` summiert die unveränderten analytischen
Bernstein-Zellintegrale vor dem Exponential. Es vermeidet die Rundungsdrift vieler
kleiner Transmittanzprodukte. Der Default bleibt aus; Shaderbindings und
Hostlayouts ändern sich nicht. Gemeinsame Wolken-/Luftzellen verwenden weiterhin
ihre bisherige Komposition vor der Integration.

Die unabhängige Referenz integriert variable lokale Extinktion auch innerhalb
jeder Quellenquadratur. Vertikale geschlossene Exponential-/Ozonspalten,
Rayleigh-Radiance und der homogene Grenzfall prüfen sie unabhängig. Gauss16/32
mit höchstens25m-Teilsegmenten müssen bis1e-8 absolut übereinstimmen.
Vier Zustände,30 Strahlen und dichte/sparse/wiederholte Queries behalten1% RGB
bei1e-5 Floor; klare Transmittanz2e-6, deklarierte Wolkenzellen1e-4 absolut.
Der eingefrorene Eintrittskoeffizientenpfad ist eine bewusst falsche Kontrolle.

Reproduktion mit regulär gepaartem Archiv-/API-Stand:

```sh
python test/experiments/weather/cloud_air_depth_native.py --profiled \
  --output /tmp/outshine-light-profiled-air-<commit> \
  --api-root <clean-producer-checkout> --archive <regular-liboutshine.a> \
  --archive-receipt <producer-receipt.json> --repeats 3
```

`analysis.json` trennt Referenzfehler, Quellen-/Dichtearbeit und Kontrollen.
`receipt.json` enthält terminale Befehle, tatsächliche GPU-Zeiten und Lastzustände.
Das1920×1080-Schritt8-Raster misst nur Komponentenarbeit bis zur physischen Grenze;
sein RGB ist kein unabhängig abgenommenes Vollbild. Residente Tiefe, Interpolation,
Opaque/Thin/History und vollständige Frame-/A18-Budgets bleiben offen.
Grundlagen und optisch geprüfte SIGGRAPH-Quellen stehen bei #353.
