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
from prep import vendor  # noqa: E402
from prep import test262 as test262_module  # noqa: E402
from prep import wpt as wpt_module  # noqa: E402
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
CASE_TREES = ("test/khronos/glTF", "test/khronos/generator", "test/outshine/render",
              "test/wpt/css", "test/test262/js")


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
                                        "dry-run", "wpt-cases", "test262-cases", "generator-cases"))
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
    parser.add_argument("--wpt-commit", default=None, help="wpt-cases: the pin every case records")
    parser.add_argument("--wpt-directory", default=None, help="wpt-cases: the upstream directory, e.g. css/css-flexbox")
    parser.add_argument("--wpt-root", default=None, help="wpt-cases: where the case directories are written")
    parser.add_argument("--wpt-viewport", default="800x600")
    parser.add_argument("--wpt-pinned-on", default=None, help="wpt-cases: the date the pin was taken")
    parser.add_argument("--wpt-pin-reason", default=None)
    parser.add_argument("--t262-commit", default=None, help="test262-cases: the pin every case records")
    parser.add_argument("--t262-root", default=None)
    parser.add_argument("--t262-pinned-on", default=None)
    parser.add_argument("--t262-pin-reason", default=None)
    parser.add_argument("--gen-commit", default=None, help="generator-cases: the pin every case records")
    parser.add_argument("--gen-root", default=None, help="generator-cases: where the case directories are written")
    parser.add_argument("--gen-pinned-on", default=None)
    parser.add_argument("--gen-pin-reason", default=None)
    arguments = parser.parse_args(argv)

    if arguments.job == "wpt-cases":
        return _wpt_cases(arguments)
    if arguments.job == "test262-cases":
        return _test262_cases(arguments)
    if arguments.job == "generator-cases":
        return _generator_cases(arguments)

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


def _wpt_cases(arguments):
    """Turn an upstream directory at the pin into one case directory each, and report both counts."""
    for name in ("wpt_commit", "wpt_directory", "wpt_root", "wpt_pinned_on", "wpt_pin_reason"):
        if getattr(arguments, name) is None:
            raise Refusal("wpt-cases", expected="--" + name.replace("_", "-"), observed="absent")
    width, _, height = arguments.wpt_viewport.partition("x")
    viewport = (int(width), int(height))
    tests = wpt_module.tests_in(arguments.wpt_commit, arguments.wpt_directory)
    written, stated_nothing = [], 0
    for test in tests:
        declared = wpt_module.case(arguments.wpt_commit, arguments.wpt_directory, test,
                                   arguments.wpt_pinned_on, arguments.wpt_pin_reason, viewport)
        if declared is None:
            stated_nothing += 1
            continue
        written.append(wpt_module.write(declared, arguments.wpt_root))
    _emit({"directory": arguments.wpt_directory, "commit": arguments.wpt_commit,
           "tests": len(tests), "cases": len(written),
           "statesNoLayoutOfItsOwn": stated_nothing, "root": arguments.wpt_root})
    return 0


def _generator_cases(arguments):
    """The generator's animation groups at the pin, one case directory each (board:1458)."""
    for name in ("gen_commit", "gen_root", "gen_pinned_on", "gen_pin_reason"):
        if getattr(arguments, name) is None:
            raise Refusal("generator-cases", expected="--" + name.replace("_", "-"), observed="absent")
    # THE IMPORTER IS THE VENDOR'S, found the same way its fetch step is: by position under
    # `test/harness/`, so a corpus that adds one adds a directory and nothing else (board:1469).
    generator_module = vendor.at(os.path.join(vendor.harness_of(arguments.gen_root), "prepare",
                                              "import_cases.py"))
    written, groups = [], {}
    for group in generator_module.GROUPS:
        stated = generator_module.says(arguments.gen_commit, group)
        models = generator_module.models_in(arguments.gen_commit, group)
        groups[group] = len(models)
        for name, files, materials, animates, moves, ends in models:
            declared = generator_module.case(
                arguments.gen_commit, group, name, files, materials, animates, moves, ends, stated,
                arguments.gen_pinned_on, arguments.gen_pin_reason,
                generator_module.measured_fraction(arguments.gen_root, name))
            written.append(generator_module.write(declared, arguments.gen_root, name))
    _emit({"commit": arguments.gen_commit, "groups": groups, "cases": len(written),
           "root": arguments.gen_root})
    return 0


def _test262_cases(arguments):
    """Turn every declared container at the pin into one case directory each."""
    for name in ("t262_commit", "t262_root", "t262_pinned_on", "t262_pin_reason"):
        if getattr(arguments, name) is None:
            raise Refusal("test262-cases", expected="--" + name.replace("_", "-"), observed="absent")
    tests = test262_module.tests_at(arguments.t262_commit)
    written = []
    for test in tests:
        declared = test262_module.case(arguments.t262_commit, test, arguments.t262_pinned_on,
                                       arguments.t262_pin_reason)
        written.append(test262_module.write(declared, arguments.t262_root))
    _emit({"containers": len(test262_module.CONTAINERS), "commit": arguments.t262_commit,
           "tests": len(tests), "cases": len(written), "root": arguments.t262_root})
    return 0


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

    # A DOCUMENT CASE HAS NOTHING TO ASK BLENDER, AND THAT IS A PROPERTY OF THE CASE RATHER THAN A
    # FLAG ON THE RUN. `all` over the whole corpus reaches both suites, so the oracle jobs are the ones
    # a case without an oracle simply does not have; refusing here would make `--every-case` unusable
    # the day a second kind of case arrived, which is today.
    blender = None
    if declared.oracle and arguments.job in ("convert", "render", "all"):
        blender = blender_module.Blender(blender_module.locate(arguments.blender))
        notice = blender.against(declared.blender_version)
        if notice:
            print("notice: " + notice + " -- recorded, not refused", file=sys.stderr)

    needs_conversion = declared.oracle and any(subject.kind == "blend" for subject in declared.subjects)
    if declared.oracle and (arguments.job == "convert" or (arguments.job == "all" and needs_conversion)):
        report["convert"] = jobs.convert_blends(declared, store, blender, destination, force=arguments.force)

    if declared.oracle and arguments.job in ("render", "all"):
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
