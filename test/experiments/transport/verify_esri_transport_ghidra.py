"""Fresh nonce, exact ELF bounds and complete hashed exports; never regenerate pins."""

import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from esri_transport_catalog import BINARY_SHA256, CORE, catalog
from esri_transport_ghidra import inspect, read_analysis, script_source


class ExportChecks(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="outshine-tests.transport-ghidra.")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.targets = dict(probe=dict(elf_address=32, elf_symbol_bytes=8))
        self.export = self.root / "reference-00100020.c"
        self.export.write_text("void probe() {}\n")
        self.row = dict(name="probe", address="00100020", elf_symbol_bytes=8, body_bytes=8, instructions=5,
                        decoded_instruction_bytes=8, control_flow_blocks_outside_symbol=0, decompile_completed=True,
                        external_reference_file=self.export.name, external_reference_sha256=hashlib.sha256(self.export.read_bytes()).hexdigest())
        self.report = dict(schema_version=3, run_id="fresh", binary_sha256=BINARY_SHA256, image_base="00100000", selected_functions=[self.row])

    def verify(self):
        (self.root / "ghidra-core.json").write_text(json.dumps(self.report))
        return read_analysis(self.root, self.targets, "fresh", BINARY_SHA256)

    def test_complete_bounded_fresh_export(self):
        self.assertEqual(len(self.verify()["selected_functions"]), 1)

    def test_stale_nonce_foreign_binary_and_missing_symbols(self):
        for key, value in (("run_id", "old"), ("binary_sha256", "0" * 64), ("selected_functions", [])):
            with self.subTest(key=key):
                original = self.report[key]
                self.report[key] = value
                with self.assertRaises(ValueError):
                    self.verify()
                self.report[key] = original

    def test_bounds_decode_and_hash_failures(self):
        for key, value in (("address", "00100021"), ("elf_symbol_bytes", 9), ("body_bytes", 9),
                           ("control_flow_blocks_outside_symbol", 1), ("decoded_instruction_bytes", 9),
                           ("decompile_completed", False), ("instructions", 0), ("external_reference_sha256", "0" * 64)):
            with self.subTest(key=key):
                original = self.row[key]
                self.row[key] = value
                with self.assertRaises(ValueError):
                    self.verify()
                self.row[key] = original
        self.export.write_text("")
        with self.assertRaises(ValueError):
            self.verify()

    def test_zero_exit_without_fresh_export_keeps_previous_output(self):
        class SuccessfulLauncher:
            returncode = 0
            def __init__(self, *args, **kwargs):
                pass
            def wait(self, timeout):
                return 0
        core = self.root / "fake-core.so"
        core.write_bytes(b"success without export")
        output = self.root / "previous"
        output.mkdir()
        previous = output / "ghidra-core.json"
        previous.write_text('{"old":true}\n')
        inventory = dict(binary_sha256=BINARY_SHA256, selected_symbols=self.targets)
        with patch("esri_transport_ghidra.catalog", return_value=inventory):
            with self.assertRaisesRegex(RuntimeError, "fresh script export"):
                inspect(core, output, launcher=SuccessfulLauncher)
        self.assertEqual(previous.read_text(), '{"old":true}\n')


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, default=Path("build/experiments/transport/esri-reference/ghidra"))
    parser.add_argument("--checks-only", action="store_true")
    args = parser.parse_args()
    result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(ExportChecks))
    if not result.wasSuccessful():
        raise SystemExit(1)
    if not args.checks_only:
        manifest = json.loads((args.reference / "ghidra-core.json").read_text())
        inventory = catalog(CORE)
        read_analysis(args.reference, inventory["selected_symbols"], manifest["run_id"], inventory["binary_sha256"])
        for row in manifest["selected_functions"]:
            if not row.get("external_reference_sha256") or not row.get("assembly_reference_file"):
                raise ValueError("published inspection lacks complete hashed C/assembly exports")
            assembly = args.reference / row["assembly_reference_file"]
            if not assembly.is_file() or hashlib.sha256(assembly.read_bytes()).hexdigest() != row.get("assembly_reference_sha256"):
                raise ValueError("missing or changed symbol-range assembly export")
        if manifest["script_sha256"] != hashlib.sha256(script_source().encode()).hexdigest():
            raise ValueError("inspection script changed; require a fresh explicit export")
        print(json.dumps(dict(status="pass", selected=len(manifest["selected_functions"]), stale_success_rejected=True)))
