/* The rules and the Q2's player, on the host: wins and draws are found, the AI never makes an
 * illegal move, and at each level it can be beaten (but Hard far less than Easy). Each level plays
 * 2000 games against a perfect opponent and 2000 against one who plays at random, taking turns to
 * start. */
#include <stdio.h>
#include <string.h>

#include "tt.h"

static unsigned rs = 12345;
static unsigned rnd(void) {
    rs ^= rs << 13, rs ^= rs >> 17, rs ^= rs << 5;
    return rs;
}

static int fails;
#define CHECK(c, ...) \
    do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); ++fails; } } while (0)

static int perfect(uint8_t b[9], int me) { /* a best move for me, at random among equals */
    int best = -2, moves[9], n = 0;
    for (int i = 0; i < 9; ++i) {
        if (b[i]) continue;
        b[i] = (uint8_t)me;
        int v = ai_perfect_value(b, me, 3 - me);
        b[i] = 0;
        if (v > best) best = v, n = 0;
        if (v == best) moves[n++] = i;
    }
    return moves[rnd() % (unsigned)n];
}

static int random_move(const uint8_t b[9]) {
    int e[9], n = 0;
    for (int i = 0; i < 9; ++i)
        if (!b[i]) e[n++] = i;
    return e[rnd() % (unsigned)n];
}

/* the opponent is X; returns PX, PO or 3 */
static int play(int level, int opp_perfect, int q2_first) {
    uint8_t b[9] = { 0 };
    int turn = q2_first ? PO : PX;
    for (;;) {
        int w = board_winner(b, 0);
        if (w) return w;
        int m = turn == PO ? ai_move(b, PO, level, rnd) : opp_perfect ? perfect(b, PX) : random_move(b);
        if (m < 0 || m > 8 || b[m]) {
            printf("FAIL: illegal move %d\n", m);
            ++fails;
            return -1;
        }
        b[m] = (uint8_t)turn;
        turn = 3 - turn;
    }
}

int main(void) {
    int line[3];
    uint8_t b1[9] = { 1, 1, 1, 2, 2, 0, 0, 0, 0 };
    CHECK(board_winner(b1, line) == PX && line[0] == 0 && line[2] == 2, "top row");
    uint8_t b2[9] = { 2, 1, 1, 1, 2, 0, 1, 0, 2 };
    CHECK(board_winner(b2, line) == PO && line[0] == 0 && line[1] == 4 && line[2] == 8, "diagonal");
    uint8_t b3[9] = { 1, 2, 1, 1, 2, 2, 2, 1, 1 };
    CHECK(board_winner(b3, 0) == 3, "draw");
    uint8_t b4[9] = { 1, 2, 0, 0, 0, 0, 0, 0, 0 };
    CHECK(board_winner(b4, 0) == 0, "going");
    uint8_t e[9] = { 0 };
    CHECK(ai_perfect_value(e, PX, PX) == 0, "perfect play draws");

    const char *names[LEVELS] = { "Easy", "Normal", "Hard" };
    int beat_perfect[LEVELS];
    for (int lv = 0; lv < LEVELS; ++lv) {
        int r[2][4] = { { 0 } };
        for (int opp = 0; opp < 2; ++opp)
            for (int i = 0; i < 2000; ++i) {
                int w = play(lv, opp, i & 1);
                if (w > 0) r[opp][w]++;
            }
        printf("%-6s  vs perfect: you win %4.1f%%, Q2 wins %4.1f%%, draw %4.1f%%   "
               "vs random: you win %4.1f%%, Q2 wins %4.1f%%, draw %4.1f%%\n", names[lv],
               r[1][PX] / 20.0, r[1][PO] / 20.0, r[1][3] / 20.0, r[0][PX] / 20.0, r[0][PO] / 20.0, r[0][3] / 20.0);
        CHECK(r[1][PO] == 0, "%s: beat a perfect player", names[lv]);
        beat_perfect[lv] = r[1][PX];
    }
    CHECK(beat_perfect[HARD] > 100, "Hard should lose to good play sometimes (%d of 2000)", beat_perfect[HARD]);
    CHECK(beat_perfect[HARD] < 1000, "Hard should mostly hold (%d of 2000 lost)", beat_perfect[HARD]);
    CHECK(beat_perfect[EASY] > beat_perfect[NORMAL] && beat_perfect[NORMAL] > beat_perfect[HARD], "levels in order");
    printf(fails ? "board: %d failed\n" : "board: ok\n", fails);
    return fails != 0;
}
