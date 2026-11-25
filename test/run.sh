#!/bin/sh
# Host tests for q2-tictactoe (x86-64 Linux, clang, python3 with Pillow): the rules and the Q2's
# levels, then the real game and platform layer on fbshim.so's fake /dev/fb0, driven through Q2 Pod's
# input socket (test/drive.py). Each scenario must end on TERM with status 0, the panel back on Q2
# Pod's page and the socket gone. Frames land in test/out/*.png.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host test/out
python3 tools/font.py build/fonts.c
[ build/art.c -nt tools/art.py ] || python3 tools/art.py build/art.c
clang -O2 -Wall -Wextra -Werror -Isrc test/board_test.c src/board.c -o build/host/board_test
./build/host/board_test
clang -O2 -g -fno-math-errno -Wall -Wextra -Wno-unused-parameter -Werror -Isrc src/*.c build/fonts.c build/art.c -o build/host/q2-tictactoe
clang -shared -fPIC -O1 -w test/fbshim.c -o build/host/fbshim.so -ldl
for sc in menu two touch q2 quit quit_tap; do
    python3 -I test/drive.py build/host test/out $sc > test/out/$sc.log 2>&1 || { cat test/out/$sc.log; exit 1; }
    grep -q "^exit status: 0" test/out/$sc.log || { echo "FAIL: $sc: exit status"; cat test/out/$sc.log; exit 1; }
    grep -q "^panel: yoffset 0 " test/out/$sc.log || { echo "FAIL: $sc: panel not handed back"; exit 1; }
    grep -q "^socket left: False" test/out/$sc.log || { echo "FAIL: $sc: socket left behind"; exit 1; }
done
res() { grep "^result: $2:" test/out/$1.log | sed 's/.*: //'; }
fail() { echo "FAIL: $*"; exit 1; }
[ "$(res menu sel_level)" = 2 ] && [ "$(res menu level_changed)" = 1 ] && [ "$(res menu sel_two)" = 1 ] || fail "menu: wheel and level"
[ "$(res menu screen)" = 1 ] && [ "$(res menu after_return)" = 0 ] || fail "menu: start a game, Return (held) back to the menu"
[ "$(res two mode)" = 1 ] && [ "$(res two over)" = 1 ] && [ "$(res two x_wins)" = 1 ] || fail "two: X's diagonal by wheel and Centre"
[ "$(res two next_turn)" = 2 ] && [ "$(res two next_empty)" = 0 ] || fail "two: O starts round two on an empty board"
[ "$(res touch over)" = 3 ] && [ "$(res touch draws)" = 1 ] && [ "$(res touch new_round)" = 0 ] || fail "touch: a draw by taps, tap for the next"
[ "$(res q2 mode)" = 0 ] && [ "$(res q2 rounds)" = 4 ] && [ "$(res q2 tally)" = 4 ] && [ "$(res q2 q2_moves)" -ge 6 ] || fail "q2: four rounds against the Q2"
[ "$(res quit sel)" = 3 ] && [ "$(res quit ended_itself)" = 1 ] && [ "$(res quit_tap ended_itself)" = 1 ] || fail "quit: the menu's Quit, by Centre and by a tap"
grep -q "^save: level 1 | record 0 0 0 0 | record 1 " test/out/q2.log || fail "q2: record not saved"
for f in test/out/*.raw; do python3 -I test/view.py "$f" "${f%.raw}.png"; done
echo "game: ok (menu, two players by wheel and by touch, four rounds against the Q2, saved record, Quit by Centre and by touch)"
