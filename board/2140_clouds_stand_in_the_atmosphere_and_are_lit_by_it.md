Type: feature
State: open
Area: world, render
Tags: webcam, measured
Depends: 2172, 2167

# Weather generates clouds that share atmosphere and ground lighting

## IST und Lösung

Alle neun aktuellen Places zeigen klaren Himmel; Bewölkungsfelder haben noch keine
entsprechende sichtbare Form. Die frühere Abhängigkeit von Gras/Feuer (2137) entfällt.

Provider liefert Schichten/Deckung/Wind; Generator erzeugt deterministisches räumliches
Dichtefeld; Renderer integriert Licht/Transmittance. Für nahe Wolken begrenztes Volumen,
ferne hohe Schichten günstige integrierte Darstellung. Prozedurale Noise-Volumes einmal
berechnen/cachebar halten. Form ist plausible Synthese, keine Rekonstruktion einzelner Wolken.

Reduced-resolution March als Experiment mit festen Schritt-/Speichergrenzen, Empty-space-
Skipping und Early-out; zeitliche Reprojektion mit Depth/Transmittance-Disocclusion und
History-Reset bei Sprung. 320×180 sind bei 1280×720 ein Viertel je Achse, also 1/16 der Pixel,
nicht 1/4. 48/64/96 Schritte sind Versuchspunkte, kein zugesichertes Qualitäts-/Kostenoptimum.
Licht aus derselben Atmosphäre, begrenzte Multiple-scattering-Näherung; integrierte
Cloud-Shadow-Transmittance auf Terrain und Gebäude. Gemeinsame Komposition vermeidet doppelte
Luftperspektive; Regennebel und Wolken dürfen Gelände nicht falsch überdecken.

- [ ] Klar, geschlossene Decke, gebrochene Cumulus, tiefe Wolke über Berg: plausible Tiefe,
      helle Ränder/dunklere Basis, entsprechende direkte/diffuse Beleuchtung am Boden.
- [ ] Cover 0 lässt Cloud-Beiträge neutral; bewusst abgetrennte Ground-Transmittance scheitert
      am Schattenoracle. Bewegung/Wind/Teleports ohne dauerhaftes Ghosting.
- [ ] 2092 weist GPU-/History-/Noise-Kosten aus. Kein vorab als gemessen ausgegebener 3-ms-Wert;
      Qualitätsleiter unter Gesamtbudget, 2169 enthält die Integrationsreihenfolge.

Wahl: reduzierte volumetrische Integration nach
[Unreal Volumetric Clouds](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-cloud-component-in-unreal-engine),
gemessen auf SDL_GPU. RAGE ist Fernbildvergleich; seine internen Algorithmen werden nicht behauptet.
