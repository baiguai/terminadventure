#!/usr/bin/env python3
"""Feature test for the recent-files feature: saving/opening records paths
in init.conf (most recent first); 'O' (TREE/NORMAL) and ':O' open a dialog
that lists them; j/k move, Enter opens the selected file, Esc cancels; the
list survives relaunches and ':enew' (which only clears auto-restore).
'D' removes the highlighted entry, 'K'/'J' move it up/down the list (both
persist to init.conf), and entries whose file was deleted are pruned
automatically on startup and every time the dialog opens.

Permanent regression for config/ReadInit, config/WriteInit,
treeview/PushRecentFile and src/recent/.
"""
import os
import shutil

import harness

DATA_DIR = '/tmp/terminadventure_tests/recent_files_data'
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)

init_conf = os.path.join(DATA_DIR, 'init.conf')


def recent_lines(conf=init_conf):
    with open(conf, encoding='utf-8') as f:
        return [l.strip() for l in f if l.strip().startswith('recent_file')]


def last_line():
    with open(init_conf, encoding='utf-8') as f:
        for l in f:
            if l.strip().startswith('last_file'):
                return l.strip()
    return ''


# --- save two documents: init.conf should list them, newest first ---------
s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Select a node to edit', 'first launch must start blank')

    # doc A
    s.send(b'a')
    s.require('create_node')
    s.send(b'A'); s.send(b'\r')
    s.send(b'I'); s.send(b'alpha body')
    s.send(b'\x1b'); s.send(b'\x1b')              # NORMAL -> TREE
    s.send(b'S')
    s.require('saveas')
    s.send(b'alpha.json'); s.send(b'\r')
    s.require('Saved', 'save should write the document')

    # fresh doc, then doc B (saved last -> auto-restore target)
    s.send(b':n')
    s.send(b'\r')
    s.require('Select a node to edit', ':n should blank the document')
    s.send(b'a')
    s.require('create_node')
    s.send(b'B'); s.send(b'\r')
    s.send(b'I'); s.send(b'beta body')
    s.send(b'\x1b'); s.send(b'\x1b')
    s.send(b'S')
    s.require('saveas')
    s.send(b'beta.json'); s.send(b'\r')
    s.require('Saved', 'save should write the document')
finally:
    s.quit()

lines = recent_lines()
if len(lines) != 2 or 'beta.json' not in lines[0] or 'alpha.json' not in lines[1]:
    print('FAIL: init.conf recents wrong: %r' % lines)
    raise SystemExit(1)

# --- relaunch: auto-restore AND the recents dialog both come back ---------
s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Loaded 1 nodes from beta.json',
              'relaunch should auto-restore beta.json')
    s.send(b'j')                                  # select the node
    s.require('beta body', 'auto-restored doc should show its text')
    s.send(b'O')                                  # TREE mode, capital o
    s.require('Recent Files', 'O should open the recent-files dialog')
    s.require('1/2', 'two recents restored, selection on the newest')
    s.require('beta.json', 'newest entry should be listed')
    s.require('alpha.json', 'older entry should be listed')
    s.send(b'j')
    s.require('2/2', 'j should move down the list')
    s.send(b'k')
    s.require('1/2', 'k should move back up')
    s.send(b'\x1b')
    s.forbid('Recent Files', 'Esc should close the dialog')

    # the dialog consumes keys: `a` must not open create_node
    s.send(b'O')
    s.require('Recent Files')
    s.send(b'a')
    s.forbid('create_node', 'dialog must consume a (no create_node prompt)')
    s.send(b'\x1b')
finally:
    s.quit()

# --- Enter opens the selected file -----------------------------------------
s = harness.launch(workdir=DATA_DIR)
try:
    s.send(b'O')
    s.require('Recent Files')
    s.send(b'\r')                                 # Enter on beta.json
    s.forbid('Recent Files', 'Enter should close the dialog')
    s.require('Loaded 1 nodes from beta.json',
              'Enter should open the selected file')
finally:
    s.quit()

# --- :enew clears auto-restore but NOT the recents list --------------------
s = harness.launch(workdir=DATA_DIR)
try:
    s.send(b':enew')
    s.send(b'\r')
    s.require('Select a node to edit', ':enew should blank the document')
finally:
    s.quit()
if not last_line().endswith('='):
    print('FAIL: :enew should clear last_file, got %r' % last_line())
    raise SystemExit(1)
lines = recent_lines()
if len(lines) != 2:
    print('FAIL: :enew must not wipe recent_file lines, got %r' % lines)
    raise SystemExit(1)

s = harness.launch(workdir=DATA_DIR)
try:
    s.require('Select a node to edit', 'after :enew no auto-restore')
    s.send(b'O')
    s.require('Recent Files')
    s.require('1/2', 'recents should survive :enew + relaunch')
    s.send(b'\x1b')
finally:
    s.quit()

# --- NORMAL mode 'O' and the ':O' command -----------------------------------
s = harness.launch(workdir=DATA_DIR)
try:
    s.send(b'O')
    s.require('Recent Files', 'O should open the dialog in TREE mode')
    s.send(b'\x1b')

    s.send(b'i')                                  # TREE -> NORMAL
    s.send(b'O')
    s.require('Recent Files', 'O should open the dialog in NORMAL mode')
    s.send(b'\x1b')

    s.send(b':O')
    s.send(b'\r')
    s.require('Recent Files', ':O should open the dialog via the command')
    s.send(b'\x1b')
    s.forbid('Recent Files', 'Esc should close the dialog')
finally:
    s.quit()

# --- 'K'/'J' reorder entries and 'D' removes one; both persist ---------------
MOVE_DIR = '/tmp/terminadventure_tests/recent_move_data'
shutil.rmtree(MOVE_DIR, ignore_errors=True)
os.makedirs(MOVE_DIR)
MOVE_INIT = os.path.join(MOVE_DIR, 'init.conf')

s = harness.launch(workdir=MOVE_DIR)
try:
    s.require('Select a node to edit', 'move: blank start')
    s.send(b'a'); s.require('create_node')
    s.send(b'one'); s.send(b'\r')
    s.send(b'S'); s.require('saveas')
    s.send(b'a.json'); s.send(b'\r')
    s.require('Saved', 'move: save a.json')
    s.send(b'a'); s.require('create_node')
    s.send(b'two'); s.send(b'\r')
    s.send(b'S'); s.require('saveas')
    s.send(b'b.json'); s.send(b'\r')
    s.require('Saved', 'move: save b.json')
    s.send(b':enew'); s.send(b'\r')       # clear last_file so relaunch stays blank
    s.require('Select a node to edit', 'move: :enew blanks the doc')
finally:
    s.quit()

s = harness.launch(workdir=MOVE_DIR)
try:
    s.send(b'O')
    s.require('Recent Files')
    s.require('1/2', 'b.json is newest')
    s.require('b.json'); s.require('a.json')

    s.send(b'K')                          # K on the top entry is a no-op
    s.require('1/2', 'K at the top is a no-op')

    s.send(b'j')
    s.require('2/2', 'select a.json')
    s.send(b'K')                          # promote a.json above b.json
    s.require('1/2', 'K should move the entry up')
    if s.find('a.json')[0][0] >= s.find('b.json')[0][0]:
        print('FAIL: a.json should be listed above b.json after K')
        s.dump()
        raise SystemExit(1)
    if recent_lines(MOVE_INIT) != ['recent_file = a.json', 'recent_file = b.json']:
        print('FAIL: init.conf order after K wrong: %r' % recent_lines(MOVE_INIT))
        raise SystemExit(1)

    s.send(b'J')                          # push a.json back below b.json
    s.require('2/2', 'J should move the entry down')
    if recent_lines(MOVE_INIT) != ['recent_file = b.json', 'recent_file = a.json']:
        print('FAIL: init.conf order after J wrong: %r' % recent_lines(MOVE_INIT))
        raise SystemExit(1)

    s.send(b'D')                          # remove a.json (the selected entry)
    s.require('1/1', 'one entry left after D')
    s.forbid('a.json', 'D should remove the selected entry')
    s.send(b'\x1b')
    s.require('Recent entry removed', 'status should confirm the removal')
finally:
    s.quit()

s = harness.launch(workdir=MOVE_DIR)
try:
    s.send(b'O')
    s.require('Recent Files')
    s.require('1/1', 'removal should persist across relaunch')
    s.forbid('a.json', 'removed entry should not return')
    s.send(b'\x1b')
finally:
    s.quit()

# --- entries whose files were deleted are pruned automatically ---------------
PRUNE_DIR = '/tmp/terminadventure_tests/recent_prune_data'
shutil.rmtree(PRUNE_DIR, ignore_errors=True)
os.makedirs(PRUNE_DIR)
PRUNE_INIT = os.path.join(PRUNE_DIR, 'init.conf')

s = harness.launch(workdir=PRUNE_DIR)
try:
    s.require('Select a node to edit', 'prune: blank start')
    s.send(b'a'); s.require('create_node')
    s.send(b'one'); s.send(b'\r')
    s.send(b'S'); s.require('saveas')
    s.send(b'p1.json'); s.send(b'\r')
    s.require('Saved', 'prune: save p1.json')
    s.send(b'a'); s.require('create_node')
    s.send(b'two'); s.send(b'\r')
    s.send(b'S'); s.require('saveas')
    s.send(b'p2.json'); s.send(b'\r')
    s.require('Saved', 'prune: save p2.json')
finally:
    s.quit()

# delete the OLDER file, relaunch: startup pruning must drop it
os.remove(os.path.join(PRUNE_DIR, 'p1.json'))
s = harness.launch(workdir=PRUNE_DIR)
try:
    s.send(b'O')
    s.require('Recent Files')
    s.require('1/1', 'only the surviving file should be listed')
    s.forbid('p1.json', 'deleted file should be pruned from the list')
    s.send(b'\x1b')
finally:
    s.quit()
if 'p1.json' in ' '.join(recent_lines(PRUNE_INIT)):
    print('FAIL: init.conf should have been cleaned of the deleted file')
    raise SystemExit(1)

# delete the remaining file WHILE the app runs: the dialog prunes on open
s = harness.launch(workdir=PRUNE_DIR)
try:
    os.remove(os.path.join(PRUNE_DIR, 'p2.json'))
    s.send(b'O')
    s.require('Recent Files')
    s.require('0/0', 'empty list after mid-session prune')
    s.require('No recent files', 'dialog should show the empty state')
    s.send(b'\x1b')
    s.require('Removed 1 stale recent entr', 'op should report the prune')
finally:
    s.quit()

print('PASS')
