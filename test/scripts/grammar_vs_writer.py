#!/usr/bin/env python3
"""Inventory grammar names found in writer string literals, not runtime capabilities.

Unmatched names require investigation. Matching names do not prove emission, nesting,
attribute preservation or roundtrip correctness. Computed names and reader aliases
need independent behavioral tests. Unsupported literal syntax fails analysis.
"""
import ast
import importlib.util
import pathlib
import re
import sys
import warnings

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import scenario_grammar as grammar

spec = importlib.util.spec_from_file_location('writer_comment_scanner',
                                             grammar.TREE / 'test/strip-comments.py')
scanner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scanner)
LITERAL = re.compile(
    r'(?:u8|u|U|L)?R"(?P<delimiter>[^\s()\\]{0,16})\((?P<raw>.*?)\)(?P=delimiter)"'
    r'|(?:u8|u|U|L)?(?P<ordinary>"(?:\\.|[^"\\])*")'
    r"|(?:u8|u|U|L)?(?P<character>'(?:\\.|[^'\\])*')", re.DOTALL)
TAG = re.compile(r'<([A-Za-z_][A-Za-z_0-9.-]*)(?=[\s/>]|$)')


def literal_names(source):
    source = scanner.strip(source)
    names = set()
    joined = ''
    previous = 0
    for match in LITERAL.finditer(source):
        if source[previous:match.start()].strip() or match['character'] is not None:
            names.update(TAG.findall(joined))
            joined = ''
        previous = match.end()
        if match['character'] is not None:
            continue
        if match['raw'] is not None:
            joined += match['raw']
            continue
        literal = match['ordinary']
        # Python and C++ disagree on variable-length hexadecimal escapes. Refuse those.
        if re.search(r'(?<!\\)\\x(?:[0-9a-fA-F]{3}|[0-9a-fA-F](?![0-9a-fA-F]))', literal):
            raise ValueError('unsupported hexadecimal escape in writer literal')
        with warnings.catch_warnings():
            warnings.simplefilter('error')
            joined += ast.literal_eval(literal)
    names.update(TAG.findall(joined))
    return names


def main():
    try:
        declared = grammar.children()
        if not declared:
            raise ValueError('no declared grammar children were found')
        written = literal_names(grammar.WRITER.read_text())
    except (OSError, ValueError, SyntaxError, Warning) as error:
        print(f'writer inventory: analysis incomplete: {error}')
        return 2
    missing = sorted(declared - written)
    print(f'writer inventory: {len(declared) - len(missing)}/{len(declared)} grammar names '
          f'found in literals; {len(missing)} unmatched')
    print('  NOT PROVEN: executed emission, paths, attributes, aliases or computed names.')
    for one in missing:
        print(f'  {one}')
    return 1 if missing else 0


if __name__ == '__main__':
    sys.exit(main())
