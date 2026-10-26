#!/usr/bin/env python3
"""The source-mutating scanner must pass these before it may touch source files."""
import importlib.util
import os
from pathlib import Path
import unittest
from tempfile import TemporaryDirectory
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "strip_comments", os.environ.get("OUTSHINE_STRIP_TEST_MODULE", ROOT / 'test/strip-comments.py'))
scanner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scanner)


class CommentPolicy(unittest.TestCase):
    def test_token_boundaries(self):
        self.assertEqual(scanner.strip('int/**/value; a+/*gap*/+b;'), 'int value; a+ +b;')

    def test_header_scope(self):
        self.assertFalse(scanner.keeps_comments('src/app/Main.cpp'))
        self.assertFalse(scanner.keeps_comments('src/render/device/metal/Device.mm'))
        self.assertTrue(scanner.keeps_comments('src/engine/Engine.h'))
        self.assertTrue(scanner.keeps_comments('src/base/Math.hpp'))
        self.assertIsNone(scanner.policy('include/obsolete.h'))

    def test_test_tree_is_untouched(self):
        self.assertIsNone(scanner.policy('test/example.cpp'))
        self.assertIsNone(scanner.policy('test/include/example.h'))
        self.assertEqual(scanner.settle('test/does-not-exist.cpp'), (0, 0))

    def test_header_comments_have_no_special_markup(self):
        text = '// lifetime\n/* units */\n/// legacy\n/** legacy */\n'
        self.assertEqual(scanner.strip(text, True), text)
        self.assertNotIn('lifetime', scanner.strip(text))
        self.assertNotIn('legacy', scanner.strip(text))

    def test_unchanged_implementations_and_headers_skip_formatter(self):
        with TemporaryDirectory() as directory, patch.object(scanner, 'ROOT', Path(directory)):
            source = Path(directory) / 'src'
            source.mkdir()
            cpp, header = source / 'Main.cpp', source / 'Contract.h'
            cpp.write_text('int value=1;\n')
            header.write_text('// Borrowed until the next update.\nint value;\n')
            with patch.object(scanner, 'reflowed', side_effect=AssertionError('formatter ran')):
                self.assertEqual(scanner.settle(cpp), (0, 0))
                self.assertEqual(scanner.settle(header), (0, 0))

    def test_literals_are_byte_identical(self):
        text = '''auto url = "https://host/path/*x*/";
auto shader = u8R"glsl(
// shader source  


namespace {
}
/* string payload */
)glsl";
auto c = '\\'';
auto n = 1'000 + 0xFF'AA;
'''
        self.assertEqual(scanner.strip(text), text)

    def test_block_newlines(self):
        self.assertEqual(scanner.strip('#define A x/*a\nb*/y\n'), '#define A x \ny\n')

    def test_line_splicing(self):
        self.assertEqual(scanner.strip('int a; // hidden\\\nint hidden;\nint b;'),
                         'int a;  \nint b;')
        self.assertEqual(scanner.strip('a/\\\n*hidden*\\\n/b'), 'a b')
        self.assertEqual(scanner.strip('a/\\\n/hidden\nb'), 'a \nb')

    def test_idempotence(self):
        text = 'int/**/x; // gone\n/// gone\n'
        once = scanner.strip(text)
        self.assertEqual(scanner.strip(once), once)

    def test_malformed_input_is_refused(self):
        for text in ('/* unfinished', 'R"tag(unfinished', '"unfinished'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                scanner.strip(text)


if __name__ == '__main__':
    unittest.main()
