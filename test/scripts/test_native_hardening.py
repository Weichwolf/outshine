"""Exercise native hardening with the selected compiler and real executables."""

import os
import platform
import resource
import shlex
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HELPER = ROOT / 'test/scripts/native_hardening.sh'
STANDARD = next(line.partition('=')[2] for line in (ROOT / 'test/run.sh').read_text().splitlines()
                if line.startswith('CXXSTD='))


class NativeHardening(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='outshine-native-hardening-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.compiler = shlex.split(os.environ.get('CXX', 'c++'))
        self.compile_flags = self.flags('--compile')
        self.link_flags = self.flags('--link')

    def flags(self, mode):
        return shlex.split(subprocess.check_output(['sh', str(HELPER), mode], text=True))

    def compile(self, source, name='probe', success=True):
        path = self.root / (name + '.cpp')
        path.write_text(source)
        binary = self.root / name
        result = subprocess.run(self.compiler + [STANDARD, '-O2', '-Werror'] +
                                self.compile_flags + [str(path)] + self.link_flags +
                                ['-o', str(binary)], capture_output=True, text=True, check=False)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, 'unsafe format unexpectedly compiled')
            self.assertIn('format-security', result.stderr)
        return binary

    def test_library_selection_and_runtime_bounds(self):
        binary = self.compile('''
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
''')
        self.assertEqual(subprocess.run([str(binary)], check=False).returncode, 0)
        failed = subprocess.run([str(binary), 'out-of-bounds'], capture_output=True, check=False)
        self.assertNotEqual(failed.returncode, 0, 'bounds violation was not trapped')

    def test_nonliteral_format_is_rejected(self):
        self.compile('extern int log_format(const char*, ...) __attribute__((format(printf, 1, 2)));\n'
                     'int main(int, char** v) { return log_format(v[0]); }',
                     name='unsafe-format', success=False)

    @unittest.skipUnless(platform.system() == 'Linux', 'ELF is Linux-specific')
    def test_linux_binary_and_fortification(self):
        binary = self.compile('''
#include <cstdio>
#include <cstring>
#if _FORTIFY_SOURCE != 3 || !defined(__PIE__)
#error Linux hardening missing
#endif
#if (defined(__x86_64__) || defined(__i386__)) && __CET__ != 3
#error x86 control-flow instrumentation missing
#endif
int main(int argc, char** argv) {
    char buffer[64];
    std::strcpy(buffer, argc > 1 ? argv[1] : "safe");
    return std::puts(buffer) < 0;
}
''', name='elf-probe')
        headers = subprocess.check_output(['readelf', '-hW', str(binary)], text=True)
        segments = subprocess.check_output(['readelf', '-lW', str(binary)], text=True)
        dynamic = subprocess.check_output(['readelf', '-dW', str(binary)], text=True)
        symbols = subprocess.check_output(['readelf', '-sW', str(binary)], text=True)
        self.assertRegex(headers, r'Type:\s+DYN')
        self.assertIn('GNU_RELRO', segments)
        stack = next(line for line in segments.splitlines() if 'GNU_STACK' in line)
        self.assertNotIn('E', stack.split()[6])
        self.assertIn('BIND_NOW', dynamic)
        self.assertIn('PIE', dynamic)
        self.assertIn('__stack_chk_fail', symbols)
        self.assertIn('__strcpy_chk', symbols)
        self.assertEqual(subprocess.run([str(binary)], capture_output=True, check=False).returncode, 0)
        failed = subprocess.run([str(binary), 'x' * 128], capture_output=True, check=False)
        self.assertNotEqual(failed.returncode, 0, 'fortified overflow was not trapped')

    def test_unusable_compiler_is_an_error(self):
        env = dict(os.environ, CXX='/nonexistent/outshine-compiler')
        result = subprocess.run(['sh', str(HELPER), '--compile'], env=env,
                                capture_output=True, check=False)
        self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    unittest.main()
