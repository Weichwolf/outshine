#!/usr/bin/env python3
"""Offline asset preparation for the glTF render ladder. Never a test, a gate or a build step."""

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from prep import blender as blender_module  # noqa: E402
from prep import jobs, manifest as manifest_module  # noqa: E402
from prep.refusal import Refusal  # noqa: E402
from prep.store import ContentStore  # noqa: E402


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("job", choices=("fetch", "convert", "render", "all", "dry-run"))
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--dest", default=None, help="default: the manifest's own directory")
    parser.add_argument("--store", default=None, help="default: the library's content store")
    parser.add_argument("--blender", default=None)
    parser.add_argument("--recipe", action="append", default=None, help="render only this recipe; repeatable")
    parser.add_argument("--force", action="store_true", help="redo the work even on a cache hit")
    parser.add_argument("--no-cache", action="store_true")
    arguments = parser.parse_args(argv)

    declared = manifest_module.load(arguments.manifest)
    destination = arguments.dest or declared.directory
    os.makedirs(destination, exist_ok=True)
    store = ContentStore(arguments.store, enabled=not arguments.no_cache)

    if arguments.job == "dry-run":
        _emit({"manifest": declared.id, "destination": destination, "store": store.directory,
               "plan": jobs.plan(declared, store)})
        return 0

    report = {}
    if arguments.job in ("fetch", "all"):
        report["fetch"] = jobs.fetch_subjects(declared, store, destination, force=arguments.force)

    blender = None
    if arguments.job in ("convert", "render", "all"):
        blender = blender_module.Blender(blender_module.locate(arguments.blender))
        notice = blender.against(declared.blender_version)
        if notice:
            print("notice: " + notice + " -- recorded, not refused", file=sys.stderr)

    needs_conversion = any(subject.kind == "blend" for subject in declared.subjects)
    if arguments.job == "convert" or (arguments.job == "all" and needs_conversion):
        report["convert"] = jobs.convert_blends(declared, store, blender, destination, force=arguments.force)

    if arguments.job in ("render", "all"):
        report["render"] = jobs.render_oracle(
            declared, store, blender, destination, only=arguments.recipe, force=arguments.force
        )

    report["store"] = {"directory": store.directory, "hits": store.hits, "misses": store.misses,
                       "writes": store.writes}
    jobs.write_ignore(declared, destination)
    provenance = jobs.write_provenance(declared, store, destination, blender, report)
    _emit({"manifest": declared.id, "destination": destination, "provenance": provenance, "report": report})
    return 0


def _emit(document):
    json.dump(document, sys.stdout, indent=2, sort_keys=True)
    sys.stdout.write("\n")


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except Refusal as refusal:
        print(str(refusal), file=sys.stderr)
        sys.exit(1)
