# Tic-Tac-Toe for the Shanling Q2

A Q2 Pod ([DiamondBond/q2-pod](https://github.com/DiamondBond/q2-pod)) card game: play the Q2 at
three levels, or a friend on the same Q2. It runs from Library -> Games like Challenger Deep and
Wordle, owns the screen while it runs, and leaves the music playing.

The player's guide (install, modes, controls) is [card/README.txt](card/README.txt), copied into
the card folder as `README.txt`.

## The Q2 as a player

Played perfectly, tic-tac-toe is always a draw. So `src/board.c` gives the Q2 a full search (the
board is small) and then lets it fall short on purpose. On each move it may miss a win it has,
miss a block it needs, or slip: play a move that loses to good play. The chances per level:

| Level  | takes a win | blocks a win | slips |
|--------|-------------|--------------|-------|
| Easy   | 70%         | 45%          | 55%   |
| Normal | 100%        | 88%          | 22%   |
| Hard   | 100%        | 100%         | 14%   |

Measured by `test/board_test.c` over 2000 games each, taking turns to start:

| Level  | a perfect player wins | draws | the Q2 wins against random play |
|--------|-----------------------|-------|---------------------------------|
| Easy   | 83%                   | 17%   | 62%                             |
| Normal | 33%                   | 67%   | 87%                             |
| Hard   | 11%                   | 89%   | 87%                             |

The Q2 never beats a perfect player, but it punishes mistakes, and on every level it can be beaten.
To tune it, change `LEVEL` in `src/board.c`.

## Building

```sh
./build.sh       # build/q2-tictactoe and dist/.tictactoe (copy .tictactoe to the card's root)
test/run.sh      # host tests: the rules and the levels, then the real main() on a fake Q2 panel
```

The build needs `clang`, `ld.lld` and `python3` with Pillow. `tools/font.py` bakes Poppins
(`POPPINS_DIR`, Poppins-Bold.ttf and Poppins-Medium.ttf) into `build/fonts.c`. `tools/icon.py`
redraws the card icons. The binary is built as Challenger Deep and Wordle are, with no MIPS
sysroot: MIPS32r2 little-endian, o32, FR=1, NaN2008, glibc 2.28, interpreter
`/lib/ld-linux-mipsn8.so.1`. `sysroot/stubs/gen.py` makes a stub `libc.so.6` to link against. If
you add a libc call, add it to `gen.py` too.

The test frames land in `test/out/*.png`, with the glass's rounded corners shaded red.

## Licences

Code: MIT ([LICENSE](LICENSE)). Poppins: SIL Open Font License 1.1.
