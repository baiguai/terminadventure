#!/usr/bin/env python3
"""Feature test for the history dialog (`<` key): nodes with text are
recorded as they are viewed (most recent first, MRU dedup), j/k navigate,
Enter jumps to the node, Esc cancels, and deleted nodes render as
"(deleted node)".

Permanent regression for src/history/.
"""
import harness

s = harness.launch()
try:
    # Three nodes with text (all recorded) and one empty node (not recorded)
    for name, body in (('Alpha', 'Alpha body'), ('Beta', 'Beta body'),
                       ('Gamma', 'Gamma body')):
        s.send(b'a')
        s.require('create_node')
        s.send(name.encode()); s.send(b'\r')
        s.send(b'I')
        s.send(body.encode())
        s.send(b'\x1b')           # NORMAL (saves + records the node)
        s.send(b'\x1b')           # TREE

    s.send(b'a')
    s.require('create_node')
    s.send(b'Empty'); s.send(b'\r')   # no text -> must NOT be in history

    # `<` opens the history dialog: most recent (Gamma) first, footer 1/3
    s.send(b'<')
    s.require('< History', '< should open the history dialog')
    s.require('j/k move  Enter jump  Esc cancel', 'dialog footer')
    s.require('1/3', '3 recorded nodes, selection on the most recent')
    for lbl in ('Alpha', 'Beta', 'Gamma'):
        s.require(lbl, 'dialog should list %r' % lbl)

    # the dialog consumes every key: `a` must not open create_node
    s.send(b'a')
    s.forbid('create_node', 'dialog must consume a (no create_node prompt)')

    # Enter on the top (most recent) entry jumps to Gamma
    s.send(b'\r')
    s.forbid('< History', 'Enter should close the dialog')
    if s.inv_rows(0, 30) != [2]:
        print('FAIL: Enter should reveal Gamma (tree row 2), got %r'
              % s.inv_rows(0, 30))
        s.dump()
        raise SystemExit(1)

    # j/k navigation: j twice reaches Alpha (footer 3/3), Enter jumps to it
    s.send(b'<')
    s.require('< History')
    s.send(b'jj')
    s.require('3/3', 'j should move the selection down the list')
    s.send(b'\r')
    if s.inv_rows(0, 30) != [0]:
        print('FAIL: Enter should reveal Alpha (tree row 0), got %r'
              % s.inv_rows(0, 30))
        s.dump()
        raise SystemExit(1)

    # MRU dedup: Alpha is now the most recent, so reopening shows it on top
    s.send(b'<')
    s.require('< History')
    s.require('1/3', 'most recently viewed node should be at the top')
    s.send(b'\r')
    if s.inv_rows(0, 30) != [0]:
        print('FAIL: top entry should be Alpha (tree row 0), got %r'
              % s.inv_rows(0, 30))
        s.dump()
        raise SystemExit(1)

    # `<` also works from NORMAL mode; Esc cancels and stays in NORMAL
    s.send(b'i')                  # TREE -> NORMAL
    s.send(b'<')
    s.require('< History', '< should open the history dialog in NORMAL mode')
    s.send(b'\x1b')
    s.forbid('< History', 'Esc should close the dialog')
    s.require('NORMAL', 'after Esc the app should still be in NORMAL mode')
    s.send(b'\x1b')               # NORMAL -> TREE

    # a deleted node stays in history and renders as "(deleted node)"
    s.send(b'a')
    s.require('create_node')
    s.send(b'Delta'); s.send(b'\r')
    s.send(b'I')
    s.send(b'Delta body')
    s.send(b'\x1b')
    s.send(b'\x1b')               # back to TREE (Delta recorded)
    s.send(b'D')                  # delete Delta
    s.send(b'<')                  # active_node must stay valid after delete
    s.require('< History')
    s.require('1/4', 'deleted Delta should be the most recent of 4 entries')
    s.require('(deleted node)', 'deleted nodes should render dimmed marker')
    s.send(b'\x1b')
    s.forbid('< History', 'Esc should close the dialog')
finally:
    s.quit()

print('PASS')
