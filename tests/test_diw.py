#!/usr/bin/env python3
"""Regression test for diw (NORMAL 'd i w' -> delete inner word), the delete
counterpart of viw. The inner word is the space-delimited word under the
cursor; deleting keeps surrounding whitespace. Also verifies the multi-key
operator does not fire single-key `i` mid-sequence and that a count prefix
deletes multiple words.
"""
import harness

s = harness.launch()
try:
    s.send(b'a')
    s.send(b'Alpha')
    s.send(b'\r')
    s.send(b'I')
    s.send(b'foo bar baz')
    s.send(b'\x1b')

    # cursor at start: diw deletes 'foo', keeps the trailing space
    s.send(b'0')
    s.send(b'diw')
    s.require(' bar baz', 'diw at start should delete exactly the first word')

    # cursor mid-word on 'bar': diw deletes exactly 'bar'
    s.send(b'l')                 # col 1 -> 'b' of bar
    s.send(b'diw')
    s.require('  baz', 'diw mid-word should delete exactly that word')

    # cursor on the last word: diw leaves the leading spaces
    s.send(b'l')                 # col 2 -> 'b' of baz
    s.send(b'diw')
    s.require('    ', 'diw on the last word leaves the spaces')

    # an all-space line: diw reports it instead of deleting the line
    s.send(b'0')
    s.send(b'diw')
    s.require('No word here', 'no word at cursor should be reported')

    # count prefix: 2diw deletes the first two words
    s.send(b'i')                 # NORMAL -> INSERT
    s.send(b'one two three')
    s.send(b'\x1b')
    s.send(b'0')
    s.send(b'2')
    s.send(b'diw')
    s.require(' three', '2diw should delete the first two words')

    # a single 'd' followed by a non-operator key stays pending-free:
    # d then l just moves right, nothing is deleted
    s.send(b'0')
    s.send(b'd')
    s.send(b'l')
    s.require(' three', 'd then l must not delete anything')
finally:
    s.quit()

print('PASS')
