#!/usr/bin/env python3
"""Feature test for :command Tab completion: command names, relative paths,
nested directories (trailing separator), and the "Matches:" status line.

Permanent regression for CollectPathMatches / CompleteCommand in src/op/op.cpp
"""
import os

import harness

s = harness.launch()
try:
    # A small fixture tree inside the test workdir (the app's cwd).
    for rel in ('final.json', 'alpha.txt', 'subdir/beta.txt'):
        path = os.path.join(s.workdir, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(rel)

    # command-name completion: `:ope` + Tab -> `:open `
    s.send(b':ope')
    s.send(b'\t')
    s.require(':open ', 'Tab should complete :ope to :open ')

    # relative filename completion: `:open fin` + Tab -> `:open final.json`
    s.send(b'\x1b')                  # Esc closes the command line
    s.send(b':open fin')
    s.send(b'\t')
    s.require(':open final.json',
              'Tab should complete a relative filename')

    # absolute path from the root: `:o /tm` + Tab -> `:o /tmp/`
    s.send(b'\x1b')
    s.send(b':o /tm')
    s.send(b'\t')
    s.require(':o /tmp/',
              'Tab should not double the root separator (/tm -> /tmp/)')

    # directory completion: `:open subd` + Tab -> `:open subdir/`
    s.send(b'\x1b')
    s.send(b':open subd')
    s.send(b'\t')
    s.require(':open subdir/', 'Tab should complete the dir and append /')

    # completing inside the dir: `:open subdir/` + Tab -> `subdir/beta.txt`
    s.send(b'\x1b')
    s.send(b':open subdir/')
    s.send(b'\t')
    s.require(':open subdir/beta.txt',
              'Tab should complete the file inside subdir')

    # Tab with an empty path lists every candidate in the status bar
    s.send(b'\x1b')
    s.send(b':open ')
    s.send(b'\t')
    s.require('Matches: alpha.txt final.json subdir/',
              'Tab on an empty path should list the matches')

    # a partial path with no match reports it
    s.send(b'\x1b')
    s.send(b':open nope')
    s.send(b'\t')
    s.require('No match for: nope', 'a missing file should be reported')
finally:
    s.quit()

print('PASS')
