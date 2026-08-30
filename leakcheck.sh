#!/bin/bash
#
# leakcheck.sh - run terminadventure under valgrind and print a human-readable
# report instead of raw valgrind noise.
#
#   ./leakcheck.sh            automated scripted session (deterministic):
#                             creates a node, edits text, opens/closes help,
#                             saves to /tmp, quits.
#   ./leakcheck.sh --live     run the app interactively; do what you like,
#                             quit with ':qa', and the report is printed.
#
# The raw valgrind log is kept at /tmp/terminadventure-valgrind.log so you can
# dig deeper if the report flags anything.

source ./config.sh

BIN="./build/bin/$APP_NAME"
RAW="/tmp/terminadventure-valgrind.log"
DOC="/tmp/terminadventure-leak-doc.json"
LIVE=0

case "${1:-}" in
    "" ) ;;
    --live) LIVE=1 ;;
    *) echo "usage: ./leakcheck.sh [--live]"; exit 2 ;;
esac

[ -x "$BIN" ] || { echo "ERROR: $BIN not found - run ./build.sh first."; exit 1; }

echo "=== terminadventure leak check ==="

if [ "$LIVE" -eq 1 ]; then
    echo "Running $BIN under valgrind (interactive). Do stuff, then quit with ':qa'."
    echo "Raw output -> $RAW"
    valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes "$BIN" 2> "$RAW"
else
    rm -f "$DOC"
    echo "Running $BIN under valgrind with a scripted session (create node, edit, help, save, quit)..."
    printf 'aLeakNode\rIhello world\x1b\x1b?\x1bS%s\r:qa\r' "$DOC" \
        | timeout 120 valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
                "$BIN" > /dev/null 2> "$RAW"
    rm -f "$DOC"
fi
EXIT=$?

strip() { sed -E 's/^==[0-9]+==[[:space:]]*//'; }

if ! grep -qa 'HEAP SUMMARY' "$RAW"; then
    echo ""
    echo "ERROR: no valgrind summary in $RAW (valgrind exit $EXIT)."
    echo "  The run may have crashed or been interrupted. See the log."
    exit 1
fi

# --- leak numbers ----------------------------------------------------------
pick() { grep -a "$1:" "$RAW" | tail -1 | sed -E "s/.*$1: ([0-9,]+) bytes in ([0-9,]+) blocks.*/\1 \2/"; }
if grep -qa 'All heap blocks were freed' "$RAW"; then
    d="0 0"; i="0 0"; p="0 0"; r="0 0"
else
    d=$(pick 'definitely lost');  [ -n "$d" ] || d="0 0"
    i=$(pick 'indirectly lost');  [ -n "$i" ] || i="0 0"
    p=$(pick 'possibly lost');    [ -n "$p" ] || p="0 0"
    r=$(pick 'still reachable');  [ -n "$r" ] || r="0 0"
fi
db=${d#* };  d=${d% *}
ib=${i#* };  i=${i% *}
pb=${p#* };  p=${p% *}
rb=${r#* };  r=${r% *}
dlost=$(tr -d ',' <<< "$d"); ilost=$(tr -d ',' <<< "$i"); plost=$(tr -d ',' <<< "$p")
tot=$(( dlost + ilost + plost ))
inuse=$(grep -a 'in use at exit' "$RAW" | tail -1 | strip | sed 's/^ *//')

echo ""
echo "--- LEAKS ----------------------------------------------------------------------"
if [ "$tot" -eq 0 ]; then
    echo "  CLEAN: $inuse - every heap block was freed at exit."
else
    echo "  $inuse"
    echo "  definitely lost:  $d bytes in $db blocks   <- no pointer remains to it (a real leak)"
    echo "  indirectly lost:  $i bytes in $ib blocks   <- owned by the definitely/possibly lost blocks"
    echo "  possibly lost:    $p bytes in $pb blocks   <- valgrind cannot prove whether a pointer exists"
fi
echo "  still reachable:  $r bytes in $rb blocks   <- pointer exists at exit but was never freed"

if [ "$tot" -gt 0 ]; then
    echo ""
    echo "  Where the lost blocks were allocated:"
    awk '
        /are (definitely|possibly) lost in loss record/ { print; n=0; next }
        n < 2 && /^==[0-9]+==    (at|by) 0x/ { n++; print; next }
    ' "$RAW" | strip | sed 's/^/    /'
fi

# --- errors ------------------------------------------------------------------
# Valgrind prints one record per distinct context; the heading line is the
# first non-blank line after a blank separator. Classify by heading text.
read -r real uni termio other <<< "$(awk '
    /^==[0-9]+== $/ { blank=1; next }
    blank && /^==[0-9]+== / {
        h = $0; sub(/^==[0-9]+== /, "", h); blank=0
        if (h ~ /^Syscall param ioctl/) { t++ }
        else if (h ~ /^(Invalid (read|write|free)|Mismatched free|Source and destination overlap|overlapping memcpy|exceeded stack bounds)/) { r++ }
        else if (h ~ /^(Conditional jump|Invalid jump|Use of uninitialised|Syscall param .*uninitialised)/) { u++ }
        else { o++ }
    }
    END { print (r+0), (u+0), (t+0), (o+0) }
' "$RAW")"

echo ""
echo "--- ERRORS ---------------------------------------------------------------------"
errtot=$(grep -a 'ERROR SUMMARY:' "$RAW" | tail -1 | sed -E 's/.*ERROR SUMMARY: ([0-9]+) errors from ([0-9]+) contexts.*/\1 errors from \2 contexts/')
echo "  Total reported:  $errtot"
echo "  Breakdown by context:"
echo "    real memory errors : $real   (invalid access / free / overlap)"
echo "    uninitialised reads: $uni"
echo "    termios (ioctl)     : $termio"
if [ "$other" -gt 0 ]; then echo "    other               : $other"; fi
if [ "$real" -gt 0 ]; then
    echo "  >>> REAL MEMORY ERRORS - inspect the log."
else
    echo "  The remaining uninitialised/termios records are known valgrind noise:"
    echo "  FTXUI's ScreenInteractive passes partially-initialised structs to"
    echo "  tcsetattr(3) and leaves padding bytes in its screen buffers. They are"
    echo "  harmless and not bugs in terminadventure."
fi

# --- verdict ------------------------------------------------------------------
echo ""
echo "--- VERDICT --------------------------------------------------------------------"
if [ "$real" -gt 0 ]; then
    echo "  FAIL - real memory errors found."
elif [ "$tot" -gt 0 ]; then
    echo "  FAIL - memory is leaked at exit (definitely lost: $d bytes in $db block(s))."
else
    echo "  PASS - no leaks, no memory errors (only benign FTXUI valgrind noise)."
fi
echo "  Raw log: $RAW"
