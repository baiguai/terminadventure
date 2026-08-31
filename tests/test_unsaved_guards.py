#!/usr/bin/env python3
"""Test unsaved-changes guards: open/recent/enew are blocked when the document
is dirty, and the ! force variants (o!, O!, enew!) skip the guard."""
import os
import shutil

import harness

DATA_DIR = '/tmp/terminadventure_tests/unsaved_data'
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)

s = harness.launch(workdir=DATA_DIR, cols=90, rows=24)
try:
    def joined():
        return "\n".join(s.screen())

    def status():
        return s.row_text(s.rows - 1)

    def dirty():
        """Make an unsaved change to the selected node's text."""
        s.send(b'j')            # select the first node
        s.send(b'i')            # TREE -> NORMAL
        s.send(b'i')            # NORMAL -> INSERT
        s.send(b'x')
        s.send(b'\x1b')
        s.send(b'\x1b')         # back to TREE

    # --- two saved documents ---
    s.send(b'a'); s.send(b'Alpha'); s.send(b'\r')
    s.send(b'I'); s.send(b'alpha body'); s.send(b'\x1b')
    s.send(b'\x1b')             # TREE
    s.send(b'S'); s.send(b'doc1.json'); s.send(b'\r')
    s.require('Alpha', 'doc1 saved')

    s.send(b'a'); s.send(b'Beta'); s.send(b'\r')
    s.send(b'I'); s.send(b'beta body'); s.send(b'\x1b')
    s.send(b'\x1b')             # TREE
    s.send(b'S'); s.send(b'doc2.json'); s.send(b'\r')
    s.require('Beta', 'doc2 saved')

    # active document is doc1
    s.send(b':o doc1.json\r')
    s.require('Alpha', 'doc1 loaded')

    # --- :o with unsaved changes must be blocked ---
    dirty()
    s.send(b':o doc2.json\r')
    s.step(0.4)
    assert 'Unsaved changes' in status(), 'open must report unsaved changes'
    assert 'Alpha' in joined(), 'blocked open must not switch documents'
    print('ok: :o blocked with unsaved changes')

    # --- :o! must force the open ---
    s.send(b':o! doc2.json\r')
    s.step(0.4)
    assert 'Loaded' in status(), 'force open should load the file'
    assert 'Beta' in joined(), ':o! should switch to doc2'
    print('ok: :o! forces the open')

    # --- recent dialog (O): Enter blocked, then ! + Enter forces ---
    dirty()
    s.send(b'O')
    s.step(0.3)
    s.send(b'\r')
    s.step(0.4)
    assert 'Unsaved changes' in status(), 'recent open must report unsaved changes'
    assert 'Beta' in joined(), 'blocked recent open must not switch documents'
    print('ok: O (recent) blocked with unsaved changes')

    s.send(b'O')
    s.step(0.3)
    s.send(b'!')
    s.step(0.2)
    s.send(b'\r')
    s.step(0.4)
    assert 'Loaded' in status(), 'forced recent open should load the file'
    assert 'Beta' in joined(), 'forced recent open should keep doc2'
    print('ok: O! forces the recent open')

    # --- :O! opens the recent dialog pre-armed for a forced open ---
    dirty()
    s.send(b':O!\r')
    s.step(0.3)
    assert '!force-on' in joined(), ':O! should open the dialog pre-armed'
    s.send(b'\r')
    s.step(0.4)
    assert 'Loaded' in status(), ':O! Enter should load the file'
    assert 'Beta' in joined(), ':O! should force-open doc2'
    print('ok: :O! forces the recent open')

    # --- :enew blocked, :enew! forces ---
    dirty()
    s.send(b':enew\r')
    s.step(0.4)
    assert 'Unsaved changes' in status(), ':enew must report unsaved changes'
    assert 'Beta' in joined(), 'blocked :enew must keep the document'
    print('ok: :enew blocked with unsaved changes')

    s.send(b':enew!\r')
    s.step(0.4)
    assert 'New document' in status(), ':enew! should create a new document'
    s.require('Select a node to edit', ':enew! must blank the document')
    assert 'Beta' not in joined(), ':enew! must drop the old tree'
    print('ok: :enew! forces a new document')
finally:
    s.quit()

print('PASS')
