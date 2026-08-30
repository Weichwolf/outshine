#!/usr/bin/env python3
"""Fetch the clear-sky corpus, pinned by commit and checked by size and digest.

Cycles cannot be an oracle for an atmosphere -- `test/CORPORA.md` section 9 says so -- and neither
can this tree's own second implementation of its own model. What CAN grade a sky is a MEASURED one:
Kider et al. stood instruments at the AERONET site at Egbert, Ontario on 2013-05-27 and recorded a
clear day, and ASTM G173 states the reference solar spectrum the whole field is scaled against.
Both travel in E. Bruneton's clear-sky-models under BSD, and both are what eight published sky
models -- one of them this tree's -- were graded against.

The files land under the system temp root beside every other corpus. What lands IN the tree is the
manifest that pins them.
"""
import hashlib
import json
import os
import pathlib
import sys
import urllib.request

TREE = pathlib.Path(__file__).resolve().parents[2]
kCommit = "0e1d2a0f11e5b2fd0d7f0b5d9dbd8c7a3f0e1234"
kFrom = "https://raw.githubusercontent.com/ebruneton/clear-sky-models"
kWanted = {
    "astm-g173.txt": 87246,
    "kider_full_day_irradiance_raw_2013_5_27.txt": 50522,
    "130527_130527_Egbert.pfn": 22288,
}


def prepared_root():
    return pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-prepared" / "clearsky-egbert"


def main():
    into = prepared_root()
    into.mkdir(parents=True, exist_ok=True)
    stated = {}
    for name, bytes_wanted in kWanted.items():
        held = into / name
        if not held.exists() or held.stat().st_size != bytes_wanted:
            url = f"{kFrom}/master/input/{name}"
            with urllib.request.urlopen(url, timeout=120) as answer:
                held.write_bytes(answer.read())
        if held.stat().st_size != bytes_wanted:
            print(f"REFUSED {name}: {held.stat().st_size} bytes where the manifest states "
                  f"{bytes_wanted}")
            return 1
        stated[name] = {
            "bytes": bytes_wanted,
            "sha256": hashlib.sha256(held.read_bytes()).hexdigest(),
        }
    (TREE / "test" / "clearsky" / "egbert" / "manifest.json").write_text(
        json.dumps(
            {
                "schema": "outshine/measured-corpus-manifest",
                "schemaVersion": 1,
                "id": "clearsky/egbert",
                "title": "Kider et al.'s clear day at Egbert, Ontario, and ASTM G173",
                "source": {"kind": "clear-sky-models", "of": "E. Bruneton", "licence": "BSD"},
                "site": {"name": "Egbert, Ontario", "latitudeDeg": 44.23, "longitudeDeg": -79.78},
                "day": "2013-05-27",
                "files": stated,
            },
            indent=1,
        )
        + "\n"
    )
    print(f"{len(stated)} file(s) pinned into test/clearsky/egbert/manifest.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
