#!/usr/bin/env python3
"""NORMAL-mode 'J'/'K' move the current line down/up, Vim style.

The cursor follows the moved line, a count works ('3K'), the boundary lines
are no-ops, and the move is undoable via the undo dialog.
"""
import json
import os

import harness

s = harness.launch(cols=120, rows=24)
try:
    s.send(b'a')
    s.send(b'Doc')
    s.send(b'\r')
    s.send(b'I')
    s.send(b'aaa')
    s.send(b'\r')
    s.send(b'bbb')
    s.send(b'\r')
    s.send(b'ccc')
    s.send(b'\x1b')  # NORMAL, cursor on 'ccc'

    doc_path = os.path.join(s.workdir, 'doc.json')

    def note_text():
        with open(doc_path, encoding='utf-8') as f:
            return json.load(f)['roots'][0]['text']

    def save():
        s.send((':saveas %s\r' % doc_path).encode())
        s.require('Saved', 'doc should save')

    # 'k' -> cursor on 'bbb', then 'K' moves it above 'aaa'
    s.send(b'k')
    s.send(b'K')
    save()
    assert note_text().split('\n') == ['bbb', 'aaa', 'ccc'], 'K should move bbb up'

    # cursor follows the moved line (now 'bbb' at row 0); 'J' moves it back
    s.send(b'J')
    save()
    assert note_text().split('\n') == ['aaa', 'bbb', 'ccc'], 'J should move bbb down'

    # 'J' on the last line is a no-op
    s.send(b'j')
    s.send(b'J')
    save()
    assert note_text().split('\n') == ['aaa', 'bbb', 'ccc'], 'J on last line no-ops'

    # a count moves repeatedly: append a line and move it to the top with '3K'
    s.send(b'$')   # end of 'ccc'
    s.send(b'a')   # insert after cursor
    s.send(b'\r')
    s.send(b'ddd')
    s.send(b'\x1b')  # cursor on 'ddd' (row 3)
    s.send(b'3K')
    save()
    assert note_text().split('\n') == ['ddd', 'aaa', 'bbb', 'ccc'], '3K moves ddd to top'

    # the move is undoable: 'u' opens the undo dialog; Enter applies the
    # newest snapshot (the '3K' move)
    s.send(b'u')
    s.require('Undo', 'undo dialog should open')
    s.send(b'\r')
    s.require('Undone', 'undo should apply')
    save()
    assert note_text().split('\n') == ['aaa', 'bbb', 'ccc', 'ddd'], 'undo restores order'

    print('ok: J/K line moves work with count and undo')
finally:
    s.quit()

print('PASS')
