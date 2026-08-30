#!/usr/bin/env python3
"""Feature test for init.conf auto-restore: saving a document records the
path in init.conf (next to the app), so a relaunch reopens the last opened
file; :new clears the record so the next launch starts blank.

Permanent regression for config/ReadInit, config/WriteInit and the startup
hook in main.cpp.
"""
import os
import shutil

import harness

DATA_DIR = '/tmp/terminadventure_tests/init_data'
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)

init_conf = os.path.join(DATA_DIR, 'init.conf')

# --- save a document, then relaunch: it should come back automatically ---
s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Select a node to edit', 'first launch must start blank')
    s.send(b'a')
    s.require('create_node')
    s.send(b'Persist Me'); s.send(b'\r')
    s.send(b'I')
    s.send(b'remembered body')
    s.send(b'\x1b')
    s.send(b'\x1b')                 # TREE

    path = os.path.join(DATA_DIR, 'doc.json')
    s.send(b'S')
    s.require('saveas', 'S should open the :saveas prompt')
    s.send(path)
    s.send(b'\r')
    s.require('Saved', 'status bar should show Saved after :S')
finally:
    s.quit()

# init.conf should record the saved path next to the (per-test) app dir
if not os.path.exists(init_conf):
    print('FAIL: %s was not written' % init_conf)
    raise SystemExit(1)
with open(init_conf, encoding='utf-8') as f:
    content = f.read()
if 'last_file' not in content or path not in content:
    print('FAIL: init.conf should record %r' % path)
    raise SystemExit(1)

s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Persist Me', 'relaunch should auto-restore the last opened file')
    s.send(b'j')                    # select the node -> editor shows its text
    s.require('remembered body', 'auto-loaded document should carry its text')
finally:
    s.quit()

# --- :new clears the record, so the next launch starts blank ---
s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Persist Me', 'launch should still auto-restore')
    s.send(b':n')
    s.send(b'\r')
    s.require('Select a node to edit', ':new should blank the document')
finally:
    s.quit()

s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Select a node to edit', 'launch after :new must start blank')
    s.forbid('Persist Me', 'auto-restore should be cleared by :new')
finally:
    s.quit()

print('PASS')
