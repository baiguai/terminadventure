#!/usr/bin/env python3
"""Feature test for the .txt exports (TREE t / TREE T), matching the web app's
exportNoteAsTxt()/exportTreeAsTxt().

t/T load :export_note_txt/:export_tree_txt into the command field (save-as
style). Tab completes directory paths; Enter with a directory arg pops the same
file browser as :saveas; `:` inside the dialog hands the current folder back to
the command line for a filename; a file path exports directly without the
dialog. Note exports strip _link_ underscores; branch exports keep them
verbatim, skip empty nodes and nodes whose text contains #noexp, and separate
nodes with four blank lines.
"""
import os

import harness


def edit_note(s, text):
    s.send(b'i'); s.step(0.3)   # TREE -> NORMAL
    s.send(b'i'); s.step(0.3)   # NORMAL -> INSERT
    s.send(text.encode()); s.step(0.3)
    s.send(b'\x1b'); s.step(0.3)   # INSERT -> NORMAL
    s.send(b'\x1b'); s.step(0.3)   # NORMAL -> TREE


s = harness.launch(cols=110, rows=24)
try:
    docs = os.path.join(s.workdir, 'docs')
    nested = os.path.join(docs, 'nested')
    os.makedirs(nested, exist_ok=True)

    # Root note with a _link_, plus a child whose note is #noexp (skipped).
    s.send(b'a'); s.send(b'Root'); s.send(b'\r'); s.step(0.4)
    edit_note(s, 'root _text_')
    s.send(b'A'); s.step(0.3)
    s.send(b'Child'); s.send(b'\r'); s.step(0.4)
    edit_note(s, 'hidden #noexp content')
    s.send(b'k'); s.step(0.3)   # select Root (the branch root)

    # t loads the command line (save-as style), not the dialog directly.
    s.send(b't'); s.step(0.5)
    s.require('export_note_txt', 't must load :export_note_txt in the command field')
    s.forbid(' File Browser ', 't must not open the dialog directly')

    # Tab completes a directory path with a trailing separator.
    partial = docs[:-1]
    s.send(partial.encode()); s.step(0.3)
    s.send(b'\t'); s.step(0.5)
    buf = s.row_text(s.rows - 2).rstrip()
    if not buf.endswith('docs/'):
        print('FAIL: Tab should complete the dir, buffer was %r' % buf)
        s.dump()
        raise SystemExit(1)

    # Enter with the directory arg pops the file browser on that directory.
    s.send(b'\r'); s.step(0.6)
    s.require(' File Browser ', 'a dir arg should pop the file browser')

    # The dialog is a folder picker: Enter on a folder closes it and puts
    # that folder's path back in the command field, ready for a filename.
    s.send(b'j'); s.step(0.3)           # onto nested/
    s.send(b'\r'); s.step(0.5)
    s.forbid(' File Browser ', 'Enter on a folder should close the dialog')
    buf = s.row_text(s.rows - 2).rstrip()
    s.require('export_note_txt', 'Enter on a folder should reopen :export_note_txt')
    if not buf.endswith('docs/nested/'):
        print('FAIL: command field should hold the folder path, got %r' % buf)
        s.dump()
        raise SystemExit(1)
    s.send(b'note.txt'); s.send(b'\r'); s.step(0.7)
    s.require('Exported note to', 'note export status')
    with open(os.path.join(nested, 'note.txt')) as f:
        assert f.read() == 'root text', 'note export must strip _link_ underscores'

    # T + file path exports the branch directly (no dialog), keeping links,
    # skipping the #noexp child, and separating nodes with four blank lines.
    branch = os.path.join(docs, 'branch.txt')
    s.send(b'T'); s.step(0.4)
    s.send(branch.encode()); s.step(0.3); s.send(b'\r'); s.step(0.7)
    s.forbid(' File Browser ', 'a file path must export directly')
    s.require('Exported branch to', 'branch export status')
    with open(branch) as f:
        content = f.read()
    assert 'root _text_\n\n\n\n' in content, 'branch keeps links + 4 blank lines'
    assert 'hidden' not in content, '#noexp node must be skipped'
finally:
    s.quit()

print('PASS')
