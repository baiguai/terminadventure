#!/usr/bin/env python3
"""Feature test for the dead-links dialog (`X` key): it scans every node for
`_Title_` references, lists those whose title does not exist, deduplicates by
title keeping the first instance in document order, and Enter / double-click
jumps to the note containing that first instance.

Permanent regression for the broken-links dialog in src/links/.
"""
import harness

s = harness.launch()
try:
    # Alpha holds two broken references; Charlie repeats _Ghost_ later in the
    # document, so _Ghost_ must be listed once and jump to Alpha, not Charlie.
    s.send(b'a')
    s.require('create_node')
    s.send(b'Alpha'); s.send(b'\r')
    s.require('Alpha')
    s.send(b'I')
    s.send(b'ref _Ghost_ on line zero'); s.send(b'\r')
    s.send(b'_Missing_ here'); s.send(b'\r')
    s.send(b'visit https://example.com/x')
    s.send(b'\x1b'); s.send(b'\x1b')          # INSERT -> NORMAL -> TREE

    s.send(b'a')
    s.require('create_node')
    s.send(b'Second Note'); s.send(b'\r')
    s.send(b'a')
    s.require('create_node')
    s.send(b'Charlie'); s.send(b'\r')
    s.require('Charlie')
    s.send(b'I')
    s.send(b'first line'); s.send(b'\r')
    s.send(b'again _Ghost_ on line one')
    s.send(b'\x1b'); s.send(b'\x1b')

    # `X` opens the dead-links dialog
    s.send(b'X')
    s.require('# Dead Links', 'X should open the dead-links dialog')
    s.require('1/2', 'two broken links should be listed')
    s.require('Ghost   (in Alpha)', 'first broken link should be listed')
    s.require('Missing   (in Alpha)', 'second broken link should be listed')

    # exactly the two broken titles, no URLs or resolvable notes
    if len(s.find('(in ')) != 2:
        print('FAIL: dialog should list only the two broken titles, got %r'
              % s.find('(in '))
        s.dump()
        raise SystemExit(1)

    # the dialog consumes every key
    s.send(b'a')
    s.forbid('create_node', 'dialog must consume a (no create_node prompt)')

    # Enter on the first entry jumps to the first instance: Alpha, not Charlie
    s.send(b'\r')
    s.forbid('# Dead Links', 'Enter should close the dialog')
    if s.inv_rows(0, 30) != [0]:
        print('FAIL: should select Alpha (first instance), got %r'
              % s.inv_rows(0, 30))
        s.dump()
        raise SystemExit(1)

    # reopen: _Ghost_ stays deduplicated even though Charlie also references it
    s.send(b'X')
    s.require('# Dead Links')
    s.require('1/2', 'dedup should still list two broken links')
    s.send(b'j')
    s.require('2/2', 'j should move down the list')
    s.send(b'\r')
    s.forbid('# Dead Links', 'Enter on the second entry should close the dialog')
    if s.inv_rows(0, 30) != [0]:
        print('FAIL: should select Alpha, got %r' % s.inv_rows(0, 30))
        s.dump()
        raise SystemExit(1)

    # double-click on a row activates it (two SGR mouse presses, same row)
    s.send(b'X')
    s.require('# Dead Links')
    hits = s.find('Ghost')
    if not hits:
        print('FAIL: cannot find the broken-link row in the dialog')
        s.dump()
        raise SystemExit(1)
    r, c = hits[0]
    press = b'\x1b[<0;%d;%dM' % (c + 2, r + 1)
    s.send(press + press)
    s.forbid('# Dead Links', 'double-click should close the dialog')
    if s.inv_rows(0, 30) != [0]:
        print('FAIL: double-click should select Alpha, got %r'
              % s.inv_rows(0, 30))
        s.dump()
        raise SystemExit(1)

    # Esc cancels
    s.send(b'X')
    s.require('# Dead Links')
    s.send(b'\x1b')
    s.forbid('# Dead Links', 'Esc should close the dialog')
finally:
    s.quit()

print('PASS')
