"""Check actual ELF/Mach-O namespace closure and an omitted implementation."""

import os
import shlex
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RUNNER = (ROOT / "test/run.sh").read_text()


class AuditSymbols(unittest.TestCase):
    def test_both_object_formats_require_the_missing_source(self):
        nm = shutil.which(os.environ.get("NM", "llvm-nm"))
        if not nm and not os.environ.get("NM"):
            nm = "/opt/homebrew/opt/llvm/bin/llvm-nm"
        self.assertTrue(nm and Path(nm).is_file(), "set NM to the installed LLVM nm")
        standard = next(
            line.split("=", 1)[1]
            for line in RUNNER.splitlines()
            if line.startswith("CXXSTD=")
        )
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        start = RUNNER.index("    nm -u $OBJECTS")
        end = RUNNER.index('    unresolved=$(comm -23', start)
        command = RUNNER[start:end]
        with tempfile.TemporaryDirectory(prefix="outshine-audit-symbols-") as directory:
            root = Path(directory)
            sources = [root / "consumer.cpp", root / "implementation.cpp"]
            sources[0].write_text(
                "namespace outshine { int Missing(); "
                "int Reference() { return Missing(); } "
                "struct Value { int Missing() const; }; "
                "int Read(const Value &value) { return value.Missing(); } }\n"
                "namespace other { int Outside(); "
                "int Reference() { return Outside(); } }\n"
            )
            sources[1].write_text(
                "namespace outshine { int Missing() { return 9; } "
                "struct Value { int Missing() const; }; "
                "int Value::Missing() const { return 7; } }\n"
            )
            for triple in ["aarch64-apple-macosx14.0", "x86_64-unknown-linux-gnu"]:
                with self.subTest(triple=triple):
                    objects = [root / (source.stem + ".o") for source in sources]
                    for source, object in zip(sources, objects):
                        subprocess.run(
                            [
                                *compiler,
                                standard,
                                "-target",
                                triple,
                                "-c",
                                str(source),
                                "-o",
                                str(object),
                            ],
                            check=True,
                            capture_output=True,
                            text=True,
                        )
                    for complete in [False, True]:
                        held = objects if complete else objects[:1]
                        environment = {
                            **os.environ,
                            "OUTSHINE_NM": nm,
                            "BUILD_PLATFORM": "Darwin" if "apple" in triple else "Linux",
                            "BUILD": str(root),
                            "OBJECTS": " ".join(str(object) for object in held),
                        }
                        subprocess.run(
                            [
                                "sh",
                                "-c",
                                'nm() { "$OUTSHINE_NM" "$@"; }; ' + command,
                            ],
                            env=environment,
                            check=True,
                            capture_output=True,
                            text=True,
                        )
                        unresolved = set(
                            (root / "audit.undef").read_text().splitlines()
                        ) - set((root / "audit.def").read_text().splitlines())
                        unresolved = {symbol.lstrip("_") for symbol in unresolved}
                        self.assertEqual(
                            unresolved,
                            set() if complete else {
                                "ZN8outshine7MissingEv",
                                "ZNK8outshine5Value7MissingEv",
                            },
                        )


if __name__ == "__main__":
    unittest.main()
