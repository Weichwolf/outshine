# Physischer Sonnenpfad der sichtbaren Scheibe (#397)

`sky.frag` verwendet `clearSunPhysicalTransfer` am tatsächlichen Pixelstrahl.
Oberhalb der Atmosphäre wird der Eintritt bestimmt; bei verfehlter Atmosphäre
bleibt Sonnenradiance ungeschwächt. Die frühere eigene UV-Rechnung am Kameraradius
lag außerhalb des Veil-Domains und klemmte auf einen falschen LUT-Rand.
Sonnen-Lux/solid angle, SkyView-Radiance und statische Velocity bleiben erhalten.
Die beiden Sampler und Uniform192 samt alten Hostfeldern bleiben unverändert.
Keine Stage-/Backendänderung zu #328 und keine neue Atmosphärenintegration.

Der native Versuch rendert die tatsächlichen `fullscreenNdc.vert`, `sky.frag`
und `mediumTransmittance.comp`: acht Boden-/Top-/Orbitfälle bei1280×720,
1° vertikalem Sichtfeld und tatsächlichem Sunhalbwinkel0,00465rad.
SkyView enthält einen festen, ausdrücklich nichtphysikalischen Kontrollwert
RGB(0,125;0,25;0,5). Seine Radiance bleibt außerhalb der Scheibe exakt erhalten.
Die GPU erzeugt die produktive256×64-RGBA16F-Veil-LUT. Ein zusätzlicher FP32-
Samplergegenfall verwendet dieselben ausgelesenen Texelwerte ohne Neuberechnung.

Die unabhängige Referenz bestimmt Kugelschnitt, Eintritt, Planetenschatten und
bilineare Abbildung. Vakuum und die Gleichheit mit einer zum Eintritt versetzten
Kamera besitzen analytische Kontrollen. Die Lookupabnahme begrenzt den absoluten
Fehler auf0,001 der einfallenden Sonnenradiance je Kanal (0,1%). Das ist ein
separater Scheiben-/Samplervertrag; die1%-RGB-RTE-Grenze von #353 wird nicht geändert.
Deren Floor darf hier weder auf andere Einheiten übertragen noch als bestanden
behauptet werden. Relative Solar-RGB-Fehler mit1e-5 Floor werden zusätzlich roh
berichtet: winzige dunkle Kanäle können wegen bestehender LUT-/Filterpräzision
weiterhin100% erreichen. Kein Clamp und keine Änderung an Veil/FP16.

Pixel innerhalb von vier FP32-Epsilon am Sonnen-Cosinus-Schwellwert werden für die
numerische Lookupabnahme gesondert ausgewiesen. Der vorherige/nachherige native
Scheibenmaskenvergleich und vollständige Bilder bleiben davon unabhängig.
Nichts wird im Renderer gekürzt. Bodentag, Bodensonne und Top-Kontrolle müssen
bytegleich bleiben; VelocityRG bleibt exakt-10000. Vorhandene LUT-Integrations-,
Format-/Filter- und Sonnenkantenfehler sind keine korrigierte Lookupgeometrie.

```sh
python test/experiments/render/sky_disc_native.py \
  --baseline c4166e4f9 --output /tmp/outshine-light-sky-disc-<commit> \
  --api-root <clean-archive-producer> --archive <regular-liboutshine.a> \
  --archive-receipt <producer-receipt.json>
```

`receipt.json` hält Archiv-/API-Paar, Quellen-/Capturehashes, Aufrufe und terminale
Ergebnisse fest. Prozess-/IO-Wallzeiten sind keine Frame-/GPU-Zeiten. Der bisherige
gepaarte Probehost liefert keine Raster-/Submission-Timestamps; keine Zeit aus
Fences ableiten. Neue Device-Header benötigen ihr dazu gebautes reguläres Archiv.
Sky-/Aerial-/History-/Place-/A18-Abnahme bleibt getrennt. Der allgemeine klare
Sonnenpfad und die solaren Integrationsgrenzen bleiben bei #206/#259/#353.

`python test/experiments/render/sky_disc_report.py --output <derselbe Pfad>`
erzeugt ein dreiseitiges PDF mit Vorher/Nachher, Lookupfehlern und tatsächlichen
nativen Spektren. Es benötigt Matplotlib; die Referenz benötigt NumPy.
