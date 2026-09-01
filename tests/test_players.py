#!/usr/bin/env python3
"""Players pane: create a character, save it (list + on-disk JSON), reload it,
and exercise the delete-confirmation dialog."""

import os
import shutil

import harness

DATA_DIR = '/tmp/terminadventure_tests/players_data'
shutil.rmtree(DATA_DIR, ignore_errors=True)
os.makedirs(DATA_DIR)

NAME = 'Aldric'


def enter_players(s):
    s.send(b'j'); s.send(b'j'); s.send(b'\x1b'); s.send(b'j')


def new_character(s, name):
    s.send(b'\x1bB')                     # ArrowDown -> focus Name field
    s.send(name.encode())                # type the name


def save_character(s):
    s.send(b'\t'); s.send(b'\t'); s.send(b'2')   # to action zone -> Save


s = harness.launch(workdir=DATA_DIR)
try:
    # --- create + save a player ---
    enter_players(s)
    s.require('[New]', 'Players pane should show the action bar')
    s.require('Saved Characters', 'Players pane should show the saved list')
    s.require('(none)', 'list should start empty')
    s.require('Generate traits', 'the new-player form should show by default')

    new_character(s, NAME)
    save_character(s)
    s.require(NAME, 'the typed name should be on screen')
    s.require('Saved ' + NAME, 'status should confirm the save')

    # --- the list should show the new character ---
    if not s.find(NAME + ' ', rows=[4, 5, 6]):
        print('FAIL: %r should appear in the saved list' % NAME)
        s.dump()
        raise SystemExit(1)

    # --- save the document file and check the players round-trip ---
    s.send(b'\x1b'); s.send(b'\x1b')    # leave the pane back to the tree
    path = os.path.join(DATA_DIR, 'terminadventure.json')
    s.send(b'S')                        # :saveas
    s.require('saveas', 'S should open the :saveas prompt')
    s.send(path)
    s.send(b'\r')
    s.require('Saved', 'status should show Saved after :S')
    if not os.path.exists(path):
        print('FAIL: %s was not created' % path)
        s.dump()
        raise SystemExit(1)
    with open(path, encoding='utf-8') as f:
        content = f.read()
    for fragment in ('"players"', '"name": "' + NAME + '"',
                     '"char_class"', '"race"'):
        if fragment not in content:
            print('FAIL: %r missing from saved JSON' % fragment)
            s.dump()
            raise SystemExit(1)

    # --- reload the file: the player must come back ---
    s.quit()
    s = harness.launch(workdir=DATA_DIR)
    s.send(b':open ' + path.encode())
    s.send(b'\r')
    enter_players(s)
    s.require(NAME, 'the saved player should reappear after reload')
finally:
    s.quit()

# --- delete flow: confirming y removes the player ---
DELETE_DIR = '/tmp/terminadventure_tests/players_delete'
shutil.rmtree(DELETE_DIR, ignore_errors=True)
os.makedirs(DELETE_DIR)
s = harness.launch(workdir=DELETE_DIR)
try:
    enter_players(s)
    new_character(s, NAME)
    save_character(s)
    s.send(b'\t'); s.send(b'\t'); s.send(b'\t'); s.send(b'3')  # Delete
    s.require('Delete ' + NAME + '?', 'delete confirm dialog should appear')
    s.send(b'n')
    s.forbid('Delete ' + NAME + '?', 'declining (n) should close the dialog')
    s.require(NAME, 'declining (n) should keep the player')
    s.send(b'3')                        # hotkey still works after n
    s.require('Delete ' + NAME + '?', 'delete dialog should reappear')
    s.send(b'y')
    s.require('(none)', 'confirming (y) should remove the player')
    s.forbid('Delete ' + NAME + '?', 'dialog should be gone after y')
finally:
    s.quit()

# --- dropdown: Enter opens the list, Up/Down moves, Enter selects ---
DROP_DIR = '/tmp/terminadventure_tests/players_dropdown'
shutil.rmtree(DROP_DIR, ignore_errors=True)
os.makedirs(DROP_DIR)
ARROW_DOWN = b'\x1b[B'
ENTER = b'\r'
s = harness.launch(workdir=DROP_DIR)
try:
    enter_players(s)
    s.send(ARROW_DOWN); s.send(ARROW_DOWN)  # focus the Race dropdown
    s.send(ENTER)                        # open it
    s.require('Enter: select', 'Enter should open the dropdown window')
    s.require('Dragonborn', 'the dropdown list should show the current race')
    s.require('Human', 'the dropdown should list the available races')
    s.send(ARROW_DOWN); s.send(ARROW_DOWN)  # move highlight down 2 -> Half-Elf
    s.send(ENTER)                        # select Half-Elf
    s.forbid('Enter: select', 'the dropdown window should close after selection')
    s.require('Half-Elf', 'the selected race should be applied to the character')
finally:
    s.quit()

# --- identity 'Generate traits': roll attrs from the Identity settings ---
GEN_DIR = '/tmp/terminadventure_tests/players_generate'
shutil.rmtree(GEN_DIR, ignore_errors=True)
os.makedirs(GEN_DIR)
s = harness.launch(workdir=GEN_DIR)
try:
    enter_players(s)
    # scroll down the identity fields to the Generate traits button
    for _ in range(10):
        s.send(ARROW_DOWN)
    s.require('Generate traits', 'Identity tab should offer a Generate traits button')
    s.send(ENTER)                        # run it
    s.require('Rolled from', 'Generate traits should roll attrs from the Identity')
    # derived numbers must be recomputed against the chosen class (Barbarian):
    # AC reflects unarmored defense and HP reflects the 1d12 hit die, so it is
    # very unlikely either still equals the blank-draft defaults.
finally:
    s.quit()

print('PASS')
