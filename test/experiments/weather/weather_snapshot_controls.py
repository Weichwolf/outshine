"""Judge the actual captured-weather consumer across fresh preparation/offline processes."""

import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CAPTURE = Path("test/experiments/weather/data/open-meteo")
PAYLOAD = CAPTURE / "koerbersee-20261015.json"
RECEIPT = CAPTURE / "koerbersee-20261015.receipt.json"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    probe = args.probe.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    raw = (ROOT / PAYLOAD).read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    receipt = json.loads((ROOT / RECEIPT).read_text())
    assert digest == receipt["source_sha256"]
    hourly = json.loads(raw)["hourly"]
    assert all(value == 0 for value in hourly["cloud_cover_low"])
    records = []
    failures = []

    def run(name, cwd, cache, utc, expected, prepare=False):
        command = [str(probe), str(cache), str(utc)] + (["prepare"] if prepare else [])
        process = subprocess.run(
            command, cwd=cwd, capture_output=True, text=True, timeout=30, check=False
        )
        record = {
            "name": name,
            "command": command,
            "cwd": str(cwd),
            "exit_code": process.returncode,
            "expected_exit_code": expected,
            "stdout": process.stdout,
            "stderr": process.stderr,
        }
        records.append(record)
        if process.returncode != expected:
            failures.append(name)
            return None
        return json.loads(process.stdout) if expected == 0 else None

    with tempfile.TemporaryDirectory(prefix="weather-source-", dir=output) as scratch:
        scratch = Path(scratch)
        cache = scratch / "source-cache"
        at = hourly["time"][0]
        prepared = run(
            "captured-response-preparation", ROOT, cache, at, 0, prepare=True
        )
        if prepared is not None and (
            prepared["provider_starts"] != 1
            or prepared["source_cache_hits"] != 0
            or prepared["source_sha256"] != digest
            or prepared["native_cloud_optical_depth"] != 0.0
        ):
            failures.append("preparation-identity-or-consumption")
        offline_root = scratch / "receipt-only-root"
        (offline_root / CAPTURE).mkdir(parents=True)
        shutil.copyfile(ROOT / RECEIPT, offline_root / RECEIPT)
        assert not (offline_root / PAYLOAD).exists()
        for at in hourly["time"]:
            result = run(f"offline-{at}", offline_root, cache, at, 0)
            if result is not None and (
                result["valid_utc_s"] != at
                or result["source_sha256"] != digest
                or result["low_cloud_cover"] != 0.0
                or result["native_cloud_optical_depth"] != 0.0
                or result["provider_starts"] != 0
                or result["source_cache_hits"] != 1
            ):
                failures.append(f"offline-state-or-cache-{at}")
        for at in [
            hourly["time"][0] - 3600,
            hourly["time"][0] + 1,
            hourly["time"][-1] + 3600,
            -(2**63),
            2**63 - 1,
        ]:
            run(f"absent-{at}", offline_root, cache, at, 3)
        run(
            "offline-source-miss",
            offline_root,
            scratch / "empty-cache",
            hourly["time"][0],
            4,
        )
        for value in ["", "1.5", "9223372036854775808", "-9223372036854775809"]:
            run(f"invalid-utc-{value}", offline_root, cache, value, 2)
    assert hashlib.sha256((ROOT / PAYLOAD).read_bytes()).hexdigest() == digest
    report = {
        "cases": len(records),
        "source_sha256": digest,
        "offline_source_payload_available": False,
        "failures": failures,
        "records": records,
    }
    (output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"cases": len(records), "failures": failures}))
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
