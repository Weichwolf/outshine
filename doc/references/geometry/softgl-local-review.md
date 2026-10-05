# SoftGL: lokale Optimierungsverfahren für Outshine

Vergleich: `/Users/cosmo/Git/softgl`, Stand `74a6b6c`, 2026-10-05.
Software-Rasterizer in C11, SSE/WASM SIMD; kein Ersatz für SDL_GPU.
Die Verfahren liefern Hypothesen, keine Outshine-Kostenbelege.

| Befund im Produktionscode | Übertragung auf Outshine | Grenze |
| --- | --- | --- |
| `workers.c:sg_bin_sort_z` sortiert 4-Byte-Schlüssel statt 496-Byte-Dreiecksdatensätzen | Kompakte Cluster-IDs/Tiefenschlüssel; native Geometrie beim Ordnen unverändert lassen | Opaque Reihenfolge nur bei gleicher Semantik ändern; Material-Batching und zusätzlicher Verkehr zählen |
| `workers.c:sg_geometry_key` trennt Positions-/Indexrevisionen von Materialzustand | Geometrieauswahl getrennt von Material-/Lichtänderung wiederverwenden | Kamera, Projektion, Weltbasis, Geometrie und konsumierte Tiefe müssen gültig sein |
| `workers.c:sg_workers_geometry_replay` verwirft nur streng verdeckte Referenzen | Aktuelle gültige Tiefe zum Ausschluss ganzer Cluster-Unterbäume verwenden | Bei Drehung ist alte Verdeckung kein Beweis; Schatten und transparente Wirkung getrennt behandeln |
| `raster_hz.h:sg_hz_occlusion_class4` braucht vollständige Coverage und konservative Tiefe | HiZ-Lücken und Tiefengleichheit behalten; Reverse-Z und Rundung explizit prüfen | Keine Konstanten oder Tiefenkonventionen aus SoftGL kopieren |
| Gepackte Vertexpfade und begrenzter transienter Geometriespeicher | Unveränderliche Attribute und Bewegung trennen; tatsächlich gelesene Bytes reduzieren | Keine zweite residente Vollkopie; Dekodierung und Präzision im echten Shader messen |

## Gegenbeispiele und Messverfahren

- `experiments/depth-visible-vertices/README.md`: 25.668,35 von 42.914,69
  vorbereiteten Vertices entfallen, also 59,81 %. Kein reproduzierbarer BMW-4x-Gewinn;
  beide Varianten verworfen. Weniger logische Arbeit beweist keine kürzere Framezeit.
- `experiments/hz4-span/README.md`: zusätzliche hierarchische Row-Span-Prüfung
  kostet BMW 4x in beiden Audits 2,821 % / 3,148 %. Prüfkosten können Einsparungen übersteigen.
- `experiments/depth-replay-off-bound/README.md`: Wiederverwendung hilft BMW ohne MSAA,
  kostet aber 4x Zeit. Keine Übertragung eines Erfolgs auf andere Profile oder Szenen.
- `experiments/raster-profile-20261005/README.md`: Diagnose-Outlining verändert
  Codegenerierung; aufsummierte Thread-Samples sind keine Frame- oder GPU-Latenz.
- Kontroll- und Kandidatenläufe im Wechsel auf ruhigem Host, gleiche Eingaben/Bilder.
  Diagnosen getrennt von Zeitmessungen. Gezählt wird konsumierte Arbeit, nicht nur erzeugte.
  SoftGLs Warm-up gehört nicht in Outshines Place-Abnahme: Anfangsframes bleiben enthalten.

## Konkrete Reihenfolge

1. Frame-Spitzen mit tatsächlich eingereichten GPU-Kommandos und Vorbereitung korrelieren;
   Host-Encoding, Queue/Fence-Warten und GPU-Passzeit nicht gleichsetzen.
2. In 2336 räumliche Hierarchie/LOD vor Mesh-Erzeugung; in 2340 Cluster-Auswahl statt
   flacher Vollprüfung vergleichen. Sortierung bewegt Schlüssel, keine Meshdaten.
3. Aktuelle Tiefenvorlage gegen ihre zusätzliche Geometrie-/Bandbreitenarbeit messen.
   Materialspezifische Arbeit erst nach wirksamer Coverage; bestehende sichere Fallbacks erhalten.
4. Native Vertexstreams kompakt halten; konstante Farben/Emission und statische Bewegung
   gegen heutige per-Vertex-Streams vergleichen. Speicherpeaks und Decodekosten zählen.

Abnahme bleibt das vollständige Outshine-Bild mit identischem Profil, Sichtweite und Inhalt.
Alle zehn Places einschließlich Tokyo und Central Park; keine Lockerung der Bildreferenzen.
