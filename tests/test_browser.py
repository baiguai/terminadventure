#!/usr/bin/env python3
"""Feature test for the file-browser dialog: a path command (:open/:saveas/
:import_html/:export_html) given a directory argument opens a modal browser.
h/j/k/l/gg/G navigate, Enter picks a file, l does not pick, Esc cancels, and
the picked file's full path is handed back to the invoking command.

Permanent regression for src/browser/.
"""
import os

import harness

s = harness.launch()
try:
    # Fixture: a docs dir (with a nested dir) and a loose file at the root.
    for rel in ('docs/info.txt', 'docs/notes.md', 'docs/nested/deep.txt',
                'root.txt'):
        path = os.path.join(s.workdir, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(rel + '\n')

    # A valid tree saved into docs/ so the browser can load it later.
    s.send(b'a')
    s.require('create_node')
    s.send(b'Saved'); s.send(b'\r')
    s.send(b'I')
    s.send(b'Saved body')
    s.send(b'\x1b'); s.send(b'\x1b')          # NORMAL -> TREE
    s.send(b':saveas docs/tree.json')
    s.send(b'\r')
    s.require('Saved 1 nodes to', 'saveas with a file path writes directly')

    # `:open docs` (a directory) opens the browser on that directory.
    s.send(b':open docs')
    s.send(b'\r')
    s.require(' File Browser ', 'a dir arg should open the file browser')
    s.require('1/5', 'selection should start on ../ (row 1 of 5)')
    for entry in ('../', 'info.txt', 'notes.md', 'nested/', 'tree.json'):
        s.require(entry, 'browser should list %r' % entry)

    # The dialog consumes every key: `a` must not open create_node.
    s.send(b'a')
    s.forbid('create_node', 'dialog must consume a (no create_node prompt)')

    # G jumps to the last row (tree.json); Enter picks it and loads it.
    s.send(b'G')
    s.require('5/5', 'G should jump to the last entry')
    s.send(b'\r')
    s.forbid(' File Browser ', 'Enter should close the dialog')
    s.require('Loaded 1 nodes from', 'Enter should run :open on the picked file')
    s.forbid('Saved body',
             'a fresh load starts with no active node (editor placeholder)')

    # gg -> first row (../), l enters it (goes up a level to the workdir).
    s.send(b':open docs')
    s.send(b'\r')
    s.require(' File Browser ')
    s.send(b'gg')
    s.require('1/5', 'gg should jump to the first row')
    s.send(b'l')
    s.require('docs/', 'l on ../ should go up to the workdir')
    s.require('root.txt', 'workdir listing should show root.txt')

    # l enters the docs dir again; then l enters nested/.
    s.send(b'j')
    s.require('2/4', 'j should move onto docs/')
    s.send(b'l')
    s.require('tree.json', 'l on docs/ should enter the directory')
    s.forbid('root.txt', 'inside docs/ there should be no root.txt')
    s.send(b'j')
    s.require('2/5', 'j should move onto nested/')
    s.send(b'l')
    s.require('deep.txt', 'l should enter nested/')

    # l on a file does nothing; only Enter picks it.
    s.send(b'j')
    s.require('2/2', 'j should move onto deep.txt')
    s.send(b'l')
    s.require('2/2', 'l on a file must not pick or move it')
    s.send(b'\r')
    s.forbid(' File Browser ', 'Enter should close the dialog')
    s.require('Error: could not parse',
              'Enter should run :open on nested/deep.txt (a non-JSON file)')

    # h goes up one level; Esc cancels without doing anything.
    s.send(b':open docs')
    s.send(b'\r')
    s.require(' File Browser ')
    s.require('tree.json', 'the browser should reopen on docs/')
    s.send(b'h')
    s.require('docs/', 'h should go up one level (back to the workdir)')
    s.require('root.txt', 'the workdir still lists root.txt')
    s.send(b'h')
    s.forbid('root.txt', 'a second h should go up to the parent directory')
    s.send(b'\x1b')
    s.forbid(' File Browser ', 'Esc should close the dialog')

    # `.` opens the browser on the current directory.
    s.send(b':open .')
    s.send(b'\r')
    s.require(' File Browser ', 'a `.` arg should open the browser on cwd')
    s.require('docs/', 'cwd listing should show docs/')
    s.send(b'\x1b')
    s.forbid(' File Browser ')

    # For open/import, Enter on a folder still navigates into it.
    s.send(b':open docs')
    s.send(b'\r')
    s.require(' File Browser ')
    s.send(b'j')                        # onto nested/
    s.send(b'\r')                       # Enter navigates (open mode)
    s.require('deep.txt', 'Enter on a folder should navigate in open mode')
    s.require('1/2', 'inside nested/ there are 2 entries')
    s.forbid(':open ', 'open mode must not reopen the command line')
    s.send(b'\x1b')
    s.forbid(' File Browser ')

    # --- / filter ---------------------------------------------------------
    # `/` opens a filter field; typed keys go to the filter, not navigation.
    s.send(b':open docs')
    s.send(b'\r')
    s.require(' File Browser ')
    s.send(b'/')
    s.require('type to filter  Enter keep  Esc clear',
              '/ should open the filter field')
    s.require('1/5', 'an empty filter still shows the full list')

    # 'o' matches info.txt, notes.md and tree.json (not nested/ or ../).
    s.send(b'o')
    s.require('1/3 (of 5)', "'o' should narrow the list to 3 of 5 entries")
    # The last row is the status bar, which still shows the earlier
    # "Error: could not parse .../nested/deep.txt" message; the dialog
    # itself (rows above it) must not list nested/.
    s.forbid('nested/', 'nested/ does not contain o', rows=range(s.rows - 1))

    # 'j' while typing goes into the filter ('oj' matches nothing).
    s.send(b'j')
    s.require('(no matches)', 'typing j should narrow to nothing (not move)')
    s.send(b'\x7f')
    s.require('1/3 (of 5)', 'Backspace should remove the trailing j')

    # Enter keeps the filtered list and hides the filter field.
    s.send(b'\r')
    s.forbid('type to filter', 'Enter should hide the filter field')
    s.require('1/3 (of 5)', 'Enter should keep the filtered list')
    s.require('Esc clear', 'with a filter applied Esc should clear it')

    # navigation now works on the filtered list; Enter picks tree.json.
    s.send(b'j')
    s.require('2/3', 'j should move within the filtered list')
    s.send(b'G')
    s.require('3/3', 'G should jump to the last filtered entry')
    s.send(b'\r')
    s.forbid(' File Browser ', 'Enter should pick the filtered tree.json')
    s.require('Loaded 1 nodes from', 'the pick should be loaded')

    # Esc with a filter applied (field hidden) clears it; the dialog stays.
    s.send(b':open docs')
    s.send(b'\r')
    s.send(b'/')
    s.send(b'o')
    s.send(b'\r')                     # keep the filter
    s.require('Esc clear')
    s.send(b'\x1b')                   # Esc: clear the filter, not the dialog
    s.require('1/5', 'Esc should clear the filter and show all entries')
    s.require('Esc cancel', 'with no filter Esc should cancel the dialog')
    s.send(b'\x1b')                   # Esc now closes the dialog
    s.forbid(' File Browser ')

    # Esc with the filter field open clears and closes the field, not the
    # dialog.
    s.send(b':open docs')
    s.send(b'\r')
    s.send(b'/')
    s.send(b'o')
    s.require('type to filter')
    s.send(b'\x1b')                   # Esc in the field: clear + close field
    s.require('1/5', 'Esc in the field should clear the filter')
    s.send(b'\x1b')                   # Esc closes the dialog
    s.forbid(' File Browser ')

    # Entering a directory clears the applied filter.
    s.send(b':open docs')
    s.send(b'\r')
    s.send(b'/')
    s.send(b'nes')                    # matches nested/ only
    s.send(b'\r')                     # keep the filter
    s.require('1/1 (of 5)', 'filter should match nested/ only')
    s.send(b'l')                      # enter the filtered directory
    s.require('deep.txt', 'l should enter the filtered directory')
    s.require('1/2', 'inside nested/ the filter should be cleared')
    s.forbid(' (of ', 'entering a dir should clear the filter')
    s.send(b'\x1b')
    s.forbid(' File Browser ')

    # Going up a level also clears the applied filter.
    s.send(b':open docs')
    s.send(b'\r')
    s.send(b'/')
    s.send(b'o')
    s.send(b'\r')                     # keep the filter
    s.require('1/3 (of 5)')
    s.send(b'h')                      # go up one level
    s.require('root.txt', 'h should go up to the workdir')
    s.require('1/4', 'the workdir has 4 entries')
    s.forbid(' (of ', 'going up should clear the filter')
    s.send(b'\x1b')
    s.forbid(' File Browser ')

    # The dialog keeps a constant size across all filter states.
    def dialog_size():
        lines = s.screen()
        top = next(r for r, line in enumerate(lines) if '\u256d' in line)
        bottom = next(r for r, line in enumerate(lines) if '\u2570' in line)
        return len(lines[bottom].rstrip()), bottom - top + 1

    def require_size(msg):
        size = dialog_size()
        if size != base_size:
            print('FAIL: %s (got %r, want %r)' % (msg, size, base_size))
            s.dump()
            raise SystemExit(1)

    s.send(b':open docs')
    s.send(b'\r')
    s.require(' File Browser ')
    base_size = dialog_size()
    s.send(b'/')                      # open the filter field
    require_size('opening the filter field must not resize the dialog')
    s.send(b'o')                      # type a filter (field open)
    require_size('typing a filter must not resize the dialog')
    s.send(b'\r')                     # keep the filter (field closed)
    require_size('keeping the filter must not resize the dialog')
    s.send(b'j')                      # move within the filtered list
    require_size('moving the selection must not resize the dialog')
    s.send(b'\x1b')                   # clear the filter
    require_size('clearing the filter must not resize the dialog')
    s.send(b'\x1b')
    s.forbid(' File Browser ')

    # :saveas with a directory also opens the browser; the pick saves there.
    s.send(b':saveas docs')
    s.send(b'\r')
    s.require(' File Browser ', 'a dir arg to :saveas should open the browser')
    s.send(b'G')
    s.require('5/5')
    s.send(b'\r')
    s.require('Saved 1 nodes to', 'the pick should be saved via :saveas')

    # Save/export: the dialog is a folder picker. Enter on a folder closes it
    # and reopens the command line prefilled with that folder's path; a
    # filename is then typed and Enter saves into it.
    s.send(b':saveas docs')
    s.send(b'\r')
    s.require(' File Browser ')
    s.send(b'j')                        # onto nested/
    s.send(b'\r')                       # Enter picks the folder
    s.forbid(' File Browser ', 'Enter on a folder should close the browser')
    s.require(':saveas ', 'Enter on a folder should reopen :saveas')
    s.require('nested/', 'the command line should be prefilled with the folder')
    s.send(b'out.json')
    s.send(b'\r')
    s.require('Saved 1 nodes to',
              'typing a name should save into the chosen folder')
    if not os.path.exists(os.path.join(s.workdir, 'docs', 'nested', 'out.json')):
        print('FAIL: out.json was not written into docs/nested')
        s.dump()
        raise SystemExit(1)

    # `l` still navigates inside the browser for save/export ops.
    s.send(b':saveas docs')
    s.send(b'\r')
    s.require(' File Browser ')
    s.send(b'j')                        # onto nested/
    s.send(b'l')                        # navigates (not exit-to-command)
    s.require('deep.txt', 'l should still navigate inside save mode')
    s.require('1/3', 'inside nested/ there are 3 entries (../, deep.txt, out.json)')
    s.forbid(':saveas ', 'l on a folder must not reopen the command line')
    s.send(b'\x1b')
    s.forbid(' File Browser ')

    # Export: Enter on a folder closes the browser and reopens :X prefilled
    # with that folder's path for a filename.
    s.send(b':X docs')
    s.send(b'\r')
    s.require(' File Browser ', 'a dir arg to :X should open the browser')
    s.send(b'j')                        # onto nested/
    s.send(b'\r')                       # Enter picks the folder
    s.forbid(' File Browser ', 'Enter on a folder should close the browser')
    s.require(':X ', 'Enter on a folder should reopen :X')
    s.send(b'site.html')
    s.send(b'\r')
    s.require('Exported 1 nodes to',
              'typing a name should export into the chosen folder')
    if not os.path.exists(os.path.join(s.workdir, 'docs', 'nested', 'site.html')):
        print('FAIL: site.html was not exported into docs/nested')
        s.dump()
        raise SystemExit(1)
finally:
    s.quit()

print('PASS')
