#!/usr/bin/env python3
"""Regression test: history round-trips through the HTML export/import.

`:X` writes the viewed-node history into the exported HTML's `historyStack`;
`:U` reads it back, so history persists per document. Also: notes whose first
line differs from the node name get the name prepended (followed by a blank
line) in the export, unless the note has no text at all (only in the HTML
output, the stored note is unchanged).
"""
import os
import shutil

import harness

DATA_DIR = '/tmp/terminadventure_tests/html_roundtrip_data'
HTML = os.path.join(DATA_DIR, 'roundtrip.html')
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)

s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Select a node to edit', 'app must start blank')

    s.send(b'a')
    s.require('create_node')
    s.send(b'Alpha'); s.send(b'\r')
    s.require('Alpha')
    s.send(b'I')
    s.send(b'Hello world')
    s.send(b'\x1b'); s.send(b'\x1b')      # INSERT -> NORMAL -> TREE

    # a sibling node whose first line already matches its name must be unchanged
    s.send(b'a')
    s.require('create_node')
    s.send(b'Beta'); s.send(b'\r')
    s.send(b'I')
    s.send(b'Beta')
    s.send(b'\r')
    s.send(b'and the rest')
    s.send(b'\x1b'); s.send(b'\x1b')

    # a sibling node with no text gets no name line invented for it
    s.send(b'a')
    s.require('create_node')
    s.send(b'Gamma'); s.send(b'\r')

    # export the document (tree + history) to an HTML file
    s.send(b':X ' + HTML.encode())
    s.send(b'\r')
    s.require('Exported', 'status bar should report the export')
    with open(HTML, encoding='utf-8') as f:
        html = f.read()
    if 'let historyStack' not in html:
        print('FAIL: exported HTML has no historyStack')
        s.dump()
        raise SystemExit(1)
    if '"title": "Alpha"' not in html:
        print('FAIL: exported HTML history lacks the node title')
        s.dump()
        raise SystemExit(1)
    # a note whose first line differs from its name gets the name prepended,
    # followed by a blank line before the note's own text
    if '"content": "Alpha\\n\\nHello world"' not in html:
        print('FAIL: Alpha content should be prefixed with its name')
        s.dump()
        raise SystemExit(1)
    # a note whose first line equals its name is left untouched (no doubling)
    if '"content": "Beta\\nand the rest"' not in html:
        print('FAIL: Beta content should keep its name as the first line')
        s.dump()
        raise SystemExit(1)
    # a note with no text stays empty - no title line is invented for it
    if '"content": ""' not in html or '"content": "Gamma' in html:
        print('FAIL: empty Gamma node should stay empty (no name line)')
        s.dump()
        raise SystemExit(1)

    # new document clears history; importing the HTML brings it back
    s.send(b':n!'); s.send(b'\r')
    s.require('New document', 'new document should reset the workspace')
    s.send(b':U ' + HTML.encode())
    s.send(b'\r')
    s.require('Imported', 'status bar should report the import')
    s.send(b'<')
    s.require(' < History ', 'history dialog should open')
    s.require('1/2', 'imported HTML should carry its history (2 entries)')
    s.send(b'\x1b')

finally:
    s.quit()

print('PASS')
