#!/usr/bin/env python3
"""The source-mutating scanner must pass these before it may touch source files."""
import importlib.util
import os
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "strip_comments", os.environ.get("OUTSHINE_STRIP_TEST_MODULE", ROOT / 'test/strip-comments.py'))
scanner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scanner)


class CommentPolicy(unittest.TestCase):
    def test_token_boundaries(self):
        self.assertEqual(scanner.strip('int/**/value; a+/*gap*/+b;'), 'int value; a+ +b;')

    def test_doxygen_scope(self):
        self.assertFalse(scanner.keeps_doxygen('src/client/Main.cpp'))
        self.assertTrue(scanner.keeps_doxygen('include/Outshine.h'))
        self.assertFalse(scanner.keeps_doxygen('src/include/internal.h'))

    def test_test_tree_is_untouched(self):
        self.assertIsNone(scanner.policy('test/example.cpp'))
        self.assertIsNone(scanner.policy('test/include/example.h'))
        self.assertEqual(scanner.settle('test/does-not-exist.cpp'), (0, 0))

    def test_documentation(self):
        text = '/// API\n//! detail\n/** API */\n/*! detail */\n// internal\n'
        self.assertEqual(scanner.strip(text, True), text.replace('// internal', ' '))
        self.assertNotIn('API', scanner.strip(text, False))
        self.assertNotIn('detail', scanner.strip(text, False))

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
