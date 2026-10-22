"""Exercise native hardening with the selected compiler and real executables."""

import json
import os
import platform
import resource
import shlex
import signal
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HELPER = ROOT / "test/scripts/native_hardening.sh"
STANDARD = next(
    line.partition("=")[2]
    for line in (ROOT / "test/run.sh").read_text().splitlines()
    if line.startswith("CXXSTD=")
)


class NativeHardening(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="outshine-native-hardening-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.compiler = shlex.split(os.environ.get("CXX", "c++"))
        self.compile_flags = self.flags("--compile")
        self.link_flags = self.flags("--link")

    def flags(self, mode):
        return shlex.split(
            subprocess.check_output(["sh", str(HELPER), mode], text=True)
        )

    def compile(self, source, name="probe", success=True, extra_flags=()):
        path = self.root / (name + ".cpp")
        path.write_text(source)
        binary = self.root / name
        result = subprocess.run(
            self.compiler
            + [STANDARD, "-O2", "-Werror"]
            + self.compile_flags
            + list(extra_flags)
            + [str(path)]
            + self.link_flags
            + ["-o", str(binary)],
            capture_output=True,
            text=True,
            check=False,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(
                result.returncode, 0, "unsafe format unexpectedly compiled"
            )
            self.assertIn("format-security", result.stderr)
        return binary

    def check_guard_trap(self, result):
        self.assertIn(
            -result.returncode,
            (signal.SIGABRT, signal.SIGILL, signal.SIGTRAP),
            "hardening did not terminate through a guard trap",
        )

    def test_docs_resolve_shared_compile_and_link_hardening(self):
        compiler = self.root / "compiler"
        captured = self.root / "compiler.jsonl"
        compiler.write_text(
            "#!/usr/bin/env python3\n"
            "import json, subprocess, sys\n"
            f"with open({str(captured)!r}, 'a') as stream:\n"
            "    stream.write(json.dumps(sys.argv[1:]) + '\\n')\n"
            f"sys.exit(subprocess.call({self.compiler!r} + sys.argv[1:]))\n"
        )
        compiler.chmod(0o755)
        result = subprocess.run(
            ["sh", "test/lint-docs.sh"],
            cwd=ROOT,
            env=dict(os.environ, CXX=str(compiler)),
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        commands = [json.loads(line) for line in captured.read_text().splitlines()]
        invocations = [
            a
            for a in commands
            if any(v.endswith("TheMapCitesLinesThatSayWhatItClaims.cpp") for v in a)
        ]
        self.assertEqual(len(invocations), 1)
        for flag in (
            self.compile_flags
            + self.link_flags
            + [STANDARD, "-O2", "-ffp-contract=off"]
        ):
            self.assertIn(flag, invocations[0])
        self.assertFalse(any("$NATIVE_" in a for a in invocations[0]))

    def test_library_selection_and_runtime_bounds(self):
        binary = self.compile("""
#include <vector>
#if defined(__GLIBCXX__)
#if !defined(_GLIBCXX_ASSERTIONS) || defined(_LIBCPP_HARDENING_MODE)
#error wrong libstdc++ hardening
#endif
#elif defined(_LIBCPP_VERSION)
#if defined(_GLIBCXX_ASSERTIONS) || _LIBCPP_HARDENING_MODE != _LIBCPP_HARDENING_MODE_FAST
#error wrong libc++ hardening
#endif
#else
#error untested standard library
#endif
int main(int argc, char**) {
    std::vector<int> values{7};
    return values[static_cast<unsigned>(argc - 1)] == 7 ? 0 : 1;
}
""")
        self.assertEqual(subprocess.run([str(binary)], check=False).returncode, 0)
        failed = subprocess.run(
            [str(binary), "out-of-bounds"], capture_output=True, check=False
        )
        self.check_guard_trap(failed)

    def test_nonliteral_format_is_rejected(self):
        self.compile(
            "extern int log_format(const char*, ...) __attribute__((format(printf, 1, 2)));\n"
            "int main(int, char** v) { return log_format(v[0]); }",
            name="unsafe-format",
            success=False,
        )

    def test_address_sanitizer_retains_real_heap_instrumentation(self):
        binary = self.compile(
            """
#include <cstdlib>
#if defined(__APPLE__) && _FORTIFY_SOURCE != 0
#error compiler sanitizer fortify mode must be preserved
#endif
int main(int argc, char**) {
    volatile char* bytes = static_cast<char*>(std::malloc(8));
    bytes[argc == 1 ? 0 : 8] = 'x';
    const int result = bytes[0] == 'x' ? 0 : 1;
    std::free(const_cast<char*>(bytes));
    return result;
}
""",
            name="asan-probe",
            extra_flags=[
                *self.flags("--address-sanitizer"),
                "-fsanitize=address,undefined",
            ],
        )
        self.assertEqual(
            subprocess.run([str(binary)], capture_output=True, check=False).returncode,
            0,
        )
        failed = subprocess.run(
            [str(binary), "overflow"], capture_output=True, text=True, check=False
        )
        self.assertNotEqual(failed.returncode, 0)
        self.assertIn("AddressSanitizer: heap-buffer-overflow", failed.stderr)

    def overflow_probe(self, fortify):
        return self.compile(
            """
#include <cstdio>
#include <cstring>
#if _FORTIFY_SOURCE != FORTIFY_LEVEL || (defined(__linux__) && !defined(__PIE__))
#error native hardening missing
#endif
#if defined(__linux__) && (defined(__x86_64__) || defined(__i386__)) && __CET__ != 3
#error x86 control-flow instrumentation missing
#endif
int main(int argc, char** argv) {
    char buffer[64];
    std::strcpy(buffer, argc > 1 ? argv[1] : "safe");
    return std::puts(buffer) < 0;
}
""".replace("FORTIFY_LEVEL", str(fortify)),
            name="overflow-probe",
        )

    def check_overflow_traps(self, binary):
        self.assertEqual(
            subprocess.run([str(binary)], capture_output=True, check=False).returncode,
            0,
        )
        failed = subprocess.run(
            [str(binary), "x" * 128], capture_output=True, check=False
        )
        self.check_guard_trap(failed)

    @unittest.skipUnless(platform.system() == "Linux", "ELF is Linux-specific")
    def test_linux_binary_and_fortification(self):
        binary = self.overflow_probe(3)
        headers = subprocess.check_output(["readelf", "-hW", str(binary)], text=True)
        segments = subprocess.check_output(["readelf", "-lW", str(binary)], text=True)
        dynamic = subprocess.check_output(["readelf", "-dW", str(binary)], text=True)
        symbols = subprocess.check_output(["readelf", "-sW", str(binary)], text=True)
        self.assertRegex(headers, r"Type:\s+DYN")
        self.assertIn("GNU_RELRO", segments)
        stack = next(line for line in segments.splitlines() if "GNU_STACK" in line)
        self.assertNotIn("E", stack.split()[6])
        self.assertIn("BIND_NOW", dynamic)
        self.assertIn("PIE", dynamic)
        self.assertIn("__stack_chk_fail", symbols)
        self.assertIn("__strcpy_chk", symbols)
        self.check_overflow_traps(binary)

    @unittest.skipUnless(platform.system() == "Darwin", "Mach-O is Darwin-specific")
    def test_apple_cpp_binary_and_stack_protection(self):
        binary = self.overflow_probe(2)
        headers = subprocess.check_output(["otool", "-hv", str(binary)], text=True)
        symbols = subprocess.check_output(["nm", "-u", str(binary)], text=True)
        self.assertIn("EXECUTE", headers)
        self.assertIn("PIE", headers)
        self.assertIn("__stack_chk_fail", symbols)
        self.check_overflow_traps(binary)

    def test_unusable_compiler_is_an_error(self):
        env = dict(os.environ, CXX="/nonexistent/outshine-compiler")
        result = subprocess.run(
            ["sh", str(HELPER), "--compile"], env=env, capture_output=True, check=False
        )
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    unittest.main()
