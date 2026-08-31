#!/usr/bin/env python3
"""Regression test: deleting a node must refresh the editor to the new
selection's text.

Nodes live in std::vectors; deleting one shifts the next node into the
deleted node's memory slot, so the editor must compare node identity by id
rather than pointer, otherwise it keeps showing the deleted node's text.
"""
import harness

s = harness.launch(cols=80, rows=24)
try:
    def joined():
        return "\n".join(s.screen())

    # create Alpha (index 0), then Beta (inserted after it, index 1)
    s.send(b'a'); s.send(b'Alpha'); s.send(b'\r')
    s.send(b'a'); s.send(b'Beta'); s.send(b'\r')
    s.send(b'I'); s.send(b'bbb'); s.send(b'\x1b')
    s.send(b'\x1b')           # back to TREE
    s.send(b'k')              # select Alpha
    s.send(b'I'); s.send(b'aaa'); s.send(b'\x1b')
    s.send(b'\x1b')           # back to TREE
    s.send(b'i')              # into NORMAL mode
    s.require('aaa', 'Alpha text shown')

    s.send(b'\x1b')           # back to TREE
    s.send(b'D')              # delete Alpha
    s.step(0.4)

    assert 'aaa' not in joined(), 'deleted node text must not linger'
    s.send(b'i')
    s.step(0.3)
    s.require('bbb', 'editor must show Beta text after deleting Alpha')
    print('ok: editor refreshes to the next node after delete')

    # deleting the last remaining node clears the editor
    s.send(b'\x1b')           # back to TREE
    s.send(b'D')              # delete Beta
    s.step(0.4)
    s.require('Select a node to edit', 'editor clears when nothing is selected')
    print('ok: editor clears when no node remains')
finally:
    s.quit()

print('PASS')
