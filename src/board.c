/* The rules, and the Q2 as a player.
 *
 * Played perfectly, tic-tac-toe is always a draw, which is no fun. So the Q2 knows the perfect move
 * (a full search: the board is small) but doesn't always take it. At each level it may miss a win,
 * miss a block, or make a slip: a move that, played well, you can beat. Easy slips often, Normal
 * sometimes, Hard seldom, so even on Hard a sharp player wins now and then. */
#include "tt.h"

static const int8_t LINES[8][3] = {
    { 0, 1, 2 }, { 3, 4, 5 }, { 6, 7, 8 }, { 0, 3, 6 }, { 1, 4, 7 }, { 2, 5, 8 }, { 0, 4, 8 }, { 2, 4, 6 },
};

int board_winner(const uint8_t b[9], int line[3]) {
    for (int i = 0; i < 8; ++i) {
        int a = LINES[i][0], m = LINES[i][1], z = LINES[i][2];
        if (b[a] && b[a] == b[m] && b[a] == b[z]) {
            if (line) line[0] = a, line[1] = m, line[2] = z;
            return b[a];
        }
    }
    for (int i = 0; i < 9; ++i)
        if (!b[i]) return 0;
    return 3;
}

/* the value of the position for `me` with `turn` to move: +1, 0 or -1, both sides perfect */
int ai_perfect_value(uint8_t b[9], int me, int turn) {
    int w = board_winner(b, 0);
    if (w == 3) return 0;
    if (w) return w == me ? 1 : -1;
    int best = turn == me ? -2 : 2;
    for (int i = 0; i < 9; ++i) {
        if (b[i]) continue;
        b[i] = (uint8_t)turn;
        int v = ai_perfect_value(b, me, 3 - turn);
        b[i] = EMPTY;
        if (turn == me ? v > best : v < best) best = v;
        if (best == (turn == me ? 1 : -1)) break; /* can't do better */
    }
    return best;
}

/* cells where `who` would complete a line */
static int wins_for(const uint8_t b[9], int who, int out[9]) {
    int n = 0;
    for (int i = 0; i < 9; ++i) {
        if (b[i]) continue;
        for (int l = 0; l < 8; ++l) {
            const int8_t *L = LINES[l];
            if (L[0] != i && L[1] != i && L[2] != i) continue;
            int mine = 0;
            for (int k = 0; k < 3; ++k) mine += L[k] != i && b[L[k]] == who;
            if (mine == 2) {
                out[n++] = i;
                break;
            }
        }
    }
    return n;
}

static int in(const int *set, int n, int v) {
    for (int i = 0; i < n; ++i)
        if (set[i] == v) return 1;
    return 0;
}

/* per level, in percent: take a win, block a win, slip (a move that loses to good play) */
static const struct {
    int win, block, slip;
} LEVEL[LEVELS] = {
    { 70, 45, 55 },  /* Easy */
    { 100, 88, 22 }, /* Normal */
    { 100, 100, 14 }, /* Hard */
};

int ai_move(const uint8_t board[9], int me, int level, unsigned (*rand)(void)) {
    uint8_t b[9];
    int empty[9], ne = 0;
    for (int i = 0; i < 9; ++i) {
        b[i] = board[i];
        if (!b[i]) empty[ne++] = i;
    }
    if (!ne) return -1;
    if (level < 0 || level >= LEVELS) level = NORMAL;
#define ROLL(pct) ((int)(rand() % 100) < (pct))
#define PICK(set, n) (set)[rand() % (unsigned)(n)]
    if (ne == 9) { /* the first move: anywhere on Easy, a corner or the centre (mostly) above */
        static const int good[5] = { 0, 2, 4, 6, 8 };
        return level == EASY || ROLL(10) ? (int)(rand() % 9) : PICK(good, 5);
    }

    int opp = 3 - me, win[9], nw = wins_for(b, me, win), block[9], nb = wins_for(b, opp, block);
    int rest[9], nr;
    if (nw) {
        if (ROLL(LEVEL[level].win)) return PICK(win, nw);
        nr = 0; /* missed it: anything but a win */
        for (int i = 0; i < ne; ++i)
            if (!in(win, nw, empty[i])) rest[nr++] = empty[i];
        return nr ? PICK(rest, nr) : PICK(win, nw);
    }
    if (nb) {
        if (ROLL(LEVEL[level].block)) return PICK(block, nb);
        nr = 0; /* didn't see it coming */
        for (int i = 0; i < ne; ++i)
            if (!in(block, nb, empty[i])) rest[nr++] = empty[i];
        return nr ? PICK(rest, nr) : PICK(block, nb);
    }

    /* the rest: the search's best moves, or now and then a slip */
    int value[9], best = -2;
    for (int i = 0; i < ne; ++i) {
        b[empty[i]] = (uint8_t)me;
        value[i] = ai_perfect_value(b, me, opp);
        b[empty[i]] = EMPTY;
        if (value[i] > best) best = value[i];
    }
    int top[9], nt = 0, worse[9], nwr = 0;
    for (int i = 0; i < ne; ++i)
        if (value[i] == best) top[nt++] = empty[i];
        else worse[nwr++] = empty[i];
    if (nwr && ROLL(LEVEL[level].slip)) return PICK(worse, nwr);
    return PICK(top, nt);
#undef ROLL
#undef PICK
}
