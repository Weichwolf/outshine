import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class FileStamps(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='outshine-file-stamps-')
        self.directory = Path(self.temp.name)
        source = (ROOT / 'test/run.sh').read_text()
        function = re.search(r'^FileStamp\(\) \{\n.*?^\}\n', source, re.M | re.S)
        self.assertIsNotNone(function)
        self.helper = self.directory / 'stamp.sh'
        self.helper.write_text(function.group() + '\nFileStamp "$@"\n')
        self.file = self.directory / 'source with spaces.cpp'
        self.file.write_text('input\n')

    def stamp(self, *files):
        return subprocess.run(['sh', str(self.helper), *map(str, files)],
                              capture_output=True, text=True, timeout=5)

    def test_host_metadata_has_the_existing_fields(self):
        second = self.directory / 'other.h'
        second.write_text('other\n')
        result = self.stamp(self.file, second)
        self.assertEqual(result.returncode, 0, result.stderr)
        expected = ''.join(f'{int(path.stat().st_mtime)}|{path.stat().st_size}|{path}\n'
                           for path in (self.file, second))
        self.assertEqual(result.stdout, expected)

    def test_missing_input_is_an_error(self):
        result = self.stamp(self.file, self.directory / 'missing.cpp')
        self.assertNotEqual(result.returncode, 0)

    def test_backwards_mtime_changes_the_stamp(self):
        before = self.stamp(self.file)
        metadata = self.file.stat()
        os.utime(self.file, (metadata.st_atime, int(metadata.st_mtime) - 100))
        after = self.stamp(self.file)
        self.assertEqual(before.returncode, 0, before.stderr)
        self.assertEqual(after.returncode, 0, after.stderr)
        self.assertNotEqual(before.stdout, after.stdout)

    def test_size_change_changes_the_stamp(self):
        before = self.stamp(self.file)
        self.file.write_text('different length\n')
        after = self.stamp(self.file)
        self.assertEqual(before.returncode, 0, before.stderr)
        self.assertEqual(after.returncode, 0, after.stderr)
        self.assertNotEqual(before.stdout, after.stdout)

    def tearDown(self):
        self.temp.cleanup()


if __name__ == '__main__':
    unittest.main()
