#!/usr/bin/env python3
"""Feature test for the links dialog (`#` key): link listing, Enter jumps,
y copies to the clipboard, mouse double-click activates, Esc cancels.

Permanent regression for the links dialog in src/links/.
"""
import harness

s = harness.launch()
try:
    # Alpha node whose text contains every kind of link the dialog collects
    s.send(b'a')
    s.require('create_node')
    s.send(b'Alpha'); s.send(b'\r')
    s.send(b'I')
    s.send('Visit https://example.com/page for details'.encode()); s.send(b'\r')
    s.send('Write to _Second Note_ when done'.encode()); s.send(b'\r')
    s.send('[Docs] https://example.com/docs'.encode()); s.send(b'\r')
    s.send('Mention _Ghost_ maybe'.encode())
    s.send(b'\x1b')           # INSERT -> NORMAL

    # Second Note node: the target of the _Second Note_ link
    s.send(b'\x1b')           # NORMAL -> TREE
    s.send(b'a')
    s.require('create_node')
    s.send(b'Second Note'); s.send(b'\r')
    s.require('Second Note')
    s.send(b'k')              # back to Alpha (row 0)

    # `#` opens the links dialog
    s.send(b'#')
    s.require('# Links', '# should open the links dialog')
    for lbl in ('Docs', 'https://example.com/page',
                'Second Note  ->  Second Note', 'Ghost'):
        s.require(lbl, 'dialog should list %r' % lbl)

    # the dialog consumes every key: `a` must not open create_node
    s.send(b'a')
    s.forbid('create_node', 'dialog must consume a (no create_node prompt)')

    # Enter on a URL keeps the dialog open (it launches xdg-open externally)
    s.send(b'\r')
    s.require('# Links', 'Enter on a URL should keep the dialog open')

    # Enter on a note link reveals the node and closes the dialog
    s.send(b'jj')
    s.send(b'\r')
    s.forbid('# Links', 'Enter on a note should close the dialog')
    if s.inv_rows(0, 30) != [1]:
        print('FAIL: Second Note should be selected in the tree, got %r'
              % s.inv_rows(0, 30))
        s.dump()
        raise SystemExit(1)

    # a link to a missing node stays open and reports it in the status bar
    s.send(b'k')              # back to Alpha
    s.send(b'#')
    s.require('# Links')
    s.send(b'jjj')            # selection -> Ghost (index 3)
    s.send(b'\r')
    s.require('# Links', 'Enter on a missing note should keep the dialog open')
    s.require('Node not found: Ghost', 'missing note should set a status message')

    # y copies the selected link: URLs verbatim, notes as _Title_
    s.send(b'kkk')            # -> Docs (index 0, markdown URL)
    s.send(b'y')
    s.require('Copied: https://example.com/docs',
              'y should copy the markdown link URL')
    s.send(b'j')
    s.send(b'y')
    s.require('Copied: https://example.com/page', 'y should copy the plain URL')
    s.send(b'j')
    s.send(b'y')
    s.require('Copied: _Second Note_', 'y should copy a note as _Title_')
    s.require('# Links', 'y must keep the dialog open')

    # Esc cancels
    s.send(b'\x1b')
    s.forbid('# Links', 'Esc should close the dialog')

    # double-click on a row activates it (two SGR mouse presses, same row)
    s.send(b'#')
    s.require('# Links')
    hits = s.find(' -> ')
    if not hits:
        print('FAIL: cannot find the note row in the dialog')
        s.dump()
        raise SystemExit(1)
    r, c = hits[0]
    press = b'\x1b[<0;%d;%dM' % (c + 2, r + 1)
    s.send(press + press)     # back-to-back = double-click within 500ms
    s.forbid('# Links', 'double-click should close the dialog')
    if s.inv_rows(0, 30) != [1]:
        print('FAIL: double-click should select Second Note, got %r'
              % s.inv_rows(0, 30))
        s.dump()
        raise SystemExit(1)
finally:
    s.quit()

print('PASS')
