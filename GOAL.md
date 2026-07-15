# /goal — Weltdaten-Dienst, testgesichert, dann Refactor nach Zuständigkeit

**Aktives Fokus-Ziel.** Ein **funktionierender Tile-Server als eigener Container**
(`fb-tiles`), der echte Weltdaten dynamisch nachlädt, aufbereitet und cached — und
von **beiden** Seiten konsumiert wird: die **Engine** holt sich die Bodenhöhe
(`/elev`, echtes AGL statt der flachen 71-m-Scheibe), der **Renderer** holt sich
Terrain, OSM-Vektor und Luftbilder; damit fällt jede vorgeladene Region weg und der
Startort ist weltweit frei. Abgesichert wird das durch **100 % Testabdeckung, wo sie
möglich und sinnvoll ist** — reine Logik (Kachel-/Projektionsmathematik, Parser,
Cache, Aero) wird vollständig unit-getestet und mit gcov *gemessen*, GL- und
Netz-Loops bleiben der End-to-End-Physiksuite überlassen; wo eine Lücke bleibt, wird
sie **benannt statt weggerundet**. Der **WASM-Renderer** bekommt `static_assert`, wo
es sinnvoll ist — vor allem auf die Wire-Structs und Vertex-Layouts, damit ein
Protokoll- oder Stride-Fehler beim Kompilieren auffliegt statt als Garbage im Bild.
**Erst danach** kommt der **Refactor nach Zuständigkeit**: die God-Files
(`xp_bridge.c`, `world3d.h`) werden entlang der Agenten-Grenzen in Module zerlegt —
abgesichert durch die dann vorhandene Testsuite, sodass „keine Regression" *bewiesen*
ist und nicht behauptet.

---

## Reihenfolge (bewusst so)

1. **`fb-tiles` fertig** — erst die Architektur richtig, dann Code verschieben.
2. **Tests + Coverage** — der Refactor braucht ein Netz, bevor er anfängt.
3. **`static_assert`** — billig, fängt genau die Fehlerklasse, die uns Stunden kostete.
4. **Refactor** — zuletzt, weil er nur so sicher ist.

Ein Refactor ohne Testnetz ist kein Refactor, sondern ein Umbau auf Verdacht.

## Done-Kriterien

**Tile-Server**
- [x] `fb-tiles` als eigener Container, Disk-Cache im Volume (Upstream einmal pro Kachel).
- [x] `GET /elev?lat=&lon=` — Engine bekommt echte Bodenhöhe; `HOME_ELEV`-Magic-Number weg.
      *Bewiesen: Hameln 70.91 m (alte Konstante: 71.0), Mont Blanc 4771 m, Amsterdam 7.2 m.*
- [ ] `GET /t/terrain|vector|imagery/{z}/{x}/{y}` — Renderer bezieht Kacheln von hier
      (MVT **gunzipped**: osmmesh' Decoder lehnt gzip ab).
- [ ] Vorgeladene PMTiles + `fetch-data.sh` **gelöscht**; beliebiger Startort fliegt
      (Beweis: Grenchen 47.283/7.524 startet auf ~717 m und zeigt Gelände).

**Tests**
- [x] Unit-Runner mit gcov, der die *eigene* Zahl von gcov nimmt und Lücken benennt.
- [x] `tiles/tilemath.h` 100 % (34 Zeilen) — jede geo↔tile-Umrechnung lebt dort.
- [ ] `elev.c`, `terrain.c`, Query-Parser, `protocol.h`-Layout, FDM-Kernmathe abgedeckt.
- [ ] Physiksuite vor/nach jedem Refactor-Schritt identisch grün.

**static_assert**
- [ ] Wire-Structs (`telem/ctrl/video_packet_t`): Größe + Feld-Offsets festgenagelt.
- [ ] Vertex-Layouts (Stride 32 = pos+uv+normal) gegen die `glVertexAttribPointer`-Offsets.

**Refactor**
- [ ] `xp_bridge.c` → `fdm/{aero,atmosphere,ephemeris}`, `link/{xplane,msp}`,
      `nav/autopilot`, `weather/`, `telemetry`, `main` — Owner je Modul.
- [ ] `world3d.h` → `gfx/{shaders,sky,terrain,hud,video,camera}`.
- [ ] Keine Datei mehr ohne eindeutigen Owner; keine God-Files.

## Grenzen
- **Nicht-Ziele:** echtes ELRS/5,8 GHz, Video-Scrambler, reale Hardware — bleibt später.
- **Verifikation zuerst:** jede Stufe wird gemessen, bevor sie als erledigt gilt.
  „Läuft vermutlich" zählt nicht; „Coverage 100 %" ohne gcov-Ausgabe auch nicht.
