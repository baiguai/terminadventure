#!/usr/bin/env python3
"""TREE-mode 'L' nests the selected node under the node directly above at the
same level (its previous sibling), exactly one level deeper.

Regression for the leftover folder/note logic in MoveParent() that picked the
deepest visible node above the selection, dropping the node under a descendant
several levels too deep. Also checks 'H' reverses it and that 'L' is a no-op
when there is no node above at the same level.
"""
import json
import os

import harness

s = harness.launch(cols=80, rows=24)
try:
    # Doc, Alpha(AlphaKid(Deep)), Beta -- Beta is a root directly after Alpha.
    s.send(b'a'); s.send(b'Doc'); s.send(b'\r')
    s.send(b'a'); s.send(b'Alpha'); s.send(b'\r')
    s.send(b'A'); s.send(b'AlphaKid'); s.send(b'\r')
    s.send(b'A'); s.send(b'Deep'); s.send(b'\r')
    s.send(b'k'); s.send(b'k')            # Deep -> AlphaKid -> Alpha
    s.send(b'a'); s.send(b'Beta'); s.send(b'\r')

    doc_path = os.path.join(s.workdir, 'doc.json')

    def save():
        s.send((':saveas %s\r' % doc_path).encode())
        s.require('Saved', 'doc should save')

    def roots():
        with open(doc_path, encoding='utf-8') as f:
            return json.load(f)['roots']

    def child_names(node):
        return [c['name'] for c in node['children']]

    # Select Beta (after Deep) and press L: it must become a child of Alpha --
    # the node above at the same level -- not of Deep (Alpha's deepest
    # descendant), which the old logic dropped it under.
    s.send(b'j'); s.send(b'j'); s.send(b'j')  # Alpha -> AlphaKid -> Deep -> Beta
    s.send(b'L')
    save()
    alpha = [n for n in roots() if n['name'] == 'Alpha'][0]
    assert child_names(alpha) == ['Beta', 'AlphaKid'], (
        'Beta should nest directly under Alpha, got %r' % child_names(alpha))
    assert child_names(alpha['children'][0]) == [], (
        'Beta should have no children yet, got %r' % child_names(alpha['children'][0]))
    assert child_names(alpha['children'][1]) == ['Deep'], (
        'AlphaKid should keep Deep, got %r' % child_names(alpha['children'][1]))

    # H moves Beta back out, one level up, as a root after Alpha.
    s.send(b'H')
    save()
    assert [n['name'] for n in roots()] == ['Doc', 'Alpha', 'Beta'], (
        'H should return Beta to root level, got %r' % [n['name'] for n in roots()])
    beta = [n for n in roots() if n['name'] == 'Beta'][0]
    assert child_names(beta) == [], 'H should not leave children behind'

    # L on the first root (no node above at the same level) is a no-op.
    s.send(b'k'); s.send(b'k'); s.send(b'k'); s.send(b'k')  # Beta -> Deep -> AlphaKid -> Alpha -> Doc
    s.send(b'L')
    save()
    assert [n['name'] for n in roots()] == ['Doc', 'Alpha', 'Beta'], (
        'L on the first root should be a no-op, got %r' % [n['name'] for n in roots()])

    # L on a first child (no node above at its level) is also a no-op, and
    # must NOT nest under its own ancestor.
    s.send(b'j'); s.send(b'j')            # Doc -> Alpha -> AlphaKid
    s.send(b'L')
    save()
    alpha = [n for n in roots() if n['name'] == 'Alpha'][0]
    assert child_names(alpha) == ['AlphaKid'], (
        'L on a first child should be a no-op, got %r' % child_names(alpha))
    kid = alpha['children'][0]
    assert child_names(kid) == ['Deep'], (
        'AlphaKid should keep Deep, got %r' % child_names(kid))
finally:
    s.quit()

print('PASS')
