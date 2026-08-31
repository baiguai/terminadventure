#!/usr/bin/env python3
"""Smoke test: build/update a markdown table from VISUAL mode.

Select pipe rows in the editor (v + j...), press ';' and the app rebuilds
them into a padded table with a '+---+' separator row, mirroring the web
app's Ctrl+; (formatMarkdownTable).
"""
import harness

s = harness.launch()
try:
    s.require('Select a node to edit', 'app must start blank')

    s.send(b'a')
    s.require('create_node', 'a should open the :create_node prompt')
    s.send(b'Alpha')
    s.send(b'\r')
    s.require('Alpha', 'Alpha should appear in the tree')

    # type the unformatted table body (4 lines)
    s.send(b'I')
    s.send(b'| name | age |')
    s.send(b'\r')
    s.send(b'|----|---|')
    s.send(b'\r')
    s.send(b'| alice | 30 |')
    s.send(b'\r')
    s.send(b'| bob | 5 |')
    s.send(b'\x1b')
    s.require('| name | age |', 'raw table rows should be visible')

    # cursor is on the last row after Esc; jump to the top and select all
    s.send(b'gg')
    s.send(b'v')
    s.require('VISUAL', 'v should enter VISUAL mode')
    s.send(b'j')
    s.send(b'j')
    s.send(b'j')
    if s.inv_rows(31, 80) != [0, 1, 2, 3]:
        print('FAIL: expected rows 0-3 selected, got %r' % s.inv_rows(31, 80))
        s.dump()
        raise SystemExit(1)

    # ';' rebuilds the padded table
    s.send(b';')
    s.require('| name  | age |', 'header row should be padded')
    s.require('+-------+-----+', 'separator row should be rebuilt')
    s.require('| alice | 30  |', 'first data row should be padded')
    s.require('| bob   | 5   |', 'second data row should be padded')
    s.require('NORMAL', 'formatting should return to NORMAL mode')
    s.require('Table updated', 'status should report the update')

    # a selection without pipes is a no-op
    s.send(b'G')
    s.send(b'v')
    s.send(b';')
    s.require('No table detected', 'a selection without pipes should be a no-op')
    s.require('| bob   | 5   |', 'the table should be left untouched')

finally:
    s.quit()

print('PASS')
