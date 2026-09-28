Type: feature
State: active
Architecture: ready
Priority: P1
Parent: 2169
Area: world, scenario, render
Tags: webcam, measured
Depends:

# One weather snapshot drives air, light, water and surface state

## Current source audit and first executable slice

Scenario::Weather validation/import/export preserve seven cloud/wind fields; Haze
already reaches Declaration/RuntimeScene::EarthMedium. Cloud/wind fields have no
render consumer. WeatherProvider/CalmWeather remain a provider boundary, not live weather.

First slice: world/weather owns a small immutable WeatherSnapshot from existing
Ground.Sky plus declared world time, with source/revision/validity and canonical units.
Engine captures it at tick/frame boundaries; RuntimeScene and atmosphere consume its
Haze instead of maintaining a parallel weather interpretation. Preserve clear-air output.
Carry cloud/wind inputs unchanged for 2140; do not claim a visible effect until consumed.
Convert meteorological wind-from to the native frame at the boundary; ambiguous
provider percent/height datums require adapter contracts, not guessed renderer units.
Owners: scenario/WeatherValidation, world/weather snapshot, engine/Declaring and
RuntimeScene, render atmosphere inputs. No public schema expansion or weather IO on frame.
Immutable snapshot failure retains the prior valid state; no partial consumer update.
Tests: existing XML/API/roundtrip bounds, clear-air equivalence, wind-axis fixtures,
source/time/revision coherence and late-result rejection. A disconnected Haze consumer
and reversed wind-from conversion must FAIL. Format, focused suites, full lint and
public-client clear/haze PNG comparison. Broader surface/water effects below are later slices.

## Weltweiter Licht- und Atmosphärenvertrag

Volumetrisches Licht, Schatten, Luftperspektive und Farbgestaltung sind eine gemeinsame
Kernkompetenz. Ort, Datum, Uhrzeit, Höhe und Wetter treiben denselben konsistenten
Zustand für direkte/indirekte Beleuchtung, Medium und Belichtung. Sonnen- und lokale
Lichtquellen einschließlich Abschattung integrieren; klare Luft braucht subtile Tiefe,
Nebel/Gegenlicht stärkere Streuung. Keine fest eingestellten Place-/Sonnenuntergangs-Looks.
Lokale Nebelbänke, Tunnelabluft und Straßenlichtkegel sind räumlich begrenzte Medien,
keine globale Haze-Erhöhung. Ihre Streuung nutzt dieselben lokalen Lichter und Caster
wie 2128; Extinktion und Luftperspektive dürfen nicht doppelt gezählt werden.

Abnahmematrix: beide Hemisphären, Äquator, mittlere Breiten, Polarregionen und Hochgebirge;
Jahreszeiten, Morgen/Mittag/Abend/Nacht, Polartag und Polarnacht; klar/bedeckt/Nebel.
Astronomisch unmögliche Kombinationen ausschließen. Repräsentative Fälle prüfen den
allgemeinen Vertrag, ohne Vollabdeckung sämtlicher Orte/Zeitpunkte zu behaupten.
Bei Kamera-, Zeit- und Wetterübergängen auf Flimmern, Nachziehen, Kontrast-/Farbsprünge
prüfen; lesbare Schatten und stabile Tiefenstaffelung erhalten. 2092 misst Frame-/Speicherkosten.
Zuständigkeiten: 2167 indirektes Licht, 2128 Schatten/Lichter, 2140 Wolken, 2155 Farbantwort;
2213 verbindet Sonne, Mond und Sternenhimmel mit demselben Weltzustand.

## Implementierung

1. Zeitgestempelter räumlicher Wetter-Snapshot aus Provider/Scenario, mit Quelle, Einheiten,
   Höhenbezug, Gültigkeit und Auflösung; Replay friert genau diese Daten ein. Vorhandene
   Scenario-Felder wiederverwenden, keine zweite konkurrierende Wetterdeklaration.
2. Wind (NED → Weltbasis), Sichtweite, Wolkenanteile/Basis, Niederschlag und Temperatur
   konsistent an Atmosphäre, 2140, 2167, 2129 und Materialien liefern. Nicht vorhandene
   Größen explizit als plausible Ableitung kennzeichnen; keine historische Messung erfinden.
3. Sichtweite auf Aerosolextinktion abbilden, Rayleigh nicht doppelt zählen. Homogene
   Näherung: beta_total = -ln(0.02)/V = 3.912/V; Aerosolanteil aus Gesamtextinktion minus
   Molekülanteil mit gültigen Grenzen. Höhenprofile/Nebel getrennt deklarieren.
4. Feuchte-/Schnee-/Pfützenzustand zeitlich integrieren, nicht Regen-Bool auf alle Flächen
   setzen. Ohne Wettervorgeschichte plausiblen Anfangszustand deklarieren. Wolkenfeld aus
   Seed, räumlicher Korrelation und Wind erzeugen; seine einzelne Form ist erfunden.
5. Asynchrone Abfrage, begrenzte räumliche Updates und stetige zeitliche Übergänge.
   Uhrzeit, Sonnenrichtung, Belichtung, Wolkenlicht und Boden benutzen denselben Snapshot.

## Abnahme

- [ ] Klar/bedeckt/Regen/Nebel/Schnee bei identischem Ort ergeben physikalisch zusammenhängende
      Änderungen. Keine Forderung nach der einzelnen Wolke des Webcam-Fotos.
- [ ] Winddrehen bewegt Wolken und Wasser konsistent; Sichtweitenreihe hat monotonen
      Kontrastverlust. Snapshot-Replay liefert identischen Zustand; absichtliches Abklemmen
      eines Verbrauchers scheitert am jeweiligen Wirkungsoracle.
- [ ] Keine Wetter-IO/Generierung blockiert einen Frame; 2092 misst Wechsel unter Bewegung.
- [ ] Nachtstraße mit Laternen, bewegtem Scheinwerfer und lokalem Nebel: Kegel und
      Verdeckung reagieren auf Licht/Caster; ohne Nebel verschwindet nur die Streuung.
      Räumliche Auflösung und zeitliche Reprojektion messen, Ghosting im Schwenk geht rot.

Wahl: eine deklarative Wetterquelle mit physikalischen Verbrauchern statt handgemalter
Place-Looks. Unreal-Atmosphäre ist die technische Referenz, RAGE-Timecycle nur das
vergleichbare Organisationsprinzip, keine übernommene proprietäre Implementierung.

Experiment: Hillaire 2020 (dynamische Sky-View-/Aerial-Perspective-LUTs) gegen
Bruneton 2017 (vorberechnete Streuung) auf denselben klaren und dunstigen
Snapshots messen. Sonne am Zenit/Horizont, Hochlage und Wetterwechsel prüfen;
separate Sky-/Ground-Irradiance und Transmittance-AOVs müssen konsistent sein.
CPU/GPU-p95, LUT-Bytes und Update-Spitzen auf A18 Pro entscheiden, nicht PC-Zeiten.
Die RDR2-SIGGRAPH-2019-Präsentation ist Vorbild für geteilte Atmosphäre,
Wolken/Fog und Sky-Irradiance, nicht für eine kopierte Implementierung.
Referenzen: https://sebh.github.io/publications/egsr2020.pdf ;
https://ebruneton.github.io/precomputed_atmospheric_scattering/ ;
https://advances.realtimerendering.com/s2019/index.htm .

## Verified input boundary

WeatherValidation's field schema supplies import/export/API bounds: cloud fractions
[0,1], nonnegative AGL/wind speed, finite unwrapped wind-from direction and finite
nonnegative Haze within current GPU storage. No clamps or truncated number prefixes.
Defaults/zero values and all seven cloud/wind fields survive roundtrip. Historical
NaN/Inf/negative/token/retry/layer fixtures pass; full physical weather remains open.

## Updategrenze

WeatherSnapshot-Revision ist keine Terrain-/Geometrie-Quellrevision. Änderungen von Zeit,
Wind oder CloudCover aktualisieren begrenzte Renderinputs/LUTs/History; kein kompletter
Ground-/WorldContent-Neubau und keine Neuinstanziierung stabiler Weltobjekte pro Tick.
Erster Snapshot-Slice erhält bestehende Deklaration; dynamische Wirkung folgt separat.
Counter-/PNG-Kontrolle: Wind-/Zeitwechsel verändert Wetterverbraucher, bewahrt Geometrie-
IDs und Uploads; absichtliche vollständige Redeclaration verletzt das Kostenoracle.
