#!/usr/bin/env python3
"""Tag search in the TREE-mode / dialog: '#' lists tags, picking one (or an
exact match) shows the notes carrying it, and Enter jumps to a note."""
import json
import os

import harness

s = harness.launch(cols=92, rows=30)
try:
    doc_path = os.path.join(s.workdir, 'doc.json')
    doc = {
        'version': 1,
        'roots': [
            {'id': 'n1', 'name': 'Alpha', 'expanded': True,
             'text': 'alpha note #foo #bar'},
            {'id': 'n2', 'name': 'Beta', 'expanded': True,
             'text': 'beta note #Foo #baz, and #foo.bar'},
            {'id': 'n3', 'name': 'Gamma', 'expanded': True,
             'text': 'gamma #qux #alpha-tag'},
            {'id': 'n4', 'name': 'Delta', 'expanded': True,
             'text': 'colors #fff #ff8800 #ff8800aa and #mix #work'},
        ],
    }
    with open(doc_path, 'w', encoding='utf-8') as f:
        json.dump(doc, f)

    s.send((':open %s\r' % doc_path).encode())
    s.require('Loaded', 'doc should load')

    # '#' alone lists every tag once, case-insensitively de-duplicated and sorted
    s.send(b'/')
    s.require('/ Search', 'search dialog should open')
    s.send(b'#')
    s.require('Search: #_', 'filter should show #')
    s.require('#alpha-tag', 'tag #alpha-tag listed')
    s.require('#bar', 'tag #bar listed')
    s.require('#baz', 'tag #baz listed')
    s.require('#foo', 'tag #foo listed once (Foo deduped)')
    s.require('#mix', 'tag #mix listed')
    s.require('#qux', 'tag #qux listed')
    s.require('#work', 'tag #work listed')
    s.forbid('#Foo', 'tags should be lowercased', rows=range(3, 26))
    for color in ('#fff', '#ff8800', '#ff8800aa'):
        s.forbid(color, 'HTML color %s should not be a tag' % color,
                 rows=range(3, 26))

    # searching for an HTML color finds nothing
    s.send(b'ff8800')
    s.require('Search: #ff8800_', 'filter should show #ff8800')
    s.require('No matches', 'color should not match any tag')
    s.send(b'\x7f' * 6)  # clear 'ff8800' back to '#'
    s.require('Search: #_', 'back to bare #')

    # a substring narrows the tag list
    s.send(b'fo')
    s.require('Search: #fo_', 'filter should show #fo')
    s.require('#foo', '#foo still listed')
    s.forbid('#bar', '#bar should not match #fo', rows=range(3, 26))

    # an exact tag shows the notes carrying it (case-insensitive match)
    s.send(b'o')
    s.require('Search: #foo_', 'filter should show #foo')
    s.require('Alpha', 'Alpha has #foo')
    s.require('Beta', 'Beta has #Foo')
    s.forbid('Gamma', 'Gamma has no #foo', rows=range(3, 26))

    # Backspace drops back to the tag list
    s.send(b'\x7f')
    s.require('Search: #fo_', 'backspace should return to tag list')
    s.require('#foo', '#foo tag listed again')
    s.forbid('Alpha', 'not in notes phase anymore', rows=range(3, 26))

    # picking a tag from the list jumps to its notes, then Enter reveals it
    s.send(b'\x7f\x7f')  # '#fo' -> '#' (clear the tag text)
    s.require('Search: #_', 'back to bare #')
    s.send(b'b')         # '#b' -> #bar and #baz
    s.require('#bar', '#bar listed')
    s.require('#baz', '#baz listed')
    s.send(b'\r')
    s.require('Search: #bar_', 'picking a tag fills the filter')
    s.require('Alpha', 'Alpha is the only #bar note')
    s.forbid('Beta', 'Beta has no #bar', rows=range(3, 26))
    s.send(b'\r')
    s.forbid('Search:', 'Enter on a note should close the dialog',
             rows=range(3, 26))
finally:
    s.quit()

print('PASS')
