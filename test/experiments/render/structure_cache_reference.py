"""Export one cached native building mesh without downloads or generation."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

from terrain_cache_reference import ROOT, compile_helper

SOURCE = ROOT / "test/scripts/structure-geometry-export.cpp"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cache", type=Path)
    parser.add_argument("key")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    binary, command = compile_helper(args.output, SOURCE, "StructureArtifact.cpp")
    result = subprocess.run(
        [str(binary), str(args.cache), args.key, str(args.output)],
        capture_output=True,
        text=True,
        check=True,
        timeout=60,
    )
    receipt = {
        "asset_key": args.key,
        "compile_command": command,
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "exporter_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
        "exit": result.returncode,
        "geometry": result.stdout.strip(),
    }
    (args.output / "export.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(result.stdout.strip())


if __name__ == "__main__":
    main()
