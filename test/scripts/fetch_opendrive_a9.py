#!/usr/bin/env python3
"""Fetch the A9 OpenDRIVE corpus, pinned by commit and checked by size and digest.

A road this tree derives from OpenStreetMap can be graded by an eye, by itself, or by a MEASUREMENT.
The first two are what `test/CORPORA.md` calls agreement with ourselves. The third exists: two
sections of the German Autobahn A9 were SURVEYED by 3D Mapping Solutions and published through the
MDM portal as open data under GeoNutzV. The Mobilithek migration broke that download, so TUM's
geoinformatics chair mirrors the files, licence PDF included.

What makes them an oracle rather than an example is that the same kilometres are ALSO IN OSM. The
derivation and the measurement describe one road, so every quantity the derivation has to guess --
width, gradient, superelevation, where the reference line runs -- has a surveyed answer beside it.

The files land under the system temp root beside every other corpus. What lands IN the tree is the
manifest that pins them.
"""
import hashlib
import json
import os
import pathlib
import urllib.parse
import urllib.request

TREE = pathlib.Path(__file__).resolve().parents[2]
kCommit = "e75ee549659a277b55116d30561561720438fa29"
kFrom = "https://raw.githubusercontent.com/tum-gis/opendrive-testfeld-a9"
kWanted = {
    "2017-04-04_Testfeld_A9_Nord.xodr": 10808489,
    "2017-04-04_Testfeld_A9_Sued.xodr": 9766787,
    "GeoNutzV_130319.pdf": 48713,
}


def prepared_root():
    return pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-prepared" / "opendrive-a9"


def main():
    into = prepared_root()
    into.mkdir(parents=True, exist_ok=True)
    stated = {}
    for name, bytes_wanted in kWanted.items():
        held = into / name
        if not held.exists() or held.stat().st_size != bytes_wanted:
            url = f"{kFrom}/{kCommit}/{urllib.parse.quote(name)}"
            with urllib.request.urlopen(url, timeout=600) as answer:
                held.write_bytes(answer.read())
        if held.stat().st_size != bytes_wanted:
            print(f"REFUSED {name}: {held.stat().st_size} bytes where the manifest states "
                  f"{bytes_wanted}")
            return 1
        stated[name] = {
            "bytes": bytes_wanted,
            "sha256": hashlib.sha256(held.read_bytes()).hexdigest(),
        }
    (TREE / "test" / "opendrive" / "a9" / "manifest.json").write_text(
        json.dumps(
            {
                "schema": "outshine/measured-corpus-manifest",
                "schemaVersion": 1,
                "id": "opendrive/a9",
                "title": "Two surveyed sections of the German Autobahn A9, in ASAM OpenDRIVE 1.4",
                "source": {
                    "kind": "opendrive-testfeld-a9",
                    "of": "3D Mapping Solutions, via the MDM portal, mirrored by tum-gis",
                    "licence": "GeoNutzV",
                    "commit": kCommit,
                },
                "grade": "TRUTH",
                "surveyed": "2017-04-04",
                "georeference":
                    "+proj=tmerc +lat_0=0 +lon_0=9 +k=0.9996 +x_0=500000 +y_0=0 "
                    "+datum=WGS84 +units=m +no_defs",
                "files": stated,
            },
            indent=1,
        ) + "\n")
    print(f"{len(stated)} file(s) pinned into test/opendrive/a9/manifest.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
