#!/usr/bin/env python3
"""Smoke test: blank start, create a node, edit its text, save to disk.

The launch()/quit() pair is the reusable start/end every test uses; the
checks in between are specific to this test.
"""
import os

import harness

s = harness.launch()          # START: spawn the app
try:
    s.require('Select a node to edit', 'app must start blank')

    # create a node via the :create_node prompt
    s.send(b'a')
    s.require('create_node', 'a should open the :create_node prompt')
    s.send(b'Alpha')
    s.send(b'\r')
    s.require('Alpha', 'Alpha should appear in the tree')

    # edit its text (I -> insert mode, two lines, Esc back to NORMAL)
    s.send(b'I')
    s.send(b'hello line one')
    s.send(b'\r')
    s.send(b'hello line two')
    s.send(b'\x1b')
    s.require('hello line one', 'first line should be visible in the editor')
    s.require('hello line two', 'second line should be visible in the editor')

    # S opens :saveas; saving writes a JSON file next to the app's cwd
    save_path = os.path.join(s.workdir, 'doc.json')
    s.send(b'S')
    s.require('saveas', 'S should open the :saveas prompt')
    s.send(save_path)
    s.send(b'\r')
    s.require('Saved', 'status bar should report Saved')
    if not os.path.exists(save_path):
        print('FAIL: %s was not created' % save_path)
        s.dump()
        raise SystemExit(1)
    with open(save_path, encoding='utf-8') as f:
        content = f.read()
    for fragment in ('"name": "Alpha"', 'hello line one', 'hello line two'):
        if fragment not in content:
            print('FAIL: %r missing from saved JSON' % fragment)
            s.dump()
            raise SystemExit(1)
finally:
    s.quit()                  # END: quit the app

print('PASS')
