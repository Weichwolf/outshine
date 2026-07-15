# osmmesh — vendored copy

Source: https://github.com/… (local: ~/Git/wasm-osm/libosmmesh) — copied verbatim so FlightBox
has **no external path dependency**. Do not edit here to fix upstream bugs without also carrying
the change back upstream; note any local delta below.

Local deltas: none.

`../data/*.pmtiles` are the prebuilt Hameln region tiles (Shortbread vector via Planetiler,
Copernicus GLO-30 terrain via rio-rgbify -> Terrarium). Both are stored **uncompressed**
(`--tile_compression=none`): the reader rejects gzip.

These preloaded archives disappear once worldwide on-demand tile loading lands; see the
`geo-mapdata` agent for that plan.
