#!/usr/bin/env python3
"""Regression test for viw (VISUAL 'i w' -> select inner word).

Covers the reported bug: selecting a word mid-line at column 15 must select
that word, not the last word on the line. Also checks cursor-on-space picks
the next word, and that a subsequent delete operates on exactly the word.
"""
import harness

s = harness.launch()
try:
    # create a node with the exact repro text
    s.send(b'a')
    s.send(b'Alpha')
    s.send(b'\r')
    s.send(b'I')
    s.send(b'ddddd ddd dd dddddd dddd dd ddddddd d dddddd')
    s.send(b'\x1b')
    s.require('ddddd ddd dd dddddd dddd dd ddddddd d dddddd',
              'note text should be visible')

    # locate the editor row and the screen column where the text starts
    (row, text_start) = s.find('ddddd ddd dd')[0]

    def inv_cols():
        return [c for c in range(text_start, s.cols)
                if 'inv' in s.grid[row][c][1]]

    def expect_selection(a, b, label):
        got = inv_cols()
        want = list(range(text_start + a, text_start + b))
        if got != want:
            print('FAIL: %s expected inverted columns %r, got %r'
                  % (label, want, got))
            s.dump()
            raise SystemExit(1)
        print('ok: %s -> text columns [%d,%d)' % (label, a, b))

    # 1) cursor at text column 15 (mid-word) -> select the word at 13..18
    s.send(b'0')
    s.send(b'l' * 15)
    s.send(b'viw')
    expect_selection(13, 19, 'viw mid-word at col 15')

    # 2) cursor at text column 37 (a space) -> select the next word 38..43
    s.send(b'\x1b')
    s.send(b'0')
    s.send(b'l' * 37)
    s.send(b'viw')
    expect_selection(38, 44, 'viw on space at col 37')

    # 3) cursor at text column 5 (a space) -> select the next word 6..8
    s.send(b'\x1b')
    s.send(b'0')
    s.send(b'l' * 5)
    s.send(b'viw')
    expect_selection(6, 9, 'viw on space at col 5')

    # 4) delete the selected word (6..8) -> text keeps surrounding spaces
    s.send(b'd')
    s.require('ddddd  dd dddddd dddd dd ddddddd d dddddd',
              'viw + d should remove exactly the selected word')
finally:
    s.quit()

print('PASS')
