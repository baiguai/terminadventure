#!/usr/bin/env python3
"""Regression test: :w warning, :S save, on-disk JSON, :open reload, and
auto-restore of the last opened file via init.conf on relaunch.

Uses an explicit workdir so the saved file survives a relaunch.
"""
import os
import shutil

import harness

DATA_DIR = '/tmp/terminadventure_tests/persist_data'
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)

s = harness.launch(workdir=DATA_DIR)
try:
    # blank start, and :w with no stored path warns instead of saving
    s.require('Select a node to edit', 'app must start blank')
    s.send(b':w'); s.send(b'\r')
    s.require('use :saveas', ':w with no path should suggest :S')

    # build a node with text (including chars that stress the JSON escaping)
    s.send(b'a')
    s.require('create_node')
    s.send(b'Read Me'); s.send(b'\r')
    s.require('Read Me')
    s.send(b'I')
    s.send('Hello "world" \\ back'.encode())
    s.send(b'\r')
    s.send('second line é'.encode())
    s.send(b'\x1b')

    # S opens :saveas; check the JSON that lands on disk
    path = os.path.join(DATA_DIR, 'terminadventure.json')
    s.send(b'S')
    s.require('saveas', 'S should open the :saveas prompt')
    s.send(path)
    s.send(b'\r')
    s.require('Saved', 'status bar should show Saved after :S')
    if not os.path.exists(path):
        print('FAIL: %s was not created' % path)
        s.dump()
        raise SystemExit(1)
    with open(path, encoding='utf-8') as f:
        content = f.read()
    for fragment in ('"version": 1', '"name": "Read Me"',
                     'second line é', '\\"world\\"', '\\\\ back',
                     '"history"'):
        if fragment not in content:
            print('FAIL: %r missing from saved JSON' % fragment)
            s.dump()
            raise SystemExit(1)

    # quit, relaunch in the same directory: init.conf should restore the
    # last opened file automatically
    s.quit()
    s = harness.launch(workdir=DATA_DIR)
    s.require('Read Me', 'relaunch should auto-restore the last opened file')
    s.send(b':open ' + path.encode())
    s.send(b'\r')
    s.require('Read Me', ':open should still load a file explicitly')
    # the saved history should have come back with the file
    s.send(b'<')                  # open the history dialog
    s.require(' < History ', 'history dialog should open')
    s.require('1/1', 'the loaded file should carry its history (1 entry)')
    s.send(b'\x1b')
    s.send(b'j')              # select the node so the editor shows its text
    s.require('Hello', 'edited text should survive reload')
    s.require('second line', 'second line should survive reload')
finally:
    s.quit()

print('PASS')
