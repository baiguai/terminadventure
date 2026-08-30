#!/usr/bin/env python3
"""Test VISUAL U (uppercase) and VISUAL u (lowercase) selections."""
import harness

s = harness.launch(cols=80, rows=24)
try:
    s.send(b'a')
    s.send(b'Alpha')
    s.send(b'\r')
    s.send(b'I')
    s.send(b'hello World 123 cafe')
    s.send(b'\r')
    s.send(b'second Line here')
    s.send(b'\x1b')
    s.send(b'k')
    s.require('hello World 123 cafe', 'text entered')

    def joined():
        return "\n".join(s.screen())

    # 1) char-wise: 0 w v e e e selects 'World 123 cafe', U uppercases letters
    s.send(b'0')
    s.send(b'w')
    s.send(b'v')
    s.send(b'e')
    s.send(b'e')
    s.send(b'e')
    s.send(b'U')
    s.step(0.4)
    assert 'hello WORLD 123 CAFE' in joined(), 'U should uppercase the selection'
    assert 'hello World 123 cafe' not in joined(), 'U must not leave lowercase'
    print('ok: char-wise U uppercases in place')

    # 2) after U we are back in NORMAL mode
    assert 'NORMAL' in s.row_text(s.rows - 1), 'mode must be NORMAL after U'
    print('ok: mode returns to NORMAL after U')

    # 3) go to the tree and back into the editor (fresh NORMAL), line-wise V+U
    s.send(b'\x1b')
    s.step(0.3)
    s.send(b'i')
    s.step(0.3)
    s.send(b'j')
    s.send(b'V')
    s.send(b'U')
    s.step(0.4)
    assert 'SECOND LINE HERE' in joined(), 'line-wise U should uppercase whole line'
    print('ok: line-wise U uppercases whole line')

    # 4) multi-row: k 0 v j G selects both lines, U
    s.send(b'k')
    s.send(b'0')
    s.send(b'v')
    s.send(b'j')
    s.send(b'G')
    s.send(b'U')
    s.step(0.4)
    assert 'HELLO WORLD 123 CAFE' in joined(), 'multi-row U should uppercase line 1'
    assert 'SECOND LINE HERE' in joined(), 'multi-row U should keep line 2 uppercased'
    print('ok: multi-row U uppercases across lines')

    # 5) lowercase: $ b v e  ->  'CAFE' -> 'cafe'
    s.send(b'$')
    s.send(b'b')
    s.send(b'v')
    s.send(b'e')
    s.send(b'u')
    s.step(0.4)
    assert 'cafe' in joined(), 'u should lowercase the selection'
    assert 'CAFE' not in joined(), 'u must not leave uppercase'
    print('ok: char-wise u lowercases in place')

    # 6) digits survive both folds
    assert '123' in joined(), 'digits must be untouched'
    print('ok: digits untouched')
finally:
    s.quit()

print('PASS')
