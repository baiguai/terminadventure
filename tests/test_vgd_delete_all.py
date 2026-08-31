#!/usr/bin/env python3
"""Regression test: VGd on the whole node must not leave the cursor dangling.

Deleting every line of a node resets the text to one blank line; the cursor
must land on it (row 0) instead of pointing past the end, and typing must
keep working.
"""
import harness

s = harness.launch(cols=80, rows=24)
try:
    def joined():
        return "\n".join(s.screen())

    def editor_inv(row):
        return [c for c in range(31, s.cols) if 'inv' in s.grid[row][c][1]]

    # setup: a 5-line node
    s.send(b'a')
    s.send(b'Note')
    s.send(b'\r')
    s.send(b'I')
    for _ in range(4):
        s.send(b'line of text')
        s.send(b'\r')
    s.send(b'last line')
    s.send(b'\x1b')
    s.require('line of text', 'text entered')

    # VGd: select every line, delete it all
    s.send(b'gg')
    s.send(b'V')
    s.send(b'G')
    s.send(b'd')
    s.step(0.4)

    assert 'line of text' not in joined(), 'VGd must delete every line'
    assert 'last line' not in joined(), 'VGd must delete every line'
    print('ok: VGd deletes every line')

    # cursor must still render on the single remaining blank line
    assert editor_inv(0), 'cursor must stay visible on line 1'
    assert not s.inv_rows()[1:], 'no cursor rows below line 1'
    print('ok: cursor stays on line 1 after deleting all lines')

    # typing must land on the remaining line, not out-of-bounds memory
    s.send(b'i')
    s.send(b'x')
    s.step(0.4)
    assert s.find('x', rows=[0]), 'typed text must land on line 1'
    print('ok: editor still accepts input after VGd')
finally:
    s.quit()

print('PASS')
