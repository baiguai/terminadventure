#!/usr/bin/env python3
"""Feature test for the insert-link dialog (Ctrl+] in NORMAL/INSERT): it opens
a search-style dialog that, on Enter, inserts a `_Title_` node link to the
selected node into the currently active node's contents at the editor cursor,
instead of jumping to it like the normal search dialog does.
"""
import json
import os

import harness

s = harness.launch(cols=120, rows=24)
try:
    # Alpha with content, then Beta as a second root node.
    s.send(b'a')
    s.send(b'Alpha')
    s.send(b'\r')
    s.send(b'I')
    s.send(b'hello world')
    s.send(b'\x1b')  # NORMAL
    s.send(b'\x1b')  # TREE

    s.send(b'a')
    s.send(b'Beta')
    s.send(b'\r')    # TREE, Beta selected

    # Back up to Alpha and enter NORMAL mode (cursor at 0,0).
    s.send(b'k')
    s.send(b'i')

    doc_path = os.path.join(s.workdir, 'doc.json')

    def roots():
        with open(doc_path, encoding='utf-8') as f:
            return json.load(f)['roots']

    def by_name(name):
        for n in roots():
            if n['name'] == name:
                return n
        raise SystemExit('FAIL: node %r not found' % name)

    def save():
        s.send((':saveas %s\r' % doc_path).encode())
        s.require('Saved', 'doc should save')

    # Ctrl+] (GS, 0x1d) opens the insert-link dialog, not the jump search.
    s.send(b"\x1d")
    s.require('Insert Link', "Ctrl+] should open the insert-link dialog")
    s.require('Search: _', 'the dialog should have a filter line')
    s.require('Enter insert', 'footer should say Enter inserts a link')

    # Esc cancels without inserting.
    s.send(b'bet')
    s.require('1/1', 'typing should narrow the list to Beta')
    s.send(b'\x1b')
    s.forbid('Insert Link', 'Esc should close the dialog')

    # Reopen, type a lowercase query, Enter inserts _Beta_ at the cursor.
    s.send(b"\x1d")
    s.require('Insert Link')
    s.send(b':beta')          # ':x' restricts the match to node titles
    s.require('1/1')
    s.send(b'\r')
    s.forbid('Insert Link', 'Enter should close the dialog')
    s.require('Inserted Beta', 'status should confirm the insertion')
    save()
    assert by_name('Alpha')['text'] == '_Beta_hello world', (
        'link should be inserted at the cursor in Alpha, got %r'
        % by_name('Alpha')['text'])

    # INSERT mode: with the cursor mid-line, the link lands at the cursor.
    s.send(b'\x1b')           # TREE
    s.send(b'a')
    s.send(b'Gamma')
    s.send(b'\r')
    s.send(b'I')
    s.send(b'abc def')
    s.send(b'\x1b[D')         # ArrowLeft x4 -> cursor before "def"
    s.send(b'\x1b[D')
    s.send(b'\x1b[D')
    s.send(b'\x1b[D')
    s.send(b"\x1d")
    s.require('Insert Link', "Ctrl+] should work in INSERT mode")
    s.send(b'gamma')
    s.send(b'\r')
    s.forbid('Insert Link', 'Enter should close the dialog in INSERT mode')
    s.require('Inserted Gamma', 'status should confirm the INSERT-mode insertion')
    s.send(b'\x1b')           # INSERT -> NORMAL so ':' opens the command line
    save()
    assert by_name('Gamma')['text'] == 'abc_Gamma_ def', (
        'link should be inserted at the INSERT cursor in Gamma, got %r'
        % by_name('Gamma')['text'])
finally:
    s.quit()

print('PASS')
