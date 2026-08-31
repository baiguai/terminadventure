#!/usr/bin/env python3
"""Bookmarks dialog: 'D' removes the selected bookmark."""
import json
import os

import harness

s = harness.launch(cols=120, rows=24)
try:
    s.send(b'a')
    s.send(b'One')
    s.send(b'\r')
    s.send(b'a')
    s.send(b'Two')
    s.send(b'\r')

    # m bookmarks the active node (the last one created)
    s.send(b'm')
    s.require('Bookmarked: Two', 'bookmark should be set')

    # open the bookmarks dialog (backtick)
    s.send(b'`')
    s.require('` Bookmarks', 'bookmarks dialog should open')
    s.require('Two', 'bookmark should be listed')

    # 'D' removes it
    s.send(b'D')
    s.require('No bookmarks', 'D should empty the list')
    s.require('Bookmark removed', 'status should report the removal')

    # close the dialog, then save: an empty bookmarks array is written
    s.send(b'\x1b')
    doc_path = os.path.join(s.workdir, 'doc.json')
    s.send((':saveas %s\r' % doc_path).encode())
    s.require('Saved', 'doc should save')
    with open(doc_path, encoding='utf-8') as f:
        data = json.load(f)
    if data.get('bookmarks'):
        print('FAIL: bookmark still present after D')
        s.dump()
        raise SystemExit(1)
    print('ok: saved JSON has no bookmarks')
finally:
    s.quit()

print('PASS')
