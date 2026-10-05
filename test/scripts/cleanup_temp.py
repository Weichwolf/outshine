"""Remove obsolete Outshine build artifacts; retain inputs, references and active work."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


REPO = Path(__file__).resolve().parents[2]
PROTECTED = {"outshine-content", "outshine-prepared", "outshine-reference-images"}


def git(repo, *arguments):
    return subprocess.run(["git", "-C", str(repo), *arguments], text=True,
                          capture_output=True, check=True).stdout


def worktrees(repo):
    rows = []
    for block in git(repo, "worktree", "list", "--porcelain").strip().split("\n\n"):
        fields = dict(line.split(" ", 1) if " " in line else (line, "")
                      for line in block.splitlines())
        rows.append((Path(fields["worktree"]).resolve(), fields))
    return rows


def running():
    commands = subprocess.run(["ps", "-axo", "command"], text=True,
                              capture_output=True, check=True).stdout
    if shutil.which("lsof"):
        result = subprocess.run(["lsof", "-n", "-a", "-d", "cwd", "-Fn"], text=True,
                                capture_output=True, check=True)
        paths = [Path(line[1:]).resolve() for line in result.stdout.splitlines()
                 if line.startswith("n/")]
    elif Path("/proc").is_dir():
        paths = []
        for process in Path("/proc").glob("[0-9]*/cwd"):
            try:
                paths.append(process.resolve(strict=True))
            except (OSError, RuntimeError):
                continue
    else:
        raise RuntimeError("cannot inspect process working directories; cleanup refused")
    return paths, commands


def busy(path, active):
    paths, commands = active
    return any(path == p or path in p.parents for p in paths) or str(path) in commands


def owned(path):
    return not path.is_symlink() and path.stat().st_uid == os.getuid()


def disposable_worktree(repo, path, fields, roots, keep, active):
    if path in keep or not path.exists() or not owned(path) or "locked" in fields:
        return False
    if not path.name.startswith("outshine-") or not any(root in path.parents for root in roots):
        return False
    if busy(path, active) or git(path, "status", "--porcelain").strip():
        return False
    merged = subprocess.run(["git", "-C", str(repo), "merge-base", "--is-ancestor",
                             fields["HEAD"], "master"], capture_output=True)
    if merged.returncode != 0:
        return False
    build = path / "build"
    if any((build / name).exists() for name in ("sources", "cache", "content", "prepared")):
        return False
    references = build / "shots/reference"
    return not references.is_symlink() and not (
        references.exists() and any(p.is_symlink() for p in references.rglob("*")))


def old(path, hours):
    return time.time() - path.stat().st_mtime >= hours * 3600


def candidates(repo, roots, keep, active, hours):
    rows = worktrees(repo)
    result = []
    eligible = set()
    for path, fields in rows:
        if disposable_worktree(repo, path, fields, roots, keep, active) and old(path, hours):
            result.append(("worktree", path, fields))
            eligible.add(path)
        elif not path.exists() and path.name.startswith("outshine-"):
            eligible.add(path)
    obsolete_nests = {"outshine-tests." + str(p).replace("/", "_"): str(p) for p in eligible}
    for root in roots:
        if not root.exists():
            continue
        for path in root.iterdir():
            if path.name in PROTECTED or not owned(path) or busy(path, active):
                continue
            if path.name in obsolete_nests and path.is_dir():
                result.append(("tests", path, {"owner": obsolete_nests[path.name]}))
            elif path.is_dir() and path.name.startswith(("outshine-lint.", "outshine-lint-docs.")):
                if old(path, 7 * 24):
                    result.append(("report", path, {}))
            elif path.is_file() and path.name.startswith("outshine-") and path.suffix == ".log":
                if old(path, 7 * 24):
                    result.append(("log", path, {}))
        benchmark = root / "outshine-provider-review"
        if benchmark.is_dir() and owned(benchmark) and not busy(benchmark, active):
            for path in benchmark.iterdir():
                if path.is_dir() and path.name.startswith(("bulk-", "mixed-map-", "coverage-")):
                    if owned(path) and old(path, hours):
                        result.append(("benchmark", path, {}))
    return result


def preserve_references(repo, worktree):
    source = worktree / "build/shots/reference"
    if source.is_dir() and any(source.iterdir()):
        target = repo / "build/shots/reference/retired-worktrees" / worktree.name
        if target.exists():
            raise RuntimeError(f"reference archive already exists: {target}")
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(source, target)


def remove(repo, kind, path, fields, roots, keep):
    active = running()
    if not path.exists() or not owned(path) or busy(path, active):
        raise RuntimeError(f"artifact changed or is in use: {path}")
    if "owner" in fields and busy(Path(fields["owner"]), active):
        raise RuntimeError(f"artifact owner is in use: {fields['owner']}")
    if kind == "worktree":
        if not disposable_worktree(repo, path, fields, roots, keep, active):
            raise RuntimeError(f"worktree is no longer disposable: {path}")
        if git(path, "rev-parse", "HEAD").strip() != fields["HEAD"]:
            raise RuntimeError(f"worktree HEAD changed: {path}")
        preserve_references(repo, path)
        git(repo, "worktree", "remove", str(path))
    elif path.is_dir():
        shutil.rmtree(path)
    else:
        path.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true", help="delete listed obsolete artifacts")
    parser.add_argument("--keep-worktree", action="append", default=[], metavar="PATH")
    parser.add_argument("--min-age-hours", type=float, default=24)
    args = parser.parse_args()
    if args.min_age_hours < 0 or not args.min_age_hours < float("inf"):
        parser.error("minimum age must be finite and nonnegative")
    roots = {Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve()}
    keep = {REPO, *(Path(p).resolve() for p in args.keep_worktree)}
    before = shutil.disk_usage(REPO).free
    selected = candidates(REPO, roots, keep, running(), args.min_age_hours)
    failures = 0
    for kind, path, fields in selected:
        print(f"{'REMOVE' if args.apply else 'WOULD REMOVE'} {kind}: {path}", flush=True)
        if args.apply:
            try:
                remove(REPO, kind, path, fields, roots, keep)
            except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
                failures += 1
                print(f"KEPT {path}: {error}", flush=True)
    after = shutil.disk_usage(REPO).free
    print(f"{len(selected)} candidates, {failures} failures; free {before / 2**30:.2f} -> "
          f"{after / 2**30:.2f} GiB; source caches, prepared oracles and reference pins retained")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
