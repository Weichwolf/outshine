#!/usr/bin/env python3
"""Offline asset preparation for the glTF render ladder. Never a test, a gate or a build step."""

import argparse
import json
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from prep import blender as blender_module  # noqa: E402
from prep import jobs, manifest as manifest_module  # noqa: E402
from prep.refusal import Refusal  # noqa: E402
from prep.store import ContentStore  # noqa: E402


PREPARED_LEAF = "outshine-prepared"


def prepared_directory(manifest_path):
    """Where a case's fetched, generated, converted and rendered files go, and it is NOT the tree.

    `CLAUDE.md`: every artefact goes to the system temp directory, never into the tree -- a repository
    is what is declared and what is built from it. The leaf is the case's own path with its separators
    flattened, so two cases cannot collide and the mapping is derivable from either end without a
    table."""
    case = os.path.dirname(os.path.abspath(manifest_path))
    root = os.path.abspath(os.curdir)
    leaf = os.path.relpath(case, root) if case.startswith(root + os.sep) else os.path.basename(case)
    return os.path.join(tempfile.gettempdir(), PREPARED_LEAF, leaf.replace(os.sep, "-"))


# WHERE THE CASES ARE, AND THE TWO SUITES ARE NAMED RATHER THAN DISCOVERED. A walk that found a
# `manifest.json` anywhere would prepare whatever a future directory happened to contain; these two are
# the declarative suites and adding a third is a decision, not a side effect.
CASE_TREES = ("test/khronos/glTF", "test/outshine/render")


def every_manifest():
    """Every case's declaration, in a stable order so two runs report the same list."""
    found = []
    for tree in CASE_TREES:
        for here, _, files in os.walk(tree):
            if "manifest.json" in files:
                found.append(os.path.join(here, "manifest.json"))
    return sorted(found)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("job", choices=("fetch", "generate", "patch", "convert", "render", "all",
                                        "dry-run"))
    parser.add_argument("--manifest", action="append", default=None,
                        help="a case's manifest; repeatable")
    parser.add_argument("--every-case", action="store_true",
                        help="every manifest under " + " and ".join(CASE_TREES))
    parser.add_argument("--dest", default=None,
                        help="default: this case's directory under the system temp root")
    parser.add_argument("--store", default=None, help="default: the library's content store")
    parser.add_argument("--blender", default=None)
    parser.add_argument("--recipe", action="append", default=None, help="render only this recipe; repeatable")
    parser.add_argument("--force", action="store_true", help="redo the work even on a cache hit")
    parser.add_argument("--no-cache", action="store_true")
    arguments = parser.parse_args(argv)

    manifests = list(arguments.manifest or [])
    if arguments.every_case:
        manifests += [one for one in every_manifest() if one not in manifests]
    if not manifests:
        parser.error("name at least one --manifest, or --every-case")

    # ONE CASE IS THE UNIT AND A SWEEP IS A LOOP OVER IT, so a refusal names its case and the rest of
    # the corpus still gets prepared. The exit status is what says whether ANY of them refused --
    # a sweep that reported a refusal and exited zero would be the silent skip this tree keeps removing.
    if len(manifests) > 1:
        refused = []
        for one in manifests:
            try:
                _prepare(one, arguments)
            except Refusal as refusal:
                refused.append(one)
                print("refused: " + one, file=sys.stderr)
                print(str(refusal), file=sys.stderr)
        print("prepared %d of %d cases" % (len(manifests) - len(refused), len(manifests)),
              file=sys.stderr)
        for one in refused:
            print("  refused " + one, file=sys.stderr)
        return 1 if refused else 0
    return _prepare(manifests[0], arguments)


def _prepare(manifest_path, arguments):
    declared = manifest_module.load(manifest_path)
    destination = arguments.dest or prepared_directory(manifest_path)
    os.makedirs(destination, exist_ok=True)
    # THE RUNNER IS GIVEN ONE DIRECTORY AND READS THE DECLARATION FROM IT, so the manifest is copied
    # beside what it produced. The copy is rewritten on every preparation and nothing ever edits it,
    # which is what keeps the tracked file the only authority.
    if os.path.abspath(destination) != os.path.abspath(declared.directory):
        shutil.copyfile(manifest_path, os.path.join(destination, "manifest.json"))
    store = ContentStore(arguments.store, enabled=not arguments.no_cache)

    if arguments.job == "dry-run":
        _emit({"manifest": declared.id, "destination": destination, "store": store.directory,
               "plan": jobs.plan(declared, store)})
        return 0

    report = {}
    if arguments.job in ("fetch", "all"):
        report["fetch"] = jobs.fetch_subjects(declared, store, destination, force=arguments.force)

    needs_generation = any(subject.kind == "generated" for subject in declared.subjects)
    if arguments.job == "generate" or (arguments.job == "all" and needs_generation):
        report["generate"] = jobs.generate_subjects(declared, destination)

    # BETWEEN THE FETCH AND EVERYTHING THAT READS THE FILES. The fetch verifies upstream's digest and
    # restores the pristine bytes; the patch is stated on top of them, so the order is what keeps the
    # pin exact and the correction reproducible.
    needs_patching = any(subject.patch for subject in declared.subjects)
    if arguments.job == "patch" or (arguments.job == "all" and needs_patching):
        report["patch"] = jobs.patch_subjects(declared, destination)

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
