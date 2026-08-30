#!/usr/bin/env python3
"""Test VISUAL BLOCK mode (Ctrl+V): column selection, d, I, A, y, U/u, $, G."""
import harness

s = harness.launch(cols=80, rows=24)
try:
    def joined():
        return "\n".join(s.screen())

    def editor_inv(row):
        """Inverted (highlighted) editor columns on a screen row."""
        return [c for c in range(31, s.cols) if 'inv' in s.grid[row][c][1]]

    def editor_text(row):
        return s.row_text(row)[31:]

    # setup: three editor rows 'hello World', 'second Line', 'hi'
    s.send(b'a')
    s.send(b'Alpha')
    s.send(b'\r')
    s.send(b'I')
    s.send(b'hello World')
    s.send(b'\r')
    s.send(b'second Line')
    s.send(b'\r')
    s.send(b'hi')
    s.send(b'\x1b')
    s.step(0.3)
    s.send(b'gg')
    s.step(0.3)
    s.require('hello World', 'text entered')

    # 1) Ctrl+V enters VISUAL BLOCK
    s.send(b'\x16')
    s.step(0.3)
    assert 'VISUAL BLOCK' in s.row_text(s.rows - 1), 'mode must be VISUAL BLOCK'
    print('ok: Ctrl+V enters VISUAL BLOCK')

    # 2) j then l extend the block; the two editor cells are inverted
    s.send(b'j')
    s.step(0.2)
    s.send(b'l')
    s.step(0.2)
    assert editor_inv(0) == [31, 32], 'row 0 cols 0-1 highlighted'
    assert editor_inv(1) == [31, 32], 'row 1 cols 0-1 highlighted'
    print('ok: j/l extend the block highlight')

    # 3) G extends to the end of the text; h narrows back
    s.send(b'G')
    s.step(0.2)
    assert editor_inv(2) == [31, 32], 'G must extend block to last text row'
    s.send(b'h')
    s.step(0.2)
    assert editor_inv(0) == [31], 'h must narrow the block to one column'
    s.send(b'l')
    s.step(0.2)
    assert editor_inv(0) == [31, 32], 'l must widen the block again'
    print('ok: G extends, h narrows')

    # 4) yank the 2x2 block (cols 0-1, rows 0-1) and paste it
    s.send(b'y')
    s.step(0.3)
    assert 'NORMAL' in s.row_text(s.rows - 1), 'mode returns to NORMAL after yank'
    s.send(b'$')
    s.step(0.2)
    s.send(b'i')
    s.step(0.2)
    s.send(b'\x16')
    s.step(0.3)  # paste clipboard (he + newline + se)
    s.send(b'\x1b')
    s.step(0.3)
    assert editor_text(2).startswith('hihe'), 'block yank must paste its first line'
    print('ok: yank copies the block columns')

    # 5) block U uppercases only the selected rectangle
    s.send(b'gg')
    s.step(0.2)
    s.send(b'\x16')
    s.step(0.2)
    s.send(b'j')
    s.step(0.2)
    s.send(b'l')
    s.step(0.2)
    s.send(b'U')
    s.step(0.3)
    assert editor_text(0).startswith('HEllo'), 'U must uppercase the block rectangle'
    assert editor_text(1).startswith('SEcond'), 'U must uppercase the block rectangle'
    assert 'NORMAL' in s.row_text(s.rows - 1), 'mode must be NORMAL after block U'
    print('ok: block U uppercases rectangle only')

    # 6) I inserts at block start; short rows get space-padded
    s.send(b'j')
    s.step(0.2)
    s.send(b'5l')
    s.step(0.2)  # cursor (1,5), CtrlV anchors there
    s.send(b'\x16')
    s.step(0.2)
    s.send(b'j')
    s.step(0.2)  # block rows 1-2, cols 5-5
    s.send(b'I')
    s.step(0.3)
    s.send(b'<>')
    s.step(0.3)
    s.send(b'\x1b')
    s.step(0.3)
    assert editor_text(1).startswith('SEcon<>'), 'I must insert at block start'
    assert editor_text(2).startswith('hihe <>'), 'I must pad short rows with spaces'
    print('ok: I inserts at block start with space padding')

    # 7) A inserts at block end
    s.send(b'gg')
    s.step(0.2)
    s.send(b'0')
    s.step(0.2)
    s.send(b'\x16')
    s.step(0.2)
    s.send(b'j')
    s.step(0.2)
    s.send(b'l')
    s.step(0.2)
    s.send(b'A')
    s.step(0.3)
    s.send(b'!')
    s.step(0.3)
    s.send(b'\x1b')
    s.step(0.3)
    assert editor_text(0).startswith('HE!'), 'A must insert after block end'
    assert editor_text(1).startswith('SE!'), 'A must insert after block end'
    print('ok: A inserts at block end')

    # 8) d deletes the block rectangle only
    s.send(b'gg')
    s.step(0.2)
    s.send(b'\x16')
    s.step(0.2)
    s.send(b'j')
    s.step(0.2)
    s.send(b'2l')
    s.step(0.2)
    s.send(b'd')
    s.step(0.3)
    assert editor_text(0).startswith('llo World'), 'd must delete the block columns'
    assert editor_text(1).startswith('con<>d Line'), 'd must delete the block columns'
    assert editor_text(2).startswith('hihe <>'), 'd must leave rows outside the block'
    print('ok: d deletes the block rectangle')

    # 9) Esc cancels a block back to NORMAL
    s.send(b'\x16')
    s.step(0.3)
    s.send(b'\x1b')
    s.step(0.3)
    assert 'NORMAL' in s.row_text(s.rows - 1), 'Esc must cancel the block'
    print('ok: Esc cancels the block')
finally:
    s.quit()

print('PASS')
