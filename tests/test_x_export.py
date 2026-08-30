#!/usr/bin/env python3
"""Feature test for the Terminadventure export command :x (export_terminadventure).

`x` is a GLOBAL command with no key binding. With a node selected it writes
that node and its whole subtree as a standalone .terminadventure document
(bookmarks and history reset, no sibling roots); with no selection it behaves
as a plain save-as of the entire document. The branch export round-trips:
reopening it yields only the branch.
"""
import json
import os
import shutil

import harness

DATA_DIR = '/tmp/terminadventure_tests/x_export_data'
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)

SUB = os.path.join(DATA_DIR, 'alpha.terminadventure')
FULL = os.path.join(DATA_DIR, 'whole.json')

s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Select a node to edit', 'app must start blank')

    # Doc(Alpha(Omega(Delta)) Beta): Alpha is a root carrying a subtree; Doc
    # and Beta are roots that must NOT appear in Alpha's branch export.
    s.send(b'a'); s.send(b'Doc'); s.send(b'\r')
    s.send(b'a'); s.send(b'Alpha'); s.send(b'\r')
    s.send(b'a'); s.send(b'Beta'); s.send(b'\r')
    s.send(b'k')                                    # Beta -> Alpha
    s.send(b'A'); s.send(b'Omega'); s.send(b'\r')
    s.send(b'A'); s.send(b'Delta'); s.send(b'\r')

    # With a node selected, :x writes just that subtree, directly (no dialog).
    # Tab-completes the directory path like the :X export does.
    s.send(b'k'); s.send(b'k')                      # Delta -> Omega -> Alpha
    s.send(b':x '); s.send(DATA_DIR[:-1].encode()); s.send(b'\t'); s.step(0.5)
    buf = s.row_text(s.rows - 2).rstrip()
    if not buf.endswith('x_export_data/'):
        print("FAIL: Tab should complete the dir, buffer was %r" % buf)
        s.dump()
        raise SystemExit(1)
    s.send(b'alpha.terminadventure'); s.send(b'\r'); s.step(0.7)
    s.require('Exported 3 nodes to', 'subtree export status')
    s.forbid(' File Browser ', 'a file path must export directly, no browser')
    with open(SUB) as f:
        doc = json.load(f)
    assert [n['name'] for n in doc['roots']] == ['Alpha'], (
        'branch export must contain only Alpha, got %r' % [n['name'] for n in doc['roots']])
    assert [c['name'] for c in doc['roots'][0]['children']] == ['Omega'], 'Alpha must keep Omega'
    assert [c['name'] for c in doc['roots'][0]['children'][0]['children']] == ['Delta'], 'Omega must keep Delta'
    assert 'Doc' not in json.dumps(doc), 'sibling root must not leak into the export'
    assert doc['bookmarks'] == [] and doc['history'] == [], 'branch export must reset attachments'

    # No selection: :x is a save-as of the entire document (5 nodes).
    s.send(b'\x1b')                                 # deselect
    s.send((':x ' + FULL).encode()); s.send(b'\r')
    s.require('Saved 5 nodes to', 'no selection must behave as :saveas')
    with open(FULL) as f:
        whole = json.load(f)
    assert [n['name'] for n in whole['roots']] == ['Doc', 'Alpha', 'Beta'], (
        'save-as must include all roots, got %r' % [n['name'] for n in whole['roots']])

    # The branch export round-trips: open it and only the branch is present.
    s.send((':o ' + SUB).encode()); s.send(b'\r')
    s.require('Loaded 3 nodes from', 'branch file should load')
    s.send(b'E')                                    # expand all
    s.require('Alpha', 'Alpha should be the only root')
    s.require('Omega', 'Omega should be present after expand')
    s.require('Delta', 'Delta should be present after expand')
    s.forbid('Doc', 'Doc must not load with the branch')
    s.forbid('Beta', 'Beta must not load with the branch')
finally:
    s.quit()

print('PASS')