#!/usr/bin/env python3
"""Smoke test: Enter in INSERT mode keeps the current line's indentation.

Typing on a 4-space-indented line and pressing Return starts the next line
with the same 4 spaces, so an indented block keeps its shape.
"""
import harness

s = harness.launch()
try:
    s.require('Select a node to edit', 'app must start blank')

    s.send(b'a')
    s.require('create_node', 'a should open the :create_node prompt')
    s.send(b'Indent')
    s.send(b'\r')
    s.require('Indent', 'node should be created')

    s.send(b'I')
    s.send(b'    first')
    s.send(b'\r')
    s.send(b'second')
    s.send(b'\x1b')

    s.require('    first', 'the first line should keep its leading spaces')
    s.require('    second',
              'Enter should carry the indentation to the new line')

finally:
    s.quit()

print('PASS')
